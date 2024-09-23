// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/autotest_ambient_api.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/repeating_test_future.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "url/gurl.h"

namespace ash {

constexpr int64_t kMaxFileSizeInBytes = 8 * 1024 * 1024;  // 8 MB

constexpr base::TimeDelta kTestPerTransitionTimeout = base::Seconds(5);

const char kTestEmail[] = "test@example.com";
constexpr char kCacheDirectoryName[] = "managed_screensaver";
constexpr char kSigninCacheDirectoryPath[] = "signin";
const char kTestLargeImage[] = "test_large.jpg";
const char kTestInvalidImage[] = "test_invalid.jpf";
const char kRedImageFileName[] = "chromeos/screensaver/red.jpg";
const char kGreenImageFileName[] = "chromeos/screensaver/green.jpg";
const char kBlueImageFileName[] = "chromeos/screensaver/blue.jpg";

enum class TestType { LockScreen, LoginScreen };
struct ManagedScreensaverBrowserTestCase {
  std::string test_name;
  TestType test_type;
};

class ManagedScreensaverBrowserTest : public LoginManagerTest {
 public:
  ManagedScreensaverBrowserTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndEnableFeature(
        ash::features::kAmbientModeManagedScreensaver);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  }
  ~ManagedScreensaverBrowserTest() override = default;

  // LoginManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    device_policy_.Build();
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        owner_key_util_);
    // Set the signing key in the owner key util
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    // Override FakeSessionManagerClient.
    SessionManagerClient::InitializeFakeInMemory();

    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_.GetBlob());

    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  void InitializeForLoginScreen() {
    SetDevicePolicyEnabled(true);

    // Set intervals to zero so that we don't rely on time during testing.
    SetDevicePolicyImageDisplayIntervalSeconds(0);
    SetDevicePolicyScreenIdleTimeoutSeconds(0);
  }

  void InitializeForLockScreen() {
    const auto& users = login_manager_mixin_.users();
    EXPECT_EQ(users.size(), 1u);
    // Required so that fake session manager can be initialized with the correct
    // policy blob.
    user_policy_mixin_.RequestPolicyUpdate();

    LoginUser(test_account_id_);
    screen_locker_ = std::make_unique<ScreenLockerTester>();
    screen_locker_->Lock();
    SetPolicyEnabled(true);

    // Set intervals to zero so that we don't rely on time during testing
    SetPolicyImageDisplayIntervalSeconds(0);
    SetPolicyScreenIdleTimeoutSeconds(0);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Allow failing policy fetch so that we don't shutdown the profile on
    // failure.
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUp() override {
    // Setup the HTTPS test server
    https_server_.ServeFilesFromDirectory(GetChromeTestDataDir());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &ManagedScreensaverBrowserTest::HandleRequest, base::Unretained(this)));

    ASSERT_TRUE(https_server_.InitializeAndListen());

    LoginManagerTest::SetUp();
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    https_server_.StartAcceptingConnections();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = https_server_.GetURL(request.relative_url);
    auto path = absolute_url.path();
    if (!path.ends_with(kTestLargeImage) &&
        !path.ends_with(kTestInvalidImage)) {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);

    if (path.ends_with(kTestLargeImage)) {
      http_response->set_content(std::string(kMaxFileSizeInBytes + 1, 'a'));
    }
    if (path.ends_with(kTestInvalidImage)) {
      http_response->set_content("invalid");
    }
    http_response->set_content_type("image/jpeg");

    return http_response;
  }

  void TearDownOnMainThread() override {
    screen_locker_.reset();
    LoginManagerTest::TearDownOnMainThread();
  }

  void SetDevicePolicyImages(const std::vector<std::string>& images) {
    enterprise_management::DeviceScreensaverLoginScreenImagesProto*
        mutable_images = device_policy_.payload()
                             .mutable_device_screensaver_login_screen_images();
    mutable_images->clear_device_screensaver_login_screen_images();
    for (const auto& image_path : images) {
      mutable_images->add_device_screensaver_login_screen_images(
          https_server_.GetURL("/" + image_path).spec());
    }
  }

  void SetDevicePolicyEnabled(bool enabled) {
    if (!enabled) {
      // Simulate policy-group guard by clearing other policies when the managed
      // screensaver policy is disabled.
      device_policy_.payload().Clear();
    }
    device_policy_.payload()
        .mutable_device_screensaver_login_screen_enabled()
        ->set_device_screensaver_login_screen_enabled(enabled);
  }

  void SetDevicePolicyImageDisplayIntervalSeconds(int64_t interval) {
    device_policy_.payload()
        .mutable_device_screensaver_login_screen_image_display_interval_seconds()
        ->set_device_screensaver_login_screen_image_display_interval_seconds(
            interval);
  }

  void SetDevicePolicyScreenIdleTimeoutSeconds(int64_t timeout) {
    device_policy_.payload()
        .mutable_device_screensaver_login_screen_idle_timeout_seconds()
        ->set_device_screensaver_login_screen_idle_timeout_seconds(timeout);
  }

  // Note: Waits for changes to policy preferences. Verifies that all
  // |policy_prefs| receive a policy update, fails the test otherwise.
  void RefreshDevicePolicyAndWait(
      const std::vector<std::string>& policy_prefs) {
    base::test::RepeatingTestFuture<void> test_future;
    PrefChangeRegistrar registar;
    registar.Init(
        Shell::Get()->session_controller()->GetSigninScreenPrefService());

    for (const std::string& path : policy_prefs) {
      registar.Add(path, test_future.GetCallback());
    }
    device_policy_.Build();
    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_.GetBlob());
    FakeSessionManagerClient::Get()->OnPropertyChangeComplete(
        /*success=*/true);
    for (size_t count = 0; count < policy_prefs.size(); count++) {
      ASSERT_TRUE(test_future.Wait())
          << "Timed out trying to wait for pref update";
      test_future.Take();
    }
  }

  void RefreshUserPolicyAndWait() {
    Profile* profile =
        ash::ProfileHelper::Get()->GetProfileByAccountId(test_account_id_);
    user_policy_test_helper_.RefreshPolicyAndWait(profile);
  }

  // Wait for the images to be downloaded to the screensaver directory.
  void WaitForImages(const std::vector<std::string>& images,
                     const base::FilePath& directory) {
    int64_t expected_directory_size = 0;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::FilePath test_data_dir;
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
      base::File::Info info;

      for (auto image_path : images) {
        base::FilePath temp_dir_path = test_data_dir;
        EXPECT_TRUE(GetFileInfo(temp_dir_path.AppendASCII(image_path), &info));
        expected_directory_size += info.size;
      }
    }

    // Wait until the download directory size is equal to the expected directory
    // size, which would indicate that all the images have been downloaded.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      base::ScopedAllowBlockingForTesting allow_blocking;
      return ComputeDirectorySize(directory) == expected_directory_size;
    }));
  }

  // TODO(b:280809373): Remove mutable subproto1 once policies are released.
  void SetPolicyImages(const std::vector<std::string>& images) {
    // Policy update variable should be explicitly declared here, otherwise it
    // might go out of scope immediately and cause multiple updates if we
    // inline the call in the loop.
    std::unique_ptr<ScopedUserPolicyUpdate> policy =
        user_policy_mixin_.RequestPolicyUpdate();

    auto* mutable_images = policy->policy_payload()
                               ->mutable_subproto1()
                               ->mutable_screensaverlockscreenimages();

    // Fake the update as set by policy
    mutable_images->mutable_policy_options()->set_mode(
        enterprise_management::PolicyOptions::MANDATORY);
    mutable_images->mutable_value()->mutable_entries()->Clear();
    for (const auto& image_path : images) {
      mutable_images->mutable_value()->mutable_entries()->Add(
          std::string(https_server_.GetURL("/" + image_path).spec()));
    }
  }

  void SetPolicyEnabled(bool enabled) {
    if (!enabled) {
      // Simulate policy-group guard by clearing other policies when the managed
      // screensaver policy is disabled.
      user_policy_mixin_.RequestPolicyUpdate()->policy_payload()->Clear();
    }
    user_policy_mixin_.RequestPolicyUpdate()
        ->policy_payload()
        ->mutable_subproto1()
        ->mutable_screensaverlockscreenenabled()
        ->set_value(enabled);
  }

  void SetPolicyImageDisplayIntervalSeconds(int64_t interval) {
    std::unique_ptr<ScopedUserPolicyUpdate> policy =
        user_policy_mixin_.RequestPolicyUpdate();
    policy->policy_payload()
        ->mutable_subproto1()
        ->mutable_screensaverlockscreenimagedisplayintervalseconds()
        ->set_value(interval);
  }

  void SetPolicyScreenIdleTimeoutSeconds(int64_t timeout) {
    std::unique_ptr<ScopedUserPolicyUpdate> policy =
        user_policy_mixin_.RequestPolicyUpdate();
    policy->policy_payload()
        ->mutable_subproto1()
        ->mutable_screensaverlockscreenidletimeoutseconds()
        ->set_value(timeout);
  }

  views::View* GetContainerView() {
    auto* widget =
        Shell::GetPrimaryRootWindowController()->ambient_widget_for_testing();

    if (widget) {
      auto* container_view = widget->GetContentsView();
      DCHECK(container_view &&
             container_view->GetID() == kAmbientContainerView);
      return container_view;
    }
    return nullptr;
  }

 protected:
  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestEmail,
                                     signin::GetTestGaiaIdForEmail(kTestEmail));

  const LoginManagerMixin::TestUserInfo managed_user_{test_account_id_};

  EmbeddedPolicyTestServerMixin policy_server_mixin_{&mixin_host_};

  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_account_id_,
                                     &policy_server_mixin_};

  std::unique_ptr<base::test::TestFuture<void>> test_future_;
  std::unique_ptr<ScreenLockerTester> screen_locker_;

  base::test::ScopedFeatureList feature_list_;
  policy::DevicePolicyBuilder device_policy_;
  policy::UserPolicyTestHelper user_policy_test_helper_{kTestEmail,
                                                        &policy_server_mixin_};
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  net::test_server::EmbeddedTestServer https_server_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_, {managed_user_}};
};

class ManagedScreensaverBrowserTestForAnyScreen
    : public ManagedScreensaverBrowserTest,
      public ::testing::WithParamInterface<ManagedScreensaverBrowserTestCase> {
 public:
  void Init() {
    const ManagedScreensaverBrowserTestCase test_case = GetParam();
    switch (test_case.test_type) {
      case TestType::LockScreen:
        ManagedScreensaverBrowserTest::InitializeForLockScreen();
        // Call refresh policy manually to not have multiple refresh calls
        // running at the same time.
        RefreshUserPolicyAndWait();
        return;
      case TestType::LoginScreen:
        ManagedScreensaverBrowserTest::InitializeForLoginScreen();
        RefreshDevicePolicyAndWait(
            {ambient::prefs::kAmbientModeManagedScreensaverEnabled,
             ambient::prefs::
                 kAmbientModeManagedScreensaverImageDisplayIntervalSeconds,
             ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds});
        return;
    }
    NOTREACHED_IN_MIGRATION();
  }

  void SetPolicy(bool enabled) {
    const ManagedScreensaverBrowserTestCase test_case = GetParam();
    switch (test_case.test_type) {
      case TestType::LockScreen:
        SetPolicyEnabled(enabled);
        RefreshUserPolicyAndWait();

        // Set intervals to zero so that we don't rely on time during testing.
        // This is needed as disabling the policy can unset other policy values.
        if (!enabled) {
          SetPolicyImageDisplayIntervalSeconds(0);
          SetPolicyScreenIdleTimeoutSeconds(0);
          RefreshUserPolicyAndWait();
        }
        return;
      case TestType::LoginScreen:
        SetDevicePolicyEnabled(enabled);
        RefreshDevicePolicyAndWait(
            {ambient::prefs::kAmbientModeManagedScreensaverEnabled});
        // Set intervals to zero so that we don't rely on time during testing.
        // This is needed as disabling the policy can unset other policy values.
        if (!enabled) {
          SetDevicePolicyImageDisplayIntervalSeconds(0);
          SetDevicePolicyScreenIdleTimeoutSeconds(0);
          RefreshDevicePolicyAndWait(
              {ambient::prefs::
                   kAmbientModeManagedScreensaverImageDisplayIntervalSeconds,
               ambient::prefs::
                   kAmbientModeManagedScreensaverIdleTimeoutSeconds});
        }
        return;
    }
    NOTREACHED_IN_MIGRATION();
  }

  void SetImages(const std::vector<std::string>& images,
                 bool wait_for_images = false) {
    const ManagedScreensaverBrowserTestCase test_case = GetParam();
    switch (test_case.test_type) {
      case TestType::LockScreen:
        SetPolicyImages(images);
        // Call refresh policy manually to not have multiple refresh calls
        // running at the same time.
        RefreshUserPolicyAndWait();
        break;
      case TestType::LoginScreen:
        SetDevicePolicyImages(images);
        RefreshDevicePolicyAndWait(
            {ambient::prefs::kAmbientModeManagedScreensaverImages});
        break;
    }
    if (wait_for_images) {
      WaitForImages(images, GetPolicyHandlerCachePath());
    }
  }

  base::FilePath GetPolicyHandlerCachePath() {
    const ManagedScreensaverBrowserTestCase test_case = GetParam();
    switch (test_case.test_type) {
      case TestType::LoginScreen:
        return base::PathService::CheckedGet(
                   ash::DIR_DEVICE_POLICY_SCREENSAVER_DATA)
            .AppendASCII(kSigninCacheDirectoryPath);
      case TestType::LockScreen:
        return base::PathService::CheckedGet(base::DIR_HOME)
            .AppendASCII(kCacheDirectoryName);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    ManagedScreensaverBrowserTestForAnyScreenTests,
    ManagedScreensaverBrowserTestForAnyScreen,
    ::testing::ValuesIn<ManagedScreensaverBrowserTestCase>({
        {"LoginScreen", TestType::LoginScreen},
        {"LockScreen", TestType::LockScreen},
    }),
    [](const ::testing::TestParamInfo<
        ManagedScreensaverBrowserTestForAnyScreen::ParamType>& info) {
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_P(ManagedScreensaverBrowserTestForAnyScreen, BasicTest) {
  Init();
  SetImages({kRedImageFileName, kBlueImageFileName, kGreenImageFileName},
            /*wait_for_images=*/true);
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  test_future_ = std::make_unique<base::test::TestFuture<void>>();
  AutotestAmbientApi test_api;
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/3, /*timeout=*/3 * kTestPerTransitionTimeout,
      /*on_complete=*/test_future_->GetCallback(),
      /*on_timeout=*/base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }));
  ASSERT_TRUE(test_future_->Wait());
  ASSERT_NE(nullptr, GetContainerView());

  // Confirm that setting the policy to disabled cleans up the images from the
  // filesystem.
  SetPolicy(/*enabled=*/false);

  test_future_ = std::make_unique<base::test::TestFuture<void>>();
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/1, /*timeout=*/kTestPerTransitionTimeout,
      /*on_complete=*/base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }),
      /*on_timeout=*/test_future_->GetCallback());
  ASSERT_TRUE(test_future_->Wait());
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_EQ(0, ComputeDirectorySize(GetPolicyHandlerCachePath()));
  }
  ASSERT_EQ(nullptr, GetContainerView());
}

IN_PROC_BROWSER_TEST_P(ManagedScreensaverBrowserTestForAnyScreen,
                       OneImageDoesNotStartAmbientMode) {
  Init();
  SetImages({kRedImageFileName}, /*wait_for_images=*/true);
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  AutotestAmbientApi test_api;
  test_future_ = std::make_unique<base::test::TestFuture<void>>();
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/1, /*timeout=*/kTestPerTransitionTimeout,
      /*on_complete=*/base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }),
      /*on_timeout=*/test_future_->GetCallback());
  ASSERT_TRUE(test_future_->Wait());

  ASSERT_EQ(nullptr, GetContainerView());
}

IN_PROC_BROWSER_TEST_P(ManagedScreensaverBrowserTestForAnyScreen,
                       ImageMoreThanMaxSizeNotDownloadedOrShown) {
  Init();
  SetImages({kTestLargeImage, kBlueImageFileName});
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  AutotestAmbientApi test_api;
  test_future_ = std::make_unique<base::test::TestFuture<void>>();
  // The large image will not even be downloaded and will fail to download.
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/1, /*timeout=*/kTestPerTransitionTimeout,
      /*on_complete=*/base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }),
      /*on_timeout=*/test_future_->GetCallback());
  ASSERT_TRUE(test_future_->Wait());
  ASSERT_EQ(nullptr, GetContainerView());
}

IN_PROC_BROWSER_TEST_P(ManagedScreensaverBrowserTestForAnyScreen,
                       InvalidImageDownloadedButNotShown) {
  Init();
  SetImages({kTestInvalidImage, kBlueImageFileName});
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  AutotestAmbientApi test_api;
  test_future_ = std::make_unique<base::test::TestFuture<void>>();
  // The invalid image is downloaded but the screensaver will not start up and
  // show images as the second image will fail to decode.
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/1, /*timeout=*/kTestPerTransitionTimeout,
      /*on_complete=*/base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }),
      /*on_timeout=*/test_future_->GetCallback());
  ASSERT_TRUE(test_future_->Wait());
  ASSERT_EQ(nullptr, GetContainerView());
}

IN_PROC_BROWSER_TEST_P(ManagedScreensaverBrowserTestForAnyScreen,
                       ClearingTheImagesStopsScreensaver) {
  Init();
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  SetImages({kRedImageFileName, kBlueImageFileName, kGreenImageFileName},
            /*wait_for_images=*/true);
  AutotestAmbientApi test_api;

  test_future_ = std::make_unique<base::test::TestFuture<void>>();
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/3, /*timeout=*/3 * kTestPerTransitionTimeout,
      /*on_complete=*/test_future_->GetCallback(),
      /*on_timeout=*/base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }));
  ASSERT_TRUE(test_future_->Wait());
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(ComputeDirectorySize(GetPolicyHandlerCachePath()) > 0);
  }
  ASSERT_NE(nullptr, GetContainerView());

  SetImages({});
  test_future_ = std::make_unique<base::test::TestFuture<void>>();
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/1, /*timeout=*/kTestPerTransitionTimeout,
      /*on_complete=*/base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }),
      /*on_timeout=*/test_future_->GetCallback());
  ASSERT_TRUE(test_future_->Wait());
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_EQ(0, ComputeDirectorySize(GetPolicyHandlerCachePath()));
  }
  ASSERT_EQ(nullptr, GetContainerView());
}

}  // namespace ash
