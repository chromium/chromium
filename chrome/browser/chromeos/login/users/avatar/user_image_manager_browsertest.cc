// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_test_util.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "crypto/rsa_private_key.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// Because policy is not needed in some tests it is better to use e-mails that
// are definitely not enterprise. This lets us to avoid faking of policy fetch
// procedure.
constexpr char kTestUser1[] = "test-user@gmail.com";
constexpr char kTestUser1GaiaId[] = "1111111111";
constexpr char kTestUser2[] = "test-user2@gmail.com";
constexpr char kTestUser2GaiaId[] = "2222222222";

constexpr char kRandomTokenStrForTesting[] = "random-token-str-for-testing";

policy::CloudPolicyStore* GetStoreForUser(const user_manager::User* user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  if (!profile) {
    ADD_FAILURE();
    return NULL;
  }
  policy::UserCloudPolicyManagerChromeOS* policy_manager =
      policy::UserPolicyManagerFactoryChromeOS::GetCloudPolicyManagerForProfile(
          profile);
  if (!policy_manager) {
    ADD_FAILURE();
    return NULL;
  }
  return policy_manager->core()->store();
}

class UserImageChangeWaiter : public user_manager::UserManager::Observer {
 public:
  UserImageChangeWaiter() {}
  ~UserImageChangeWaiter() override {}

  void Wait() {
    user_manager::UserManager::Get()->AddObserver(this);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    user_manager::UserManager::Get()->RemoveObserver(this);
  }

  // user_manager::UserManager::Observer:
  void OnUserImageChanged(const user_manager::User& user) override {
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(UserImageChangeWaiter);
};

}  // namespace

class UserImageManagerTest : public LoginManagerTest,
                             public user_manager::UserManager::Observer {
 public:
  std::unique_ptr<net::test_server::BasicHttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/avatar.jpg")
      return nullptr;

    // Check whether the token string is the same.
    EXPECT_TRUE(request.headers.find(net::HttpRequestHeaders::kAuthorization) !=
                request.headers.end());
    const std::string authorization_header =
        request.headers.at(net::HttpRequestHeaders::kAuthorization);
    const size_t pos = authorization_header.find(" ");
    EXPECT_TRUE(pos != std::string::npos);
    const std::string token = authorization_header.substr(pos + 1);
    EXPECT_TRUE(token == kRandomTokenStrForTesting);

    std::string profile_image_data;
    base::FilePath test_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    EXPECT_TRUE(
        ReadFileToString(test_data_dir.Append("chromeos").Append("avatar1.jpg"),
                         &profile_image_data));
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("image/jpeg");
    response->set_code(net::HTTP_OK);
    response->set_content(profile_image_data);
    return response;
  }

 protected:
  UserImageManagerTest() : LoginManagerTest(true, true) {}

  // LoginManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // These tests create new users and then inject policy after the fact,
    // to avoid having to set up a mock policy server. UserCloudPolicyManager
    // will shut down the profile if there's an error loading the initial
    // policy, so disable this behavior so we can inject policy directly.
    command_line->AppendSwitch(
        chromeos::switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpOnMainThread() override {
    // Set up the test server.
    controllable_http_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/avatar.jpg",
            true /*relative_url_is_prefix*/);
    ASSERT_TRUE(embedded_test_server()->Started());

    LoginManagerTest::SetUpOnMainThread();
    local_state_ = g_browser_process->local_state();
    user_manager::UserManager::Get()->AddObserver(this);

    // FakeGaia authorizes requests for profile info.
    FakeGaia::AccessTokenInfo token_info;
    token_info.any_scope = true;
    token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
    token_info.token = kRandomTokenStrForTesting;
    token_info.email = test_account_id1_.GetUserEmail();
    fake_gaia_.IssueOAuthToken(kRandomTokenStrForTesting, token_info);
    fake_gaia_.MapEmailToGaiaId(kTestUser1, kTestUser1GaiaId);
  }

  void TearDownOnMainThread() override {
    user_manager::UserManager::Get()->RemoveObserver(this);
    LoginManagerTest::TearDownOnMainThread();
  }

  // UserManager::Observer overrides:
  void LocalStateChanged(user_manager::UserManager* user_manager) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  // Logs in |account_id|.
  void LogIn(const AccountId& account_id) {
    user_manager::UserManager::Get()->UserLoggedIn(
        account_id, account_id.GetUserEmail(), false /* browser_restart */,
        false /* is_child */);
  }

  // Verifies user image info.
  void ExpectUserImageInfo(const AccountId& account_id,
                           int image_index,
                           const base::FilePath& image_path) {
    const base::DictionaryValue* images_pref =
        local_state_->GetDictionary(UserImageManagerImpl::kUserImageProperties);
    ASSERT_TRUE(images_pref);
    const base::DictionaryValue* image_properties = NULL;
    images_pref->GetDictionaryWithoutPathExpansion(account_id.GetUserEmail(),
                                                   &image_properties);
    ASSERT_TRUE(image_properties);
    int actual_image_index;
    std::string actual_image_path;
    ASSERT_TRUE(
        image_properties->GetInteger(UserImageManagerImpl::kImageIndexNodeName,
                                     &actual_image_index) &&
        image_properties->GetString(UserImageManagerImpl::kImagePathNodeName,
                                    &actual_image_path));
    EXPECT_EQ(image_index, actual_image_index);
    EXPECT_EQ(image_path.value(), actual_image_path);
  }

  // Verifies that there is no image info for |account_id| in dictionary
  // |images_pref|.
  void ExpectNoUserImageInfo(const base::DictionaryValue* images_pref,
                             const AccountId& account_id) {
    ASSERT_TRUE(images_pref);
    const base::DictionaryValue* image_properties = NULL;
    images_pref->GetDictionaryWithoutPathExpansion(account_id.GetUserEmail(),
                                                   &image_properties);
    ASSERT_FALSE(image_properties);
  }

  // Returns the image path for user |account_id| with specified |extension|.
  base::FilePath GetUserImagePath(const AccountId& account_id,
                                  const std::string& extension) {
    return user_data_dir_.Append(account_id.GetUserEmail())
        .AddExtension(extension);
  }

  // Seeds the AccountTrackerService with test data so the ProfileDownloader can
  // retrieve the picture URL and fetch the image.
  void SeedAccountTrackerService(const AccountId& account_id,
                                 Profile* profile) {
    AccountInfo info;
    info.account_id = std::string();
    info.gaia = account_id.GetUserEmail();
    info.email = account_id.GetUserEmail();
    info.full_name = account_id.GetUserEmail();
    info.given_name = account_id.GetUserEmail();
    info.hosted_domain = AccountTrackerService::kNoHostedDomainFound;
    info.locale = account_id.GetUserEmail();
    info.picture_url = embedded_test_server()->GetURL("/avatar.jpg").spec();
    info.is_child_account = false;

    AccountTrackerServiceFactory::GetForProfile(profile)->SeedAccountInfo(info);
  }

  // Completes the download of the currently logged-in user's profile image.
  // This method must only be called after a profile data download including
  // the profile image has been started.
  void CompleteProfileImageDownload() {
    controllable_http_response_->WaitForRequest();
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        HandleRequest(*controllable_http_response_->http_request());
    controllable_http_response_->Send(response->ToResponseString());
    controllable_http_response_->Done();

    base::RunLoop run_loop;
    PrefChangeRegistrar pref_change_registrar;
    pref_change_registrar.Init(local_state_);
    pref_change_registrar.Add("UserDisplayName", run_loop.QuitClosure());
    run_loop.Run();

    const user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    ASSERT_TRUE(user);
    UserImageManagerImpl* uim = reinterpret_cast<UserImageManagerImpl*>(
        ChromeUserManager::Get()->GetUserImageManager(user->GetAccountId()));
    if (uim->job_.get()) {
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
    }
  }

  base::FilePath test_data_dir_;
  base::FilePath user_data_dir_;

  PrefService* local_state_;

  std::unique_ptr<gfx::ImageSkia> decoded_image_;

  std::unique_ptr<base::RunLoop> run_loop_;

  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_http_response_;

  const AccountId test_account_id1_ =
      AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId);
  const AccountId test_account_id2_ =
      AccountId::FromUserEmailGaiaId(kTestUser2, kTestUser2GaiaId);
  const AccountId enterprise_account_id_ =
      AccountId::FromUserEmailGaiaId(kEnterpriseUser1, kEnterpriseUser1GaiaId);
  const cryptohome::AccountIdentifier cryptohome_id_ =
      cryptohome::CreateAccountIdentifierFromAccountId(enterprise_account_id_);

 private:
  DISALLOW_COPY_AND_ASSIGN(UserImageManagerTest);
};

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveAndLoadUserImage) {
  RegisterUser(test_account_id1_);

  // Setup a user with JPEG image.
  run_loop_.reset(new base::RunLoop);
  const gfx::ImageSkia& image = default_user_image::GetDefaultImage(
      default_user_image::kFirstDefaultImageIndex);
  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(test_account_id1_);
  user_image_manager->SaveUserImage(user_manager::UserImage::CreateAndEncode(
      image, user_manager::UserImage::FORMAT_JPEG));
  run_loop_->Run();
}

// Ensures that the user image in JPEG format is loaded correctly.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveAndLoadUserImage) {
  user_manager::UserManager::Get()->GetUsers();  // Load users.
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);
  // Wait for image load.
  if (user->image_index() == user_manager::User::USER_IMAGE_INVALID)
    UserImageChangeWaiter().Wait();
  // The image should be in the safe format.
  EXPECT_TRUE(user->image_is_safe_format());
  // Check image dimensions. Images can't be compared since JPEG is lossy.
  const gfx::ImageSkia& saved_image = default_user_image::GetDefaultImage(
      default_user_image::kFirstDefaultImageIndex);
  EXPECT_EQ(saved_image.width(), user->GetImage().width());
  EXPECT_EQ(saved_image.height(), user->GetImage().height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveUserDefaultImageIndex) {
  RegisterUser(test_account_id1_);
}

// Verifies that SaveUserDefaultImageIndex() correctly sets and persists the
// chosen user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserDefaultImageIndex) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);

  const gfx::ImageSkia& default_image = default_user_image::GetDefaultImage(
      default_user_image::kFirstDefaultImageIndex);

  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(test_account_id1_);
  user_image_manager->SaveUserDefaultImageIndex(
      default_user_image::kFirstDefaultImageIndex);

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(default_user_image::kFirstDefaultImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_,
                      default_user_image::kFirstDefaultImageIndex,
                      base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveUserImage) {
  RegisterUser(test_account_id1_);
}

// Verifies that SaveUserImage() correctly sets and persists the chosen user
// image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImage) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);

  SkBitmap custom_image_bitmap;
  custom_image_bitmap.allocN32Pixels(10, 10);
  custom_image_bitmap.eraseColor(SK_ColorWHITE);
  custom_image_bitmap.setImmutable();
  const gfx::ImageSkia custom_image =
      gfx::ImageSkia::CreateFrom1xBitmap(custom_image_bitmap);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(test_account_id1_);
  user_image_manager->SaveUserImage(user_manager::UserImage::CreateAndEncode(
      custom_image, user_manager::UserImage::FORMAT_JPEG));
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(custom_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_,
                      user_manager::User::USER_IMAGE_EXTERNAL,
                      GetUserImagePath(test_account_id1_, "jpg"));

  const std::unique_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(test_account_id1_, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(custom_image.width(), saved_image->width());
  EXPECT_EQ(custom_image.height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveUserImageFromFile) {
  RegisterUser(test_account_id1_);
}

// Verifies that SaveUserImageFromFile() correctly sets and persists the chosen
// user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImageFromFile) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);

  const base::FilePath custom_image_path =
      test_data_dir_.Append(test::kUserAvatarImage1RelativePath);
  const std::unique_ptr<gfx::ImageSkia> custom_image =
      test::ImageLoader(custom_image_path).Load();
  ASSERT_TRUE(custom_image);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(test_account_id1_);
  user_image_manager->SaveUserImageFromFile(custom_image_path);
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*custom_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_,
                      user_manager::User::USER_IMAGE_EXTERNAL,
                      GetUserImagePath(test_account_id1_, "jpg"));

  const std::unique_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(test_account_id1_, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(custom_image->width(), saved_image->width());
  EXPECT_EQ(custom_image->height(), saved_image->height());

  // Replace the user image with a PNG file with transparent pixels.
  const base::FilePath transparent_image_path =
      test_data_dir_.Append(test::kUserAvatarImage3RelativePath);
  const std::unique_ptr<gfx::ImageSkia> transparent_image =
      test::ImageLoader(transparent_image_path).Load();
  ASSERT_TRUE(transparent_image);
  // This image should have transparent pixels (i.e. not opaque).
  EXPECT_FALSE(SkBitmap::ComputeIsOpaque(*transparent_image->bitmap()));

  run_loop_.reset(new base::RunLoop);
  user_image_manager->SaveUserImageFromFile(transparent_image_path);
  run_loop_->Run();

  EXPECT_TRUE(test::AreImagesEqual(*transparent_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_,
                      user_manager::User::USER_IMAGE_EXTERNAL,
                      GetUserImagePath(test_account_id1_, "png"));

  const std::unique_ptr<gfx::ImageSkia> new_saved_image =
      test::ImageLoader(GetUserImagePath(test_account_id1_, "png")).Load();
  ASSERT_TRUE(new_saved_image);

  // The saved image should have transparent pixels (i.e. not opaque).
  EXPECT_FALSE(SkBitmap::ComputeIsOpaque(*new_saved_image->bitmap()));

  // The old user image file in JPEG should be deleted. Only the PNG version
  // should stay.
  EXPECT_FALSE(base::PathExists(GetUserImagePath(test_account_id1_, "jpg")));
  EXPECT_TRUE(base::PathExists(GetUserImagePath(test_account_id1_, "png")));
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest,
                       PRE_SaveUserImageFromProfileImage) {
  RegisterUser(test_account_id1_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that SaveUserImageFromProfileImage() correctly downloads, sets and
// persists the chosen user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImageFromProfileImage) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);

  UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting();
  LoginUser(test_account_id1_);
  Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->UpdateCredentials(
      test_account_id1_.GetUserEmail(), kRandomTokenStrForTesting);
  SeedAccountTrackerService(test_account_id1_, profile);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(test_account_id1_);
  user_image_manager->SaveUserImageFromProfileImage();
  run_loop_->Run();

  CompleteProfileImageDownload();

  const gfx::ImageSkia& profile_image =
      user_image_manager->DownloadedProfileImage();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(user_manager::User::USER_IMAGE_PROFILE, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(profile_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_, user_manager::User::USER_IMAGE_PROFILE,
                      GetUserImagePath(test_account_id1_, "jpg"));

  const std::unique_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(test_account_id1_, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(profile_image.width(), saved_image->width());
  EXPECT_EQ(profile_image.height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest,
                       PRE_ProfileImageDownloadDoesNotClobber) {
  RegisterUser(test_account_id1_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Sets the user image to the profile image, then sets it to one of the default
// images while the profile image download is still in progress. Verifies that
// when the download completes, the profile image is ignored and does not
// clobber the default image chosen in the meantime.
// TODO(crbug.com/888784) disabled due to flaky timeouts.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest,
                       DISABLED_ProfileImageDownloadDoesNotClobber) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);

  const gfx::ImageSkia& default_image = default_user_image::GetDefaultImage(
      default_user_image::kFirstDefaultImageIndex);

  UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting();
  LoginUser(test_account_id1_);
  Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->UpdateCredentials(
      test_account_id1_.GetUserEmail(), kRandomTokenStrForTesting);
  SeedAccountTrackerService(test_account_id1_, profile);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(test_account_id1_);
  user_image_manager->SaveUserImageFromProfileImage();
  run_loop_->Run();

  user_image_manager->SaveUserDefaultImageIndex(
      default_user_image::kFirstDefaultImageIndex);

  CompleteProfileImageDownload();

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(default_user_image::kFirstDefaultImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_,
                      default_user_image::kFirstDefaultImageIndex,
                      base::FilePath());
}

class UserImageManagerPolicyTest : public UserImageManagerTest,
                                   public policy::CloudPolicyStore::Observer {
 protected:
  UserImageManagerPolicyTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()),
        fake_session_manager_client_(new chromeos::FakeSessionManagerClient) {}

  // UserImageManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    device_policy_.Build();
    OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    fake_session_manager_client_->set_device_policy(device_policy_.GetBlob());
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(fake_session_manager_client_));

    UserImageManagerTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    UserImageManagerTest::SetUpOnMainThread();

    base::FilePath user_keys_dir;
    ASSERT_TRUE(
        base::PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &user_keys_dir));
    const std::string sanitized_username =
        chromeos::CryptohomeClient::GetStubSanitizedUsername(cryptohome_id_);
    const base::FilePath user_key_file =
        user_keys_dir.AppendASCII(sanitized_username).AppendASCII("policy.pub");
    std::vector<uint8_t> user_key_bits;
    ASSERT_TRUE(user_policy_.GetSigningKey()->ExportPublicKey(&user_key_bits));
    ASSERT_TRUE(base::CreateDirectory(user_key_file.DirName()));
    ASSERT_EQ(
        base::WriteFile(user_key_file,
                        reinterpret_cast<const char*>(user_key_bits.data()),
                        user_key_bits.size()),
        static_cast<int>(user_key_bits.size()));
    user_policy_.policy_data().set_username(
        enterprise_account_id_.GetUserEmail());
    user_policy_.policy_data().set_gaia_id(enterprise_account_id_.GetGaiaId());

    policy_image_ = test::ImageLoader(test_data_dir_.Append(
                                          test::kUserAvatarImage2RelativePath))
                        .Load();
    ASSERT_TRUE(policy_image_);
  }

  // policy::CloudPolicyStore::Observer overrides:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  void OnStoreError(policy::CloudPolicyStore* store) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  std::string ConstructPolicy(const std::string& relative_path) {
    std::string image_data;
    if (!base::ReadFileToString(test_data_dir_.Append(relative_path),
                                &image_data)) {
      ADD_FAILURE();
    }
    std::string policy;
    base::JSONWriter::Write(*policy::test::ConstructExternalDataReference(
                                embedded_test_server()
                                    ->GetURL(std::string("/") + relative_path)
                                    .spec(),
                                image_data),
                            &policy);
    return policy;
  }

  ScopedStubInstallAttributes test_install_attributes_{
      StubInstallAttributes::CreateCloudManaged("fake-domain", "fake-id")};
  policy::UserPolicyBuilder user_policy_;
  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  FakeSessionManagerClient* fake_session_manager_client_;

  std::unique_ptr<gfx::ImageSkia> policy_image_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserImageManagerPolicyTest);
};

IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, PRE_SetAndClear) {
  RegisterUser(enterprise_account_id_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that the user image can be set through policy. Also verifies that
// after the policy has been cleared, the user is able to choose a different
// image.
// http://crbug.com/396352
IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, DISABLED_SetAndClear) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(enterprise_account_id_);
  ASSERT_TRUE(user);

  LoginUser(enterprise_account_id_);
  base::RunLoop().RunUntilIdle();

  policy::CloudPolicyStore* store = GetStoreForUser(user);
  ASSERT_TRUE(store);

  // Set policy. Verify that the policy-provided user image is downloaded, set
  // and persisted.
  user_policy_.payload().mutable_useravatarimage()->set_value(
      ConstructPolicy(test::kUserAvatarImage2RelativePath));
  user_policy_.Build();
  fake_session_manager_client_->set_user_policy(cryptohome_id_,
                                                user_policy_.GetBlob());
  run_loop_.reset(new base::RunLoop);
  store->Load();
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*policy_image_, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_,
                      user_manager::User::USER_IMAGE_EXTERNAL,
                      GetUserImagePath(enterprise_account_id_, "jpg"));

  std::unique_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(enterprise_account_id_, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_->width(), saved_image->width());
  EXPECT_EQ(policy_image_->height(), saved_image->height());

  // Clear policy. Verify that the user image switches to a random default
  // image.
  user_policy_.payload().Clear();
  user_policy_.Build();
  fake_session_manager_client_->set_user_policy(cryptohome_id_,
                                                user_policy_.GetBlob());
  run_loop_.reset(new base::RunLoop);
  store->AddObserver(this);
  store->Load();
  run_loop_->Run();
  store->RemoveObserver(this);
  base::RunLoop().RunUntilIdle();

  const int default_image_index = user->image_index();
  EXPECT_TRUE(user->HasDefaultImage());
  ASSERT_LE(default_user_image::kFirstDefaultImageIndex, default_image_index);
  ASSERT_GT(default_user_image::kFirstDefaultImageIndex +
                default_user_image::kDefaultImagesCount,
            default_image_index);
  const gfx::ImageSkia& default_image =
      default_user_image::GetDefaultImage(default_image_index);
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_, default_image_index,
                      base::FilePath());

  // Choose a different user image. Verify that the chosen user image is set and
  // persisted.
  const int user_image_index =
      default_user_image::kFirstDefaultImageIndex +
      (default_image_index - default_user_image::kFirstDefaultImageIndex + 1) %
          default_user_image::kDefaultImagesCount;
  const gfx::ImageSkia& user_image =
      default_user_image::GetDefaultImage(user_image_index);

  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(enterprise_account_id_);
  user_image_manager->SaveUserDefaultImageIndex(user_image_index);

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(user_image_index, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(user_image, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_, user_image_index,
                      base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, PRE_PolicyOverridesUser) {
  RegisterUser(enterprise_account_id_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that when the user chooses a user image and a different image is
// then set through policy, the policy takes precedence, overriding the
// previously chosen image.
IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, PolicyOverridesUser) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(enterprise_account_id_);
  ASSERT_TRUE(user);

  LoginUser(enterprise_account_id_);
  base::RunLoop().RunUntilIdle();

  policy::CloudPolicyStore* store = GetStoreForUser(user);
  ASSERT_TRUE(store);

  // Choose a user image. Verify that the chosen user image is set and
  // persisted.
  const gfx::ImageSkia& default_image = default_user_image::GetDefaultImage(
      default_user_image::kFirstDefaultImageIndex);

  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(enterprise_account_id_);
  user_image_manager->SaveUserDefaultImageIndex(
      default_user_image::kFirstDefaultImageIndex);

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(default_user_image::kFirstDefaultImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_,
                      default_user_image::kFirstDefaultImageIndex,
                      base::FilePath());

  // Set policy. Verify that the policy-provided user image is downloaded, set
  // and persisted, overriding the previously set image.
  user_policy_.payload().mutable_useravatarimage()->set_value(
      ConstructPolicy(test::kUserAvatarImage2RelativePath));
  user_policy_.Build();
  fake_session_manager_client_->set_user_policy(cryptohome_id_,
                                                user_policy_.GetBlob());
  run_loop_.reset(new base::RunLoop);
  store->Load();
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*policy_image_, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_,
                      user_manager::User::USER_IMAGE_EXTERNAL,
                      GetUserImagePath(enterprise_account_id_, "jpg"));

  std::unique_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(enterprise_account_id_, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_->width(), saved_image->width());
  EXPECT_EQ(policy_image_->height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest,
                       PRE_UserDoesNotOverridePolicy) {
  RegisterUser(enterprise_account_id_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that when the user image has been set through policy and the user
// chooses a different image, the policy takes precedence, preventing the user
// from overriding the previously chosen image.
IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, UserDoesNotOverridePolicy) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(enterprise_account_id_);
  ASSERT_TRUE(user);

  LoginUser(enterprise_account_id_);
  base::RunLoop().RunUntilIdle();

  policy::CloudPolicyStore* store = GetStoreForUser(user);
  ASSERT_TRUE(store);

  // Set policy. Verify that the policy-provided user image is downloaded, set
  // and persisted.
  user_policy_.payload().mutable_useravatarimage()->set_value(
      ConstructPolicy(test::kUserAvatarImage2RelativePath));
  user_policy_.Build();
  fake_session_manager_client_->set_user_policy(cryptohome_id_,
                                                user_policy_.GetBlob());
  run_loop_.reset(new base::RunLoop);
  store->Load();
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*policy_image_, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_,
                      user_manager::User::USER_IMAGE_EXTERNAL,
                      GetUserImagePath(enterprise_account_id_, "jpg"));

  std::unique_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(enterprise_account_id_, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_->width(), saved_image->width());
  EXPECT_EQ(policy_image_->height(), saved_image->height());

  // Choose a different user image. Verify that the user image does not change
  // as policy takes precedence.
  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(enterprise_account_id_);
  user_image_manager->SaveUserDefaultImageIndex(
      default_user_image::kFirstDefaultImageIndex);

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*policy_image_, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_,
                      user_manager::User::USER_IMAGE_EXTERNAL,
                      GetUserImagePath(enterprise_account_id_, "jpg"));

  saved_image =
      test::ImageLoader(GetUserImagePath(enterprise_account_id_, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_->width(), saved_image->width());
  EXPECT_EQ(policy_image_->height(), saved_image->height());
}

}  // namespace chromeos
