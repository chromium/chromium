// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_images_policy_handler.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/public/cpp/ambient/ambient_managed_photo_source.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/test/ash_test_helper.h"
#include "base/base64url.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/sha1.h"
#include "base/memory/raw_ptr.h"
#include "base/test/repeating_test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/handlers/screensaver_image_downloader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "screensaver_image_downloader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {

constexpr char kTestProfileDirectoryName[] = "test_profile";
constexpr char kUserEmail[] = "user@mail.com";
constexpr char kFakeFilePath1[] = "/path/to/file1";
constexpr char kFakeFilePath2[] = "/path/to/file2";

constexpr char kCacheDirectoryName[] = "managed_screensaver";
constexpr char kCacheFileExt[] = ".cache";

constexpr char kImageUrl1[] = "http://example.com/1.jpg";
constexpr char kImageUrl2[] = "http://example.com/2.jpg";
constexpr char kFileContents1[] = "file contents 1";
constexpr char kFileContents2[] = "file contents 2";

constexpr size_t kMaxUrlsToProcessFromPolicy = 25u;

}  // namespace

class ScreensaverImagesPolicyHandlerTest : public testing::Test {
 public:
  ScreensaverImagesPolicyHandlerTest() = default;

  ScreensaverImagesPolicyHandlerTest(
      const ScreensaverImagesPolicyHandlerTest&) = delete;
  ScreensaverImagesPolicyHandlerTest& operator=(
      const ScreensaverImagesPolicyHandlerTest&) = delete;

  ~ScreensaverImagesPolicyHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    ash_test_helper_.SetUp(ash::AshTestHelper::InitParams());

    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    fake_profile_dir_ =
        temp_dir_.GetPath().AppendASCII(kTestProfileDirectoryName);

    profile_ = TestingProfile::Builder()
                   .SetProfileName(kUserEmail)
                   .SetSharedURLLoaderFactory(
                       base::MakeRefCounted<
                           network::WeakWrapperSharedURLLoaderFactory>(
                           &url_loader_factory_))
                   .SetPath(fake_profile_dir_)
                   .Build();
  }

  void TearDown() override { policy_handler_.reset(); }

  void TriggerOnDownloadJobCompleted(ScreensaverImageDownloadResult result,
                                     absl::optional<base::FilePath> path) {
    ASSERT_TRUE(ScreensaverImagesPolicyHandler::Get());
    policy_handler_->OnDownloadJobCompleted(result, path);
  }

  void RegisterUser(const AccountId& account_id,
                    std::unique_ptr<PrefService> pref_service) {
    ASSERT_TRUE(profile_);
    const user_manager::User* user =
        fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, false, user_manager::USER_TYPE_REGULAR, profile_.get());
    fake_user_manager_->UserLoggedIn(
        user->GetAccountId(), user->username_hash(), true /* browser_restart */,
        false /* is_child */);
    fake_user_manager_->SwitchActiveUser(account_id);

    ash::TestSessionControllerClient* session_controller_client =
        ash_test_helper_.test_session_controller_client();
    session_controller_client->Reset();
    session_controller_client->AddUserSession(kUserEmail,
                                              user_manager::USER_TYPE_REGULAR,
                                              /*provide_pref_service =*/false);
    session_controller_client->SetUserPrefService(account_id,
                                                  std::move(pref_service));
    session_controller_client->SwitchActiveUser(account_id);
    session_controller_client->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  void CreateHandlerInstanceWithUserProfile() {
    policy_handler_ = std::make_unique<ScreensaverImagesPolicyHandler>();

    // Verify that the handler is instantiated without an image donwloader.
    EXPECT_FALSE(policy_handler_->image_downloader_);

    // Create a fake user prefs map.
    auto user_prefs = std::make_unique<TestingPrefServiceSimple>();
    ash::RegisterUserProfilePrefs(user_prefs->registry(), /*for_test=*/true);
    ScreensaverImagesPolicyHandler::RegisterPrefs(user_prefs->registry());

    // Keep a raw pointer to the user prefs before transferring ownership.
    user_prefs_ = user_prefs.get();
    RegisterUser(AccountId::FromUserEmail(kUserEmail), std::move(user_prefs));

    // Verify that the policy handler detected the new user and created a new
    // image downloader instance.
    EXPECT_TRUE(policy_handler_->image_downloader_);
  }

  base::FilePath GetExpectedFilePath(const std::string url) {
    std::string file_name;
    base::Base64UrlEncode(base::SHA1HashString(url),
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &file_name);
    file_name += kCacheFileExt;

    return fake_profile_dir_.AppendASCII(kCacheDirectoryName)
        .AppendASCII(file_name);
  }

  TestingPrefServiceSimple* user_prefs() {
    CHECK(user_prefs_);
    return user_prefs_;
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  ash::AshTestHelper ash_test_helper_;
  raw_ptr<ash::FakeChromeUserManager, ExperimentalAsh> fake_user_manager_ =
      nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  base::ScopedTempDir temp_dir_;
  base::FilePath fake_profile_dir_;
  std::unique_ptr<TestingProfile> profile_;

  // Ownership of this pref service is transferred to the session controller
  raw_ptr<TestingPrefServiceSimple, ExperimentalAsh> user_prefs_ = nullptr;

  network::TestURLLoaderFactory url_loader_factory_;

  std::unique_ptr<ScreensaverImagesPolicyHandler> policy_handler_;
};

TEST_F(ScreensaverImagesPolicyHandlerTest, SingletonInitialization) {
  EXPECT_EQ(nullptr, ScreensaverImagesPolicyHandler::Get());

  {
    std::unique_ptr<ScreensaverImagesPolicyHandler> handler_instance =
        std::make_unique<ScreensaverImagesPolicyHandler>();

    EXPECT_EQ(handler_instance.get(), ScreensaverImagesPolicyHandler::Get());
  }

  EXPECT_EQ(nullptr, ScreensaverImagesPolicyHandler::Get());
}

TEST_F(ScreensaverImagesPolicyHandlerTest, ShouldRunCallbackIfImagesUpdated) {
  CreateHandlerInstanceWithUserProfile();
  base::test::RepeatingTestFuture<std::vector<base::FilePath>> test_future;
  ScreensaverImagesPolicyHandler::Get()->SetScreensaverImagesUpdatedCallback(
      test_future.GetCallback<const std::vector<base::FilePath>&>());

  // Expect callbacks when images are downloaded.
  base::FilePath file_path1(kFakeFilePath1);
  {
    TriggerOnDownloadJobCompleted(ScreensaverImageDownloadResult::kSuccess,
                                  file_path1);
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    ASSERT_EQ(1u, file_paths.size());
    EXPECT_EQ(file_path1, file_paths.front());
  }
  base::FilePath file_path2(kFakeFilePath2);
  {
    TriggerOnDownloadJobCompleted(ScreensaverImageDownloadResult::kSuccess,
                                  file_path2);
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    ASSERT_EQ(2u, file_paths.size());
    EXPECT_NE(file_paths.end(),
              std::find(file_paths.begin(), file_paths.end(), file_path1));
    EXPECT_NE(file_paths.end(),
              std::find(file_paths.begin(), file_paths.end(), file_path2));
  }

  EXPECT_TRUE(test_future.IsEmpty());
}

TEST_F(ScreensaverImagesPolicyHandlerTest, DownloadImagesTest) {
  CreateHandlerInstanceWithUserProfile();
  base::test::RepeatingTestFuture<std::vector<base::FilePath>> test_future;
  ScreensaverImagesPolicyHandler::Get()->SetScreensaverImagesUpdatedCallback(
      test_future.GetCallback<const std::vector<base::FilePath>&>());

  base::Value::List image_urls;
  image_urls.Append(kImageUrl1);
  image_urls.Append(kImageUrl2);

  // Fill the pref service to trigger the logic under test.
  user_prefs()->SetManagedPref(
      ash::ambient::prefs::kAmbientModeManagedScreensaverImages,
      image_urls.Clone());

  // Verify that the first request is resolved
  {
    url_loader_factory()->AddResponse(image_urls[0].GetString(),
                                      kFileContents1);
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    ASSERT_EQ(1u, file_paths.size());
    EXPECT_EQ(GetExpectedFilePath(kImageUrl1), file_paths.front());
  }

  // Verify that the second request is resolved and both file paths are present.
  {
    url_loader_factory()->AddResponse(image_urls[1].GetString(),
                                      kFileContents2);
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    ASSERT_EQ(2u, file_paths.size());
    EXPECT_NE(file_paths.end(), std::find(file_paths.begin(), file_paths.end(),
                                          GetExpectedFilePath(kImageUrl1)));
    EXPECT_NE(file_paths.end(), std::find(file_paths.begin(), file_paths.end(),
                                          GetExpectedFilePath(kImageUrl2)));
  }
}

TEST_F(ScreensaverImagesPolicyHandlerTest, VerifyPolicyLimit) {
  CreateHandlerInstanceWithUserProfile();
  base::test::RepeatingTestFuture<std::vector<base::FilePath>> test_future;
  ScreensaverImagesPolicyHandler::Get()->SetScreensaverImagesUpdatedCallback(
      test_future.GetCallback<const std::vector<base::FilePath>&>());

  base::Value::List image_urls;
  // Append the same URL request `kMaxUrlsToProcessFromPolicy` times. This
  // should be the only URL that can be requested.
  for (size_t i = 0; i < kMaxUrlsToProcessFromPolicy; ++i) {
    image_urls.Append(kImageUrl1);
  }
  // Append a new URL that must be ignored.
  image_urls.Append(kImageUrl2);

  // Add both responses in the URL factory.
  url_loader_factory()->AddResponse(image_urls[0].GetString(), kFileContents1);
  url_loader_factory()->AddResponse(image_urls[1].GetString(), kFileContents2);

  // Fill the pref service to trigger the logic under test.
  user_prefs()->SetManagedPref(
      ash::ambient::prefs::kAmbientModeManagedScreensaverImages,
      image_urls.Clone());

  const base::FilePath expected_file_path = GetExpectedFilePath(kImageUrl1);
  for (size_t i = 0; i < kMaxUrlsToProcessFromPolicy; ++i) {
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    ASSERT_TRUE(file_paths.size());
    ASSERT_GT(kMaxUrlsToProcessFromPolicy, file_paths.size());
    for (const base::FilePath& p : file_paths) {
      EXPECT_EQ(expected_file_path, p);
    }
  }
}

}  // namespace policy
