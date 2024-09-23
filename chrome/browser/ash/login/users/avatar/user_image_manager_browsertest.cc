// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_test_util.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "crypto/rsa_private_key.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kRandomTokenStrForTesting[] = "random-token-str-for-testing";

policy::CloudPolicyStore* GetStoreForUser(const user_manager::User* user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile) {
    ADD_FAILURE();
    return nullptr;
  }
  policy::UserCloudPolicyManagerAsh* policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  if (!policy_manager) {
    ADD_FAILURE();
    return nullptr;
  }
  return policy_manager->core()->store();
}

}  // namespace

class UserImageManagerTestBase : public LoginManagerTest,
                                 public user_manager::UserManager::Observer {
 public:
  UserImageManagerTestBase(const UserImageManagerTestBase&) = delete;
  UserImageManagerTestBase& operator=(const UserImageManagerTestBase&) = delete;

  std::unique_ptr<net::test_server::BasicHttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url.find("/avatar.jpg") == std::string::npos) {
      return nullptr;
    }

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
    {
      base::ScopedAllowBlockingForTesting allow_io;
      EXPECT_TRUE(ReadFileToString(test_data_dir.Append("chromeos")
                                       .Append("avatars")
                                       .Append("avatar1.jpg"),
                                   &profile_image_data));
    }
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("image/jpeg");
    response->set_code(net::HTTP_OK);
    response->set_content(profile_image_data);
    return response;
  }

 protected:
  UserImageManagerTestBase() = default;

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
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
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
  }

  void TearDownOnMainThread() override {
    user_manager::UserManager::Get()->RemoveObserver(this);
    LoginManagerTest::TearDownOnMainThread();
  }

  // UserManager::Observer overrides:
  void LocalStateChanged(user_manager::UserManager* user_manager) override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  // Logs in `account_id`.
  void LogIn(const AccountId& account_id) {
    user_manager::UserManager::Get()->UserLoggedIn(
        account_id, account_id.GetUserEmail(), false /* browser_restart */,
        false /* is_child */);
  }

  // Verifies user image info.
  void ExpectUserImageInfo(const AccountId& account_id,
                           int image_index,
                           const base::FilePath& image_path) {
    const base::Value::Dict& images_pref =
        local_state_->GetDict(UserImageManagerImpl::kUserImageProperties);
    const base::Value::Dict* image_properties =
        images_pref.FindDict(account_id.GetUserEmail());
    ASSERT_TRUE(image_properties);
    std::optional<int> actual_image_index =
        image_properties->FindInt(UserImageManagerImpl::kImageIndexNodeName);
    const std::string* actual_image_path =
        image_properties->FindString(UserImageManagerImpl::kImagePathNodeName);
    ASSERT_TRUE(actual_image_index.has_value() && actual_image_path);
    EXPECT_EQ(image_index, actual_image_index.value());
    EXPECT_EQ(image_path.value(), *actual_image_path);
  }

  // Returns the image path for user `account_id` with specified `extension`.
  base::FilePath GetUserImagePath(const AccountId& account_id,
                                  const std::string& extension) {
    return user_data_dir_.Append(account_id.GetUserEmail())
        .AddExtension(extension);
  }

  void UpdatePrimaryAccountInfo(Profile* profile) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    // Sync consent level doesn't matter here.
    CoreAccountInfo core_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    signin::SetRefreshTokenForAccount(identity_manager, core_info.account_id,
                                      kRandomTokenStrForTesting);
    AccountInfo account_info;
    account_info.email = core_info.email;
    account_info.gaia = core_info.gaia;
    account_info.account_id = core_info.account_id;
    account_info.is_under_advanced_protection =
        core_info.is_under_advanced_protection;
    account_info.full_name = account_info.email;
    account_info.given_name = account_info.email;
    account_info.hosted_domain = kNoHostedDomainFound;
    account_info.locale = account_info.email;
    account_info.picture_url =
        embedded_test_server()->GetURL("/avatar.jpg").spec();
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
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
    UserImageManagerImpl* uim =
        UserImageManagerRegistry::Get()->GetManager(user->GetAccountId());
    if (uim->job_.get()) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

  base::FilePath test_data_dir_;
  base::FilePath user_data_dir_;

  raw_ptr<PrefService, DanglingUntriaged> local_state_;

  gfx::ImageSkia decoded_image_;

  std::unique_ptr<base::RunLoop> run_loop_;

  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_http_response_;

  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

class UserImageManagerTest : public UserImageManagerTestBase {
 public:
  UserImageManagerTest() {
    login_manager_mixin_.AppendRegularUsers(1);
    test_account_id1_ = login_manager_mixin_.users()[0].account_id;
  }
  void SetUpOnMainThread() override {
    UserImageManagerTestBase::SetUpOnMainThread();
    // FakeGaia authorizes requests for profile info.
    FakeGaia::AccessTokenInfo token_info;
    token_info.any_scope = true;
    token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
    token_info.token = kRandomTokenStrForTesting;
    token_info.email = test_account_id1_.GetUserEmail();
    fake_gaia_.fake_gaia()->IssueOAuthToken(kRandomTokenStrForTesting,
                                            token_info);
    fake_gaia_.fake_gaia()->MapEmailToGaiaId(test_account_id1_.GetUserEmail(),
                                             test_account_id1_.GetGaiaId());
  }

 protected:
  AccountId test_account_id1_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveAndLoadUserImage) {
  // Setup a user with JPEG image.
  LoginUser(test_account_id1_);
  run_loop_ = std::make_unique<base::RunLoop>();
  const gfx::ImageSkia& image = default_user_image::GetStubDefaultImage();
  UserImageManagerImpl* user_image_manager =
      UserImageManagerRegistry::Get()->GetManager(test_account_id1_);
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
  if (user->image_index() == user_manager::UserImage::Type::kInvalid) {
    test::UserImageChangeWaiter().Wait();
  }
  // Check image dimensions. Images can't be compared since JPEG is lossy.
  const gfx::ImageSkia& saved_image = default_user_image::GetStubDefaultImage();
  EXPECT_EQ(saved_image.width(), user->GetImage().width());
  EXPECT_EQ(saved_image.height(), user->GetImage().height());
}

// Verifies that SaveUserDefaultImageIndex() correctly sets and persists the
// chosen user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserDefaultImageIndex) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);

  UserImageManagerImpl::SkipDefaultUserImageDownloadForTesting();

  UserImageManagerImpl* user_image_manager =
      UserImageManagerRegistry::Get()->GetManager(test_account_id1_);
  user_image_manager->SaveUserDefaultImageIndex(
      default_user_image::kFirstDefaultImageIndex);

  EXPECT_EQ(default_user_image::kFirstDefaultImageIndex, user->image_index());
  ExpectUserImageInfo(test_account_id1_,
                      default_user_image::kFirstDefaultImageIndex,
                      base::FilePath());
}

// Verifies that SaveUserImage() correctly sets and persists the chosen user
// image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImage) {
  LoginUser(test_account_id1_);
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);

  SkBitmap custom_image_bitmap;
  custom_image_bitmap.allocN32Pixels(10, 10);
  custom_image_bitmap.eraseColor(SK_ColorWHITE);
  custom_image_bitmap.setImmutable();
  const gfx::ImageSkia custom_image =
      gfx::ImageSkia::CreateFrom1xBitmap(custom_image_bitmap);

  run_loop_ = std::make_unique<base::RunLoop>();
  UserImageManagerImpl* user_image_manager =
      UserImageManagerRegistry::Get()->GetManager(test_account_id1_);
  user_image_manager->SaveUserImage(user_manager::UserImage::CreateAndEncode(
      custom_image, user_manager::UserImage::FORMAT_JPEG));
  run_loop_->Run();

  EXPECT_EQ(user_manager::UserImage::Type::kExternal, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(custom_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_,
                      user_manager::UserImage::Type::kExternal,
                      GetUserImagePath(test_account_id1_, "jpg"));

  const gfx::ImageSkia saved_image =
      test::ImageLoader(GetUserImagePath(test_account_id1_, "jpg")).Load();
  ASSERT_FALSE(saved_image.isNull());

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(custom_image.width(), saved_image.width());
  EXPECT_EQ(custom_image.height(), saved_image.height());
}

// Verifies that SaveUserImageFromFile() correctly sets and persists the chosen
// user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImageFromFile) {
  LoginUser(test_account_id1_);
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);

  const base::FilePath custom_image_path =
      test_data_dir_.Append(test::kUserAvatarImage1RelativePath);
  const gfx::ImageSkia custom_image =
      test::ImageLoader(custom_image_path).Load();
  ASSERT_FALSE(custom_image.isNull());

  run_loop_ = std::make_unique<base::RunLoop>();
  UserImageManagerImpl* user_image_manager =
      UserImageManagerRegistry::Get()->GetManager(test_account_id1_);
  user_image_manager->SaveUserImageFromFile(custom_image_path);
  run_loop_->Run();

  EXPECT_EQ(user_manager::UserImage::Type::kExternal, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(custom_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_,
                      user_manager::UserImage::Type::kExternal,
                      GetUserImagePath(test_account_id1_, "jpg"));

  const gfx::ImageSkia saved_image =
      test::ImageLoader(GetUserImagePath(test_account_id1_, "jpg")).Load();
  ASSERT_FALSE(saved_image.isNull());

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(custom_image.width(), saved_image.width());
  EXPECT_EQ(custom_image.height(), saved_image.height());

  // Replace the user image with a PNG file with transparent pixels.
  const base::FilePath transparent_image_path =
      test_data_dir_.Append(test::kUserAvatarImage3RelativePath);
  const gfx::ImageSkia transparent_image =
      test::ImageLoader(transparent_image_path).Load();
  ASSERT_FALSE(transparent_image.isNull());
  // This image should have transparent pixels (i.e. not opaque).
  EXPECT_FALSE(SkBitmap::ComputeIsOpaque(*transparent_image.bitmap()));

  run_loop_ = std::make_unique<base::RunLoop>();
  user_image_manager->SaveUserImageFromFile(transparent_image_path);
  run_loop_->Run();

  EXPECT_TRUE(test::AreImagesEqual(transparent_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_,
                      user_manager::UserImage::Type::kExternal,
                      GetUserImagePath(test_account_id1_, "png"));

  const gfx::ImageSkia new_saved_image =
      test::ImageLoader(GetUserImagePath(test_account_id1_, "png")).Load();
  ASSERT_FALSE(new_saved_image.isNull());

  // The saved image should have transparent pixels (i.e. not opaque).
  EXPECT_FALSE(SkBitmap::ComputeIsOpaque(*new_saved_image.bitmap()));

  base::ScopedAllowBlockingForTesting allow_io;
  // The old user image file in JPEG should be deleted. Only the PNG version
  // should stay.
  EXPECT_FALSE(base::PathExists(GetUserImagePath(test_account_id1_, "jpg")));
  EXPECT_TRUE(base::PathExists(GetUserImagePath(test_account_id1_, "png")));
}

// Verifies that SaveUserImageFromProfileImage() correctly downloads, sets and
// persists the chosen user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImageFromProfileImage) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_account_id1_);
  ASSERT_TRUE(user);

  UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting();
  LoginUser(test_account_id1_);
  UpdatePrimaryAccountInfo(ProfileHelper::Get()->GetProfileByUser(user));

  run_loop_ = std::make_unique<base::RunLoop>();
  UserImageManagerImpl* user_image_manager =
      UserImageManagerRegistry::Get()->GetManager(test_account_id1_);
  user_image_manager->SaveUserImageFromProfileImage();
  run_loop_->Run();

  CompleteProfileImageDownload();

  const gfx::ImageSkia& profile_image =
      user_image_manager->DownloadedProfileImage();

  EXPECT_EQ(user_manager::UserImage::Type::kProfile, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(profile_image, user->GetImage()));
  ExpectUserImageInfo(test_account_id1_,
                      user_manager::UserImage::Type::kProfile,
                      GetUserImagePath(test_account_id1_, "jpg"));

  const gfx::ImageSkia saved_image =
      test::ImageLoader(GetUserImagePath(test_account_id1_, "jpg")).Load();
  ASSERT_FALSE(saved_image.isNull());

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(profile_image.width(), saved_image.width());
  EXPECT_EQ(profile_image.height(), saved_image.height());
}

class UserImageManagerPolicyTest : public UserImageManagerTestBase,
                                   public policy::CloudPolicyStore::Observer {
 public:
  UserImageManagerPolicyTest(const UserImageManagerPolicyTest&) = delete;
  UserImageManagerPolicyTest& operator=(const UserImageManagerPolicyTest&) =
      delete;

 protected:
  UserImageManagerPolicyTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()) {
    login_manager_.AppendManagedUsers(1);
    enterprise_account_id_ = login_manager_.users()[0].account_id;
    cryptohome_id_ = cryptohome::CreateAccountIdentifierFromAccountId(
        enterprise_account_id_);
  }

  // UserImageManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    device_policy_.Build();
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        owner_key_util_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    // Override FakeSessionManagerClient. This will be shut down by the browser.
    SessionManagerClient::InitializeFakeInMemory();
    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_.GetBlob());

    UserImageManagerTestBase::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    UserImageManagerTestBase::SetUpOnMainThread();

    base::FilePath user_keys_dir;
    ASSERT_TRUE(base::PathService::Get(
        chromeos::dbus_paths::DIR_USER_POLICY_KEYS, &user_keys_dir));
    const std::string sanitized_username =
        UserDataAuthClient::GetStubSanitizedUsername(cryptohome_id_);
    const base::FilePath user_key_file =
        user_keys_dir.AppendASCII(sanitized_username).AppendASCII("policy.pub");
    std::vector<uint8_t> user_key_bits;
    ASSERT_TRUE(user_policy_.GetSigningKey()->ExportPublicKey(&user_key_bits));
    ASSERT_TRUE(base::CreateDirectory(user_key_file.DirName()));
    ASSERT_TRUE(base::WriteFile(user_key_file, user_key_bits));
    user_policy_.policy_data().set_username(
        enterprise_account_id_.GetUserEmail());
    user_policy_.policy_data().set_gaia_id(enterprise_account_id_.GetGaiaId());

    policy_image_ = test::ImageLoader(test_data_dir_.Append(
                                          test::kUserAvatarImage2RelativePath))
                        .Load();
    ASSERT_FALSE(policy_image_.isNull());
  }

  // policy::CloudPolicyStore::Observer overrides:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnStoreError(policy::CloudPolicyStore* store) override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  std::string ConstructPolicy(const std::string& relative_path) {
    base::ScopedAllowBlockingForTesting allow_io;
    std::string image_data;
    if (!base::ReadFileToString(test_data_dir_.Append(relative_path),
                                &image_data)) {
      ADD_FAILURE();
    }
    std::string policy;
    base::JSONWriter::Write(policy::test::ConstructExternalDataReference(
                                embedded_test_server()
                                    ->GetURL(std::string("/") + relative_path)
                                    .spec(),
                                image_data),
                            &policy);
    return policy;
  }

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  policy::UserPolicyBuilder user_policy_;
  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;

  gfx::ImageSkia policy_image_;

  AccountId enterprise_account_id_;
  cryptohome::AccountIdentifier cryptohome_id_ =
      cryptohome::CreateAccountIdentifierFromAccountId(enterprise_account_id_);
  LoginManagerMixin login_manager_{&mixin_host_};
};

// Verifies that the user image can be set through policy. Also verifies that
// after the policy has been cleared, the user is able to choose a different
// image.
IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, SetAndClear) {
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
  FakeSessionManagerClient::Get()->set_user_policy(cryptohome_id_,
                                                   user_policy_.GetBlob());
  run_loop_ = std::make_unique<base::RunLoop>();
  store->Load();
  run_loop_->Run();

  EXPECT_EQ(user_manager::UserImage::Type::kExternal, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(policy_image_, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_,
                      user_manager::UserImage::Type::kExternal,
                      GetUserImagePath(enterprise_account_id_, "jpg"));

  gfx::ImageSkia saved_image =
      test::ImageLoader(GetUserImagePath(enterprise_account_id_, "jpg")).Load();
  ASSERT_FALSE(saved_image.isNull());

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_.width(), saved_image.width());
  EXPECT_EQ(policy_image_.height(), saved_image.height());

  UserImageManagerImpl::SkipDefaultUserImageDownloadForTesting();

  // Clear policy. Verify that the user image switches to a random default
  // image.
  user_policy_.payload().Clear();
  user_policy_.Build();
  FakeSessionManagerClient::Get()->set_user_policy(cryptohome_id_,
                                                   user_policy_.GetBlob());
  run_loop_ = std::make_unique<base::RunLoop>();
  store->AddObserver(this);
  store->Load();
  run_loop_->Run();
  store->RemoveObserver(this);
  base::RunLoop().RunUntilIdle();

  const int default_image_index = user->image_index();
  EXPECT_TRUE(default_user_image::IsValidIndex(default_image_index));
  EXPECT_TRUE(default_user_image::IsInCurrentImageSet(default_image_index));
  ExpectUserImageInfo(enterprise_account_id_, default_image_index,
                      base::FilePath());

  // Choose a different user image. Verify that the chosen user image is set and
  // persisted.
  const int user_image_index =
      default_image_index == default_user_image::kFirstDefaultImageIndex
          ? default_user_image::kFirstDefaultImageIndex + 1
          : default_user_image::kFirstDefaultImageIndex;
  EXPECT_TRUE(default_user_image::IsValidIndex(user_image_index));
  EXPECT_TRUE(default_user_image::IsInCurrentImageSet(user_image_index));

  UserImageManagerImpl* user_image_manager =
      UserImageManagerRegistry::Get()->GetManager(enterprise_account_id_);
  user_image_manager->SaveUserDefaultImageIndex(user_image_index);

  EXPECT_EQ(user_image_index, user->image_index());
  ExpectUserImageInfo(enterprise_account_id_, user_image_index,
                      base::FilePath());
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

  UserImageManagerImpl::SkipDefaultUserImageDownloadForTesting();

  // Choose a user image. Verify that the chosen user image is set and
  // persisted.
  UserImageManagerImpl* user_image_manager =
      UserImageManagerRegistry::Get()->GetManager(enterprise_account_id_);
  user_image_manager->SaveUserDefaultImageIndex(
      default_user_image::kFirstDefaultImageIndex);

  EXPECT_EQ(default_user_image::kFirstDefaultImageIndex, user->image_index());
  ExpectUserImageInfo(enterprise_account_id_,
                      default_user_image::kFirstDefaultImageIndex,
                      base::FilePath());

  // Set policy. Verify that the policy-provided user image is downloaded, set
  // and persisted, overriding the previously set image.
  user_policy_.payload().mutable_useravatarimage()->set_value(
      ConstructPolicy(test::kUserAvatarImage2RelativePath));
  user_policy_.Build();
  FakeSessionManagerClient::Get()->set_user_policy(cryptohome_id_,
                                                   user_policy_.GetBlob());
  run_loop_ = std::make_unique<base::RunLoop>();
  store->Load();
  run_loop_->Run();

  EXPECT_EQ(user_manager::UserImage::Type::kExternal, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(policy_image_, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_,
                      user_manager::UserImage::Type::kExternal,
                      GetUserImagePath(enterprise_account_id_, "jpg"));

  gfx::ImageSkia saved_image =
      test::ImageLoader(GetUserImagePath(enterprise_account_id_, "jpg")).Load();
  ASSERT_FALSE(saved_image.isNull());

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_.width(), saved_image.width());
  EXPECT_EQ(policy_image_.height(), saved_image.height());
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
  FakeSessionManagerClient::Get()->set_user_policy(cryptohome_id_,
                                                   user_policy_.GetBlob());
  run_loop_ = std::make_unique<base::RunLoop>();
  store->Load();
  run_loop_->Run();

  EXPECT_EQ(user_manager::UserImage::Type::kExternal, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(policy_image_, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_,
                      user_manager::UserImage::Type::kExternal,
                      GetUserImagePath(enterprise_account_id_, "jpg"));

  gfx::ImageSkia saved_image =
      test::ImageLoader(GetUserImagePath(enterprise_account_id_, "jpg")).Load();
  ASSERT_FALSE(saved_image.isNull());

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_.width(), saved_image.width());
  EXPECT_EQ(policy_image_.height(), saved_image.height());

  // Choose a different user image. Verify that the user image does not change
  // as policy takes precedence.
  UserImageManagerImpl* user_image_manager =
      UserImageManagerRegistry::Get()->GetManager(enterprise_account_id_);
  user_image_manager->SaveUserDefaultImageIndex(
      default_user_image::kFirstDefaultImageIndex);

  EXPECT_EQ(user_manager::UserImage::Type::kExternal, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(policy_image_, user->GetImage()));
  ExpectUserImageInfo(enterprise_account_id_,
                      user_manager::UserImage::Type::kExternal,
                      GetUserImagePath(enterprise_account_id_, "jpg"));

  saved_image =
      test::ImageLoader(GetUserImagePath(enterprise_account_id_, "jpg")).Load();
  ASSERT_FALSE(saved_image.isNull());

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_.width(), saved_image.width());
  EXPECT_EQ(policy_image_.height(), saved_image.height());
}

}  // namespace ash
