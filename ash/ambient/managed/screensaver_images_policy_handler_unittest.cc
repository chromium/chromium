// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/managed/screensaver_images_policy_handler.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/ambient/managed/screensaver_image_downloader.h"
#include "ash/ambient/test/ambient_ash_test_helper.h"
#include "ash/ambient/test/test_ambient_client.h"
#include "ash/constants/ash_paths.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/sha1.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_type.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kUserEmail[] = "user@mail.com";
constexpr char kFakeFilePath1[] = "/path/to/file1";
constexpr char kFakeFilePath2[] = "/path/to/file2";

constexpr char kCacheDirectoryName[] = "managed_screensaver";
constexpr char kSigninDirectoryName[] = "signin";
constexpr char kManagedGuestDirectoryName[] = "guest";
constexpr char kCacheFileExt[] = ".cache";

constexpr char kImageUrl1[] = "https://example.com/1.jpg";
constexpr char kImageUrl1Alt[] = "https://EXAMPLE.com/1.jpg";
constexpr char kImageUrl2[] = "https://example.com/2.jpg";
constexpr char kFileContents1[] = "file contents 1";
constexpr char kFileContents2[] = "file contents 2";

constexpr size_t kMaxUrlsToProcessFromPolicy = 25u;

}  // namespace

struct ScreensaverImagesPolicyHandlerTestCase {
  std::string test_name;
  ScreensaverImagesPolicyHandler::HandlerType handle_type;
  std::string base_directory;
};

class ScreensaverImagesPolicyHandlerTest : public AshTestBase {
 public:
  ScreensaverImagesPolicyHandlerTest() = default;

  ScreensaverImagesPolicyHandlerTest(
      const ScreensaverImagesPolicyHandlerTest&) = delete;
  ScreensaverImagesPolicyHandlerTest& operator=(
      const ScreensaverImagesPolicyHandlerTest&) = delete;

  ~ScreensaverImagesPolicyHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    home_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_HOME, temp_dir_.GetPath());
    managed_screensaver_dir_override_ =
        std::make_unique<base::ScopedPathOverride>(
            ash::DIR_DEVICE_POLICY_SCREENSAVER_DATA, temp_dir_.GetPath());
  }

  void TearDown() override {
    policy_handler_.reset();
    // Remove the custom overrides to test directory behavior before the tear
    // down so that original override can be restored.
    home_dir_override_.reset();
    managed_screensaver_dir_override_.reset();
    AshTestBase::TearDown();
  }

  void TriggerOnDownloadedImageListUpdated(
      const std::vector<base::FilePath>& image_list) {
    ASSERT_TRUE(policy_handler());
    policy_handler_->OnDownloadedImageListUpdated(image_list);
  }

  ScreensaverImageDownloader* GetPrivateImageDownloader(
      const ScreensaverImagesPolicyHandler& policy_handler) {
    return policy_handler.image_downloader_.get();
  }

  void VerifyPrivateImageDownloaderDownloadFolder(
      ScreensaverImagesPolicyHandler& policy_handler,
      const base::FilePath& expected_path) {
    ASSERT_TRUE(policy_handler.image_downloader_.get());
    EXPECT_EQ(expected_path,
              policy_handler.image_downloader_->GetDowloadDirForTesting());
  }

  void RegisterUserWithUserPrefs(const AccountId& account_id,
                                 user_manager::UserType user_type) {
    // Create a fake user prefs map.
    auto user_prefs = std::make_unique<TestingPrefServiceSimple>();
    RegisterUserProfilePrefs(user_prefs->registry(), /*country=*/"",
                             /*for_test=*/true);

    // Keep a raw pointer to the user prefs before transferring ownership.
    active_prefs_ = user_prefs.get();

    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(
        kUserEmail, user_type,
        /*provide_pref_service=*/false);
    GetSessionControllerClient()->SetUserPrefService(account_id,
                                                     std::move(user_prefs));
    GetSessionControllerClient()->SwitchActiveUser(account_id);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  TestingPrefServiceSimple* active_prefs() {
    CHECK(active_prefs_);
    return active_prefs_;
  }

  network::TestURLLoaderFactory& url_loader_factory() {
    return GetAmbientAshTestHelper()
        ->ambient_client()
        .test_url_loader_factory();
  }

  ScreensaverImagesPolicyHandler* policy_handler() {
    return policy_handler_.get();
  }

 protected:
  // Temp directory to simulate the download directory.
  base::ScopedTempDir temp_dir_;

  // Used to override the base download path to |temp_dir_|
  std::unique_ptr<base::ScopedPathOverride> home_dir_override_;
  std::unique_ptr<base::ScopedPathOverride> managed_screensaver_dir_override_;

  // Ownership of this pref service is transferred to the session controller
  raw_ptr<TestingPrefServiceSimple, DanglingUntriaged> active_prefs_ = nullptr;

  // Class under test
  std::unique_ptr<ScreensaverImagesPolicyHandler> policy_handler_;
};

TEST_F(ScreensaverImagesPolicyHandlerTest, FactoryFunctionTestSignin) {
  // Signin
  auto handler = ScreensaverImagesPolicyHandler::Create(
      Shell::Get()->session_controller()->GetSigninScreenPrefService());
  // Verify that the policy handler detected the new user and created a new
  // image downloader instance.
  EXPECT_TRUE(GetPrivateImageDownloader(*handler));
  VerifyPrivateImageDownloaderDownloadFolder(
      *handler, temp_dir_.GetPath().AppendASCII(kSigninDirectoryName));
}
TEST_F(ScreensaverImagesPolicyHandlerTest, FactoryFunctionTestUser) {
  // User
  RegisterUserWithUserPrefs(AccountId::FromUserEmail(kUserEmail),
                            user_manager::UserType::kRegular);
  auto handler = ScreensaverImagesPolicyHandler::Create(active_prefs());
  // Verify that the policy handler detected the new user and created a new
  // image downloader instance.
  EXPECT_TRUE(GetPrivateImageDownloader(*handler));
  VerifyPrivateImageDownloaderDownloadFolder(
      *handler, temp_dir_.GetPath().AppendASCII(kCacheDirectoryName));
}
TEST_F(ScreensaverImagesPolicyHandlerTest, FactoryFunctionTestManagedGuest) {
  // Managed Guest
  GetSessionControllerClient()->RequestSignOut();
  RegisterUserWithUserPrefs(AccountId::FromUserEmail(kUserEmail),
                            user_manager::UserType::kPublicAccount);
  auto handler = ScreensaverImagesPolicyHandler::Create(
      Shell::Get()->session_controller()->GetActivePrefService());
  // Verify that the policy handler detected the new user and created a new
  // image downloader instance.
  EXPECT_TRUE(GetPrivateImageDownloader(*handler));
  VerifyPrivateImageDownloaderDownloadFolder(
      *handler, temp_dir_.GetPath().AppendASCII(kManagedGuestDirectoryName));
}

class ScreensaverImagesPolicyHandlerForAnySessionTest
    : public ScreensaverImagesPolicyHandlerTest,
      public ::testing::WithParamInterface<
          ScreensaverImagesPolicyHandlerTestCase> {
 public:
  ScreensaverImagesPolicyHandlerForAnySessionTest() {
    TestSessionControllerClient::DisableAutomaticallyProvideSigninPref();
  }
  // ScreensaverImagesPolicyHandlerTest:
  void SetUp() override {
    ScreensaverImagesPolicyHandlerTest::SetUp();
    RegisterProfilePrefs();
    CreateScreensaverImagesPolicyHandler();
  }

  void RegisterProfilePrefs() {
    ScreensaverImagesPolicyHandlerTestCase test_case = GetParam();
    switch (test_case.handle_type) {
      case ScreensaverImagesPolicyHandler::HandlerType::kUser:
        RegisterUserWithUserPrefs(AccountId::FromUserEmail(kUserEmail),
                                  user_manager::UserType::kRegular);
        break;
      case ScreensaverImagesPolicyHandler::HandlerType::kSignin: {
        auto pref_service = std::make_unique<TestingPrefServiceSimple>();
        active_prefs_ = pref_service.get();
        RegisterSigninProfilePrefs(pref_service->registry(), /*country=*/"",
                                   /*for_test=*/true);
        TestSessionControllerClient* client = GetSessionControllerClient();
        client->SetSigninScreenPrefService(std::move(pref_service));
      } break;
      case ScreensaverImagesPolicyHandler::HandlerType::kManagedGuest:
        RegisterUserWithUserPrefs(AccountId::FromUserEmail(kUserEmail),
                                  user_manager::UserType::kPublicAccount);
        break;
    }
  }

  void CreateScreensaverImagesPolicyHandler() {
    ScreensaverImagesPolicyHandlerTestCase test_case = GetParam();
    switch (test_case.handle_type) {
      case ScreensaverImagesPolicyHandler::HandlerType::kUser:
        policy_handler_ = std::make_unique<ScreensaverImagesPolicyHandler>(
            active_prefs_, ScreensaverImagesPolicyHandler::HandlerType::kUser);
        break;
      case ScreensaverImagesPolicyHandler::HandlerType::kSignin: {
        policy_handler_ = std::make_unique<ScreensaverImagesPolicyHandler>(
            active_prefs_,
            ScreensaverImagesPolicyHandler::HandlerType::kSignin);

        VerifyPrivateImageDownloaderDownloadFolder(
            *policy_handler_,
            temp_dir_.GetPath().AppendASCII(kSigninDirectoryName));
      } break;
      case ScreensaverImagesPolicyHandler::HandlerType::kManagedGuest:
        policy_handler_ = std::make_unique<ScreensaverImagesPolicyHandler>(
            active_prefs_,
            ScreensaverImagesPolicyHandler::HandlerType::kManagedGuest);
        break;
    }
  }

  void ResetScreensaverImagesPolicyHandler() { policy_handler_.reset(); }

  base::FilePath GetExpectedFilePath(const std::string& url) {
    auto hash = base::SHA1Hash(base::as_byte_span(url));
    return temp_dir_.GetPath()
        .AppendASCII(GetParam().base_directory)
        .AppendASCII(base::HexEncode(hash) + kCacheFileExt);
  }
};

INSTANTIATE_TEST_SUITE_P(
    ScreensaverImagesPolicyHandlerForAnySessionTests,
    ScreensaverImagesPolicyHandlerForAnySessionTest,
    ::testing::ValuesIn<ScreensaverImagesPolicyHandlerTestCase>({
        {"Signin", ScreensaverImagesPolicyHandler::HandlerType::kSignin,
         kSigninDirectoryName},
        {"User", ScreensaverImagesPolicyHandler::HandlerType::kUser,
         kCacheDirectoryName},
        {"ManagedGuest",
         ScreensaverImagesPolicyHandler::HandlerType::kManagedGuest,
         kManagedGuestDirectoryName},
    }),
    [](const ::testing::TestParamInfo<
        ScreensaverImagesPolicyHandlerForAnySessionTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(ScreensaverImagesPolicyHandlerForAnySessionTest, DirectoryTest) {
  // Verify that the policy handler creates the new
  // image downloader instance.
  ASSERT_TRUE(GetPrivateImageDownloader(*policy_handler_));
  VerifyPrivateImageDownloaderDownloadFolder(
      *policy_handler_,
      temp_dir_.GetPath().AppendASCII(GetParam().base_directory));
}

TEST_P(ScreensaverImagesPolicyHandlerForAnySessionTest,
       ShouldRunCallbackIfImagesUpdated) {
  base::test::TestFuture<std::vector<base::FilePath>> test_future;
  policy_handler()->SetScreensaverImagesUpdatedCallback(
      test_future.GetRepeatingCallback<const std::vector<base::FilePath>&>());

  // Expect callbacks when images are downloaded.
  base::FilePath file_path1(kFakeFilePath1);
  {
    TriggerOnDownloadedImageListUpdated({file_path1});
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    ASSERT_EQ(1u, file_paths.size());
    EXPECT_EQ(file_path1, file_paths.front());
  }
  base::FilePath file_path2(kFakeFilePath2);
  {
    TriggerOnDownloadedImageListUpdated({file_path1, file_path2});
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    ASSERT_EQ(2u, file_paths.size());
    EXPECT_TRUE(base::Contains(file_paths, file_path1));
    EXPECT_TRUE(base::Contains(file_paths, file_path2));
  }

  EXPECT_FALSE(test_future.IsReady());
}

TEST_P(ScreensaverImagesPolicyHandlerForAnySessionTest, DownloadImagesTest) {
  base::test::TestFuture<std::vector<base::FilePath>> test_future;
  policy_handler()->SetScreensaverImagesUpdatedCallback(
      test_future.GetRepeatingCallback<const std::vector<base::FilePath>&>());

  ASSERT_NE(GetExpectedFilePath(kImageUrl1),
            GetExpectedFilePath(kImageUrl1Alt));

  base::Value::List image_urls;
  image_urls.Append(kImageUrl1);
  image_urls.Append(kImageUrl1Alt);
  image_urls.Append(kImageUrl2);

  // Fill the pref service to trigger the logic under test.
  active_prefs()->SetManagedPref(
      ambient::prefs::kAmbientModeManagedScreensaverImages, image_urls.Clone());

  // Verify that request 1 is resolved
  {
    url_loader_factory().AddResponse(image_urls[0].GetString(), kFileContents1);
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    ASSERT_EQ(1u, file_paths.size());
    EXPECT_EQ(GetExpectedFilePath(kImageUrl1), file_paths.front());
  }

  // Verify that request 2 resolves to the same file as request 1.
  {
    url_loader_factory().AddResponse(image_urls[1].GetString(), kFileContents1);
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    ASSERT_EQ(1u, file_paths.size());
    EXPECT_NE(GetExpectedFilePath(kImageUrl1Alt), file_paths.front());
  }

  // Verify that request 3 is resolved and both file paths are present.
  {
    url_loader_factory().AddResponse(image_urls[2].GetString(), kFileContents2);
    EXPECT_TRUE(test_future.Wait());
    std::vector<base::FilePath> file_paths = test_future.Take();
    EXPECT_EQ(2u, file_paths.size());
    EXPECT_TRUE(base::Contains(file_paths, GetExpectedFilePath(kImageUrl1)));
    EXPECT_TRUE(base::Contains(file_paths, GetExpectedFilePath(kImageUrl2)));
  }
}

TEST_P(ScreensaverImagesPolicyHandlerForAnySessionTest, VerifyPolicyLimit) {
  base::test::TestFuture<std::vector<base::FilePath>> test_future;
  policy_handler()->SetScreensaverImagesUpdatedCallback(
      test_future.GetRepeatingCallback<const std::vector<base::FilePath>&>());

  base::Value::List image_urls;
  // Append the same URL request `kMaxUrlsToProcessFromPolicy` times. This
  // should be the only URL that can be requested.
  for (size_t i = 0; i < kMaxUrlsToProcessFromPolicy; ++i) {
    image_urls.Append(kImageUrl1);
  }
  // Append a new URL that must be ignored.
  image_urls.Append(kImageUrl2);

  // Add both responses in the URL factory.
  url_loader_factory().AddResponse(image_urls[0].GetString(), kFileContents1);
  url_loader_factory().AddResponse(image_urls[1].GetString(), kFileContents2);

  // Fill the pref service to trigger the logic under test.
  active_prefs()->SetManagedPref(
      ambient::prefs::kAmbientModeManagedScreensaverImages, image_urls.Clone());

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

TEST_P(ScreensaverImagesPolicyHandlerForAnySessionTest,
       VerifyUnsetImageListPrefBehavior) {
  // Simulate a populated cache
  {
    base::CreateDirectory(GetExpectedFilePath(kImageUrl1).DirName());
    EXPECT_TRUE(
        base::WriteFile(GetExpectedFilePath(kImageUrl1), kFileContents1));
    EXPECT_TRUE(
        base::WriteFile(GetExpectedFilePath(kImageUrl2), kFileContents2));
  }
  // It is assumed that the image list pref is unset.

  // Case 1: The enabled policy is unset.
  // Expectation: The unset image list pref should not cause a cache cleanup.
  {
    CreateScreensaverImagesPolicyHandler();
    task_environment()->RunUntilIdle();
    EXPECT_TRUE(base::PathExists(GetExpectedFilePath(kImageUrl1)));
    EXPECT_TRUE(base::PathExists(GetExpectedFilePath(kImageUrl2)));
  }

  // Case 2: The enabled policy is set to true.
  // Expectation: The unset image list pref should not cause a cache cleanup.
  {
    ResetScreensaverImagesPolicyHandler();
    active_prefs()->SetManagedPref(
        ambient::prefs::kAmbientModeManagedScreensaverEnabled,
        base::Value(true));

    CreateScreensaverImagesPolicyHandler();
    task_environment()->RunUntilIdle();

    EXPECT_TRUE(base::PathExists(GetExpectedFilePath(kImageUrl1)));
    EXPECT_TRUE(base::PathExists(GetExpectedFilePath(kImageUrl2)));
  }

  // Case 3: The enabled policy is set to false.
  // Expectation: The unset image list pref should cause a cache cleanup.
  {
    ResetScreensaverImagesPolicyHandler();
    active_prefs()->SetManagedPref(
        ambient::prefs::kAmbientModeManagedScreensaverEnabled,
        base::Value(false));

    CreateScreensaverImagesPolicyHandler();
    task_environment()->RunUntilIdle();

    EXPECT_FALSE(base::PathExists(GetExpectedFilePath(kImageUrl1)));
    EXPECT_FALSE(base::PathExists(GetExpectedFilePath(kImageUrl2)));
  }
}
}  // namespace ash
