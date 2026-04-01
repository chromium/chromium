// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/frozen_update/frozen_update_notification.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

constexpr test::UIPath kAcceptConsolidatedConsentButton = {
    "consolidated-consent", "acceptButton"};
constexpr test::UIPath kConsolidatedConsentDialog = {"consolidated-consent",
                                                     "loadedDialog"};

// Text expected in the notification.
constexpr std::u16string_view kNotificationTitle = u"Final update";
constexpr std::u16string_view kNotificationMessage =
    u"Your ChromeOS Flex device will stop receiving updates "
    u"soon. Consider upgrading to a ChromeOS device.";

// Vendor and device PCIIDs which will see a notification.
constexpr int kVendor = 0x8086;
constexpr int kDevice = 0x2a42;

// Wait until the session is active, skipping consolidated consent if present.
void MaybeSkipConsolidatedConsent() {
  ASSERT_TRUE(base::test::RunUntil([]() {
    if (session_manager::SessionManager::Get()->session_state() ==
        session_manager::SessionState::ACTIVE) {
      return true;
    }

    auto* wizard_controller = WizardController::default_controller();
    if (wizard_controller && wizard_controller->current_screen()) {
      return wizard_controller->current_screen()->screen_id() ==
             ConsolidatedConsentScreenView::kScreenId;
    }

    return false;
  }));

  if (session_manager::SessionManager::Get()->session_state() ==
      session_manager::SessionState::ACTIVE) {
    return;
  }

  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kConsolidatedConsentDialog)
      ->Wait();
  test::OobeJS().TapOnPath(kAcceptConsolidatedConsentButton);
}

// Mixin that sets up session state to indicate certain Frozen Update status:
// It can override the GPU detected in the FrozenUpdateNotification
class OverrideGpuMixin : public InProcessBrowserTestMixin {
 public:
  explicit OverrideGpuMixin(InProcessBrowserTestMixinHost* mixin_host)
      : InProcessBrowserTestMixin(mixin_host) {}

  OverrideGpuMixin(const OverrideGpuMixin&) = delete;
  OverrideGpuMixin& operator=(const OverrideGpuMixin&) = delete;

  ~OverrideGpuMixin() override = default;

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTestMixin::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTestMixin::SetUpOnMainThread();
    UserSessionManager::GetInstance()
        ->SetFrozenUpdateNotificationHandlerFactoryForTesting(
            base::BindRepeating(
                &OverrideGpuMixin::CreateFrozenUpdateNotificationHandler,
                base::Unretained(this)));
  }
  void TearDownOnMainThread() override {
    UserSessionManager::GetInstance()
        ->SetFrozenUpdateNotificationHandlerFactoryForTesting(
            UserSessionManager::
                FrozenUpdateNotificationHandlerFactoryCallback());

    InProcessBrowserTestMixin::TearDownOnMainThread();
  }

  // Override the GPU that the device reports. Needs to be set before attempting
  // to display the notification.
  void OverrideGpu(int vendor, int device) {
    vendor_ = vendor;
    device_ = device;
  }

 private:
  std::unique_ptr<FrozenUpdateNotification>
  CreateFrozenUpdateNotificationHandler(PrefService& prefs) {
    auto frozen_update_notification =
        std::make_unique<FrozenUpdateNotification>(prefs);
    if (vendor_ && device_) {
      frozen_update_notification->OverrideGpuForTesting(*vendor_, *device_);
    }

    return frozen_update_notification;
  }

  std::optional<unsigned int> vendor_;
  std::optional<unsigned int> device_;
};

}  // namespace

// Tests that verify Frozen Update notifications for regular users on
// non-managed devices.
class FrozenUpdateNotificationTest : public MixinBasedInProcessBrowserTest {
 public:
  FrozenUpdateNotificationTest() {
    // Enable the frozen update
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kShowFrozenUpdateNotification);
  }

  // Only affects reven devices so force the switch.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kRevenBranding);
  }

 protected:
  OverrideGpuMixin override_gpu_mixin_{&mixin_host_};
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kConsumer};

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that verify Frozen Update notifications for users on
// managed devices are not displayed..
class ManagedDeviceFrozenUpdateNotificationTest
    : public MixinBasedInProcessBrowserTest {
 public:
  ManagedDeviceFrozenUpdateNotificationTest() {
    // Enable the frozen update
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kShowFrozenUpdateNotification);
  }

  // Only affects reven devices so force the switch.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kRevenBranding);
  }

 protected:
  OverrideGpuMixin override_gpu_mixin_{&mixin_host_};
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kManaged};

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that verify Frozen Update notifications are not shown to child users.
class ChildDeviceFrozenUpdateNotificationTest
    : public MixinBasedInProcessBrowserTest {
 public:
  ChildDeviceFrozenUpdateNotificationTest() {
    // Enable the frozen update
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kShowFrozenUpdateNotification);
  }

  // Only affects reven devices so force the switch.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kRevenBranding);
  }

 protected:
  OverrideGpuMixin override_gpu_mixin_{&mixin_host_};
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kChild};

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that verify Frozen Update notifications are not shown on non-reven
// devices.
class NotRevenFrozenUpdateNotificationTest
    : public MixinBasedInProcessBrowserTest {
 public:
  NotRevenFrozenUpdateNotificationTest() {
    // Enable the frozen update
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kShowFrozenUpdateNotification);
  }

 protected:
  OverrideGpuMixin override_gpu_mixin_{&mixin_host_};
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kConsumer};

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests showing the notification.
IN_PROC_BROWSER_TEST_F(FrozenUpdateNotificationTest, ShowNotification) {
  override_gpu_mixin_.OverrideGpu(kVendor, kDevice);
  logged_in_user_mixin_.LogInUser(
      {LoggedInUserMixin::LoginDetails::kDontWaitForSession});

  // Reven specific screen to skip
  MaybeSkipConsolidatedConsent();

  ASSERT_TRUE(base::test::RunUntil([]() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
               FrozenUpdateNotification::kFrozenUpdateNotificationId) !=
           nullptr;
  }));
  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          FrozenUpdateNotification::kFrozenUpdateNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(kNotificationTitle, notification->title());
  EXPECT_EQ(kNotificationMessage, notification->message());
}

// Tests that we don't show the notification if the GPU isn't on the list.
IN_PROC_BROWSER_TEST_F(FrozenUpdateNotificationTest,
                       NoGpuMatchResultsInNoNotification) {
  override_gpu_mixin_.OverrideGpu(0x1234, 0x5678);
  logged_in_user_mixin_.LogInUser(
      {LoggedInUserMixin::LoginDetails::kDontWaitForSession});

  // Reven specific screen to skip
  MaybeSkipConsolidatedConsent();

  ASSERT_TRUE(base::test::RunUntil([]() {
    return session_manager::SessionManager::Get()->session_state() ==
           session_manager::SessionState::ACTIVE;
  }));

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          FrozenUpdateNotification::kFrozenUpdateNotificationId);
  ASSERT_FALSE(notification);
}

// Test to confirm even with a valid GPU match we don't show notifications
// on managed devices.
IN_PROC_BROWSER_TEST_F(ManagedDeviceFrozenUpdateNotificationTest,
                       DoesNotShowNotificationEvenOnMatchingDevice) {
  override_gpu_mixin_.OverrideGpu(kVendor, kDevice);
  logged_in_user_mixin_.LogInUser(
      {LoggedInUserMixin::LoginDetails::kDontWaitForSession});

  ASSERT_TRUE(base::test::RunUntil([]() {
    return session_manager::SessionManager::Get()->session_state() ==
           session_manager::SessionState::ACTIVE;
  }));

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          FrozenUpdateNotification::kFrozenUpdateNotificationId);
  ASSERT_FALSE(notification);
}

// Test to confirm even with a valid GPU match we don't show notifications
// to child accounts.
IN_PROC_BROWSER_TEST_F(ChildDeviceFrozenUpdateNotificationTest,
                       DoesNotShowNotificationEvenOnMatchingDevice) {
  override_gpu_mixin_.OverrideGpu(kVendor, kDevice);
  logged_in_user_mixin_.LogInUser(
      {LoggedInUserMixin::LoginDetails::kDontWaitForSession});

  // Reven specific screen to skip
  MaybeSkipConsolidatedConsent();

  ASSERT_TRUE(base::test::RunUntil([]() {
    return session_manager::SessionManager::Get()->session_state() ==
           session_manager::SessionState::ACTIVE;
  }));

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          FrozenUpdateNotification::kFrozenUpdateNotificationId);
  ASSERT_FALSE(notification);
}

// Test to confirm even with a valid GPU match we don't show notifications
// on non-reven devices.
IN_PROC_BROWSER_TEST_F(NotRevenFrozenUpdateNotificationTest,
                       DoesNotShowNotificationEvenOnMatchingDevice) {
  override_gpu_mixin_.OverrideGpu(kVendor, kDevice);
  logged_in_user_mixin_.LogInUser(
      {LoggedInUserMixin::LoginDetails::kDontWaitForSession});

  ASSERT_TRUE(base::test::RunUntil([]() {
    return session_manager::SessionManager::Get()->session_state() ==
           session_manager::SessionState::ACTIVE;
  }));

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          FrozenUpdateNotification::kFrozenUpdateNotificationId);
  ASSERT_FALSE(notification);
}

}  // namespace ash
