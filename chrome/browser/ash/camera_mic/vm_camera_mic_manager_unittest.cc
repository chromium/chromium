// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_helper.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

using VmType = VmCameraMicManager::VmType;
using DeviceType = VmCameraMicManager::DeviceType;
using NotificationType = VmCameraMicManager::NotificationType;

constexpr VmType kCrostiniVm = VmType::kCrostiniVm;
constexpr VmType kPluginVm = VmType::kPluginVm;
constexpr VmType kBorealis = VmType::kBorealis;

constexpr DeviceType kCamera = DeviceType::kCamera;
constexpr DeviceType kMic = DeviceType::kMic;

constexpr NotificationType kMicNotification =
    VmCameraMicManager::kMicNotification;
constexpr NotificationType kCameraNotification =
    VmCameraMicManager::kCameraNotification;
constexpr NotificationType kCameraAndMicNotification =
    VmCameraMicManager::kCameraAndMicNotification;

constexpr auto kDebounceTime = VmCameraMicManager::kDebounceTime;

class FakeNotificationDisplayService : public NotificationDisplayService {
 public:
  FakeNotificationDisplayService() = default;
  ~FakeNotificationDisplayService() override = default;

  void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    notification_ids_.insert(notification.id());
  }

  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override {
    size_t count = notification_ids_.erase(notification_id);
    CHECK(count == 1);
  }

  void GetDisplayed(DisplayedNotificationsCallback callback) override {}
  void GetDisplayedForOrigin(const GURL& origin,
                             DisplayedNotificationsCallback callback) override {
  }
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  const std::set<std::string>& notification_ids() { return notification_ids_; }

 private:
  std::set<std::string> notification_ids_;
};

using DeviceActiveMap = base::flat_map<DeviceType, bool>;
using ActiveMap = base::flat_map<VmType, DeviceActiveMap>;

struct IsActiveTestParam {
  ActiveMap active_map;
  // Device types that are expected to be active.
  std::vector<DeviceType> device_expectations;
  // Notification types that are expected to be active.
  std::vector<NotificationType> notification_expectations;
};

// Check the visibility of privacy indicators in all displays.
void ExpectPrivacyIndicatorsVisible(bool visible) {
  for (RootWindowController* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    auto* view = root_window_controller->GetStatusAreaWidget()
                     ->notification_center_tray()
                     ->privacy_indicators_view();

    EXPECT_EQ(view->GetVisible(), visible);
  }
}

}  // namespace

class VmCameraMicManagerTest : public testing::Test {
 public:
  // Define here to access `VmCameraMicManager` private members.
  struct NotificationTestParam {
    ActiveMap active_map;
    std::vector<std::string> expected_notifications;

    NotificationTestParam(
        const ActiveMap& active_map,
        const std::vector<std::pair<VmType, NotificationType>>&
            expected_notifications) {
      this->active_map = active_map;
      for (const auto& vm_notification : expected_notifications) {
        this->expected_notifications.push_back(
            VmCameraMicManager::GetNotificationId(vm_notification.first,
                                                  vm_notification.second));
      }
    }
  };

  std::string GetNotificationId(VmType vm, NotificationType type) {
    return ash::kPrivacyIndicatorsNotificationIdPrefix +
           VmCameraMicManager::GetNotificationId(vm, type);
  }

  VmCameraMicManagerTest() {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    // Make the profile the primary one.
    fake_user_manager_->AddUser(AccountId::FromUserEmailGaiaId(
        testing_profile_.GetProfileUserName(), "id"));

    // Inject a fake notification display service.
    fake_display_service_ = static_cast<FakeNotificationDisplayService*>(
        NotificationDisplayServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &testing_profile_,
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<FakeNotificationDisplayService>();
                })));

    vm_camera_mic_manager_ = std::make_unique<VmCameraMicManager>();
    vm_camera_mic_manager_->OnPrimaryUserSessionStarted(&testing_profile_);
  }

  VmCameraMicManagerTest(const VmCameraMicManagerTest&) = delete;
  VmCameraMicManagerTest& operator=(const VmCameraMicManagerTest&) = delete;

  ~VmCameraMicManagerTest() override { fake_user_manager_.Reset(); }

  // testing::Test:
  void SetUp() override {
    // Setting ash prefs for testing multi-display.
    ash::RegisterLocalStatePrefs(local_state_.registry(), /*for_test=*/true);

    ash::AshTestHelper::InitParams params;
    params.local_state = &local_state_;
    ash_test_helper_.SetUp(std::move(params));
  }

  void TearDown() override { ash_test_helper_.TearDown(); }

  void SetCameraAccessing(VmType vm, bool value) {
    vm_camera_mic_manager_->SetCameraAccessing(vm, value);
  }

  void SetCameraPrivacyIsOn(bool value) {
    vm_camera_mic_manager_->SetCameraPrivacyIsOn(value);
  }

  void SetMicActive(VmType vm, bool value) {
    vm_camera_mic_manager_->SetMicActive(vm, value);
  }

  // Note that camera privacy is always set to off by this function.
  void SetActiveAndForwardToStable(const ActiveMap& active_map) {
    SetCameraPrivacyIsOn(false);
    for (const auto& vm_and_device_active_map : active_map) {
      VmType vm = vm_and_device_active_map.first;
      for (const auto& device_active : vm_and_device_active_map.second) {
        switch (device_active.first) {
          case kCamera:
            SetCameraAccessing(vm, device_active.second);
            break;
          case kMic:
            SetMicActive(vm, device_active.second);
            break;
        }
      }
    }

    ForwardToStable();
  }

  void ForwardToStable() {
    // The manager at most does the debounce twice (once for "stage" and once
    // for "target"), so waiting 2 times here is enough.
    for (size_t i = 0; i < 2; ++i) {
      task_environment_.FastForwardBy(kDebounceTime);
    }
  }

  // Checks that all the notifications from `notification_ids` exist as privacy
  // indicators notifications.
  void ExpectNotificationsExist(std::set<std::string> notification_ids) {
    std::set<std::string> all_privacy_indicators_notifications;
    for (message_center::Notification* notification :
         message_center::MessageCenter::Get()->GetVisibleNotifications()) {
      // Leaves out any parent container.
      if (!notification || notification->group_parent()) {
        continue;
      }

      auto& notification_id = notification->id();
      if (base::StartsWith(notification_id,
                           ash::kPrivacyIndicatorsNotificationIdPrefix)) {
        all_privacy_indicators_notifications.insert(notification_id);
      }
    }

    EXPECT_EQ(all_privacy_indicators_notifications, notification_ids);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  TestingProfile testing_profile_;

  raw_ptr<FakeNotificationDisplayService> fake_display_service_;
  std::unique_ptr<VmCameraMicManager> vm_camera_mic_manager_;

  base::test::ScopedFeatureList scoped_feature_list_;

  // Use this for testing multi-display.
  TestingPrefServiceSimple local_state_;

  ash::AshTestHelper ash_test_helper_;
};

TEST_F(VmCameraMicManagerTest, CameraPrivacy) {
  SetCameraAccessing(kPluginVm, false);
  SetCameraPrivacyIsOn(false);
  ForwardToStable();
  EXPECT_FALSE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_FALSE(
      vm_camera_mic_manager_->IsNotificationActive(kCameraNotification));

  SetCameraAccessing(kPluginVm, true);
  SetCameraPrivacyIsOn(false);
  ForwardToStable();
  EXPECT_TRUE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_TRUE(
      vm_camera_mic_manager_->IsNotificationActive(kCameraNotification));

  SetCameraAccessing(kPluginVm, false);
  SetCameraPrivacyIsOn(true);
  ForwardToStable();
  EXPECT_FALSE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_FALSE(
      vm_camera_mic_manager_->IsNotificationActive(kCameraNotification));

  SetCameraAccessing(kPluginVm, true);
  SetCameraPrivacyIsOn(true);
  ForwardToStable();
  EXPECT_FALSE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_FALSE(
      vm_camera_mic_manager_->IsNotificationActive(kCameraNotification));
}

TEST_F(VmCameraMicManagerTest, PrivacyIndicatorsNotification) {
  SetCameraAccessing(kPluginVm, false);
  SetCameraPrivacyIsOn(false);
  ForwardToStable();
  EXPECT_FALSE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_FALSE(
      vm_camera_mic_manager_->IsNotificationActive(kCameraNotification));
  ExpectNotificationsExist(std::set<std::string>{});

  SetCameraAccessing(kPluginVm, true);
  SetCameraPrivacyIsOn(false);
  ForwardToStable();
  EXPECT_TRUE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_TRUE(
      vm_camera_mic_manager_->IsNotificationActive(kCameraNotification));
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(VmType::kPluginVm, kCameraNotification)});
}

TEST_F(VmCameraMicManagerTest, PrivacyIndicatorsView) {
  // Make sure privacy indicators work on multiple displays.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .UpdateDisplay("800x700,801+0-800x700");

  SetCameraAccessing(kPluginVm, false);
  SetCameraPrivacyIsOn(false);
  ForwardToStable();
  ExpectPrivacyIndicatorsVisible(/*visible=*/false);

  SetCameraAccessing(kPluginVm, true);
  SetCameraPrivacyIsOn(false);
  ForwardToStable();
  ExpectPrivacyIndicatorsVisible(/*visible=*/true);

  // Switch back to not accessing, the indicator should not be visible.
  SetCameraAccessing(kPluginVm, false);
  SetCameraPrivacyIsOn(false);
  ForwardToStable();
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment_.FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  ExpectPrivacyIndicatorsVisible(/*visible=*/false);
}

// Test `IsDeviceActive()` and `IsNotificationActive()`.
class VmCameraMicManagerIsActiveTest
    : public VmCameraMicManagerTest,
      public testing::WithParamInterface<IsActiveTestParam> {};

TEST_P(VmCameraMicManagerIsActiveTest, IsNotificationActive) {
  SetActiveAndForwardToStable(GetParam().active_map);

  for (auto device : {kCamera, kMic}) {
    EXPECT_EQ(vm_camera_mic_manager_->IsDeviceActive(device),
              base::Contains(GetParam().device_expectations, device));
  }

  for (auto notification :
       {kCameraNotification, kMicNotification, kCameraAndMicNotification}) {
    EXPECT_EQ(
        vm_camera_mic_manager_->IsNotificationActive(notification),
        base::Contains(GetParam().notification_expectations, notification));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VmCameraMicManagerIsActiveTest,
    testing::Values(
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 0}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{},
            /*notification_expectations=*/{},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 0}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera},
            /*notification_expectations=*/{kCameraNotification},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera},
            /*notification_expectations=*/{kCameraNotification},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 0}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kMic},
            /*notification_expectations=*/{kMicNotification},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 0}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 1}}},
            },
            /*device_expectations=*/{kMic},
            /*notification_expectations=*/{kMicNotification},
        },
        // Only a crostini "camera icon" notification is displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notification_expectations=*/{kCameraAndMicNotification},
        },
        // Crostini "camera icon" notification and pluginvm mic notification are
        // displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 1}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notification_expectations=*/
            {kCameraAndMicNotification, kMicNotification},
        },
        // Crostini "camera icon" notification and pluginvm camera notification
        // are displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notification_expectations=*/
            {kCameraAndMicNotification, kCameraNotification},
        },
        // Crostini camera notification and pluginvm mic notification are
        // displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 1}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notification_expectations=*/
            {kCameraNotification, kMicNotification},
        },
        // Crostini camera notification and borealis mic notification are
        // displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 1}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notification_expectations=*/
            {kCameraNotification, kMicNotification},
        },
        // Crostini and pluginvm "camera icon" notifications are displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 1}}},
                {kBorealis, {{kCamera, 0}, {kMic, 1}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notification_expectations=*/
            {kCameraAndMicNotification, kMicNotification},
        }));

class VmCameraMicManagerNotificationTest
    : public VmCameraMicManagerTest,
      public testing::WithParamInterface<
          VmCameraMicManagerTest::NotificationTestParam> {
 public:
  static std::vector<NotificationTestParam> GetTestValues() {
    return {
        {
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 0}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*expected_notifications=*/{},
        },
        {
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*expected_notifications=*/{{kCrostiniVm, kCameraNotification}},
        },
        {
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 1}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*expected_notifications=*/
            {
                {kCrostiniVm, kCameraNotification},
                {kPluginVm, kMicNotification},
            },
        },
        {
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 1}}},
                {kBorealis, {{kCamera, 0}, {kMic, 0}}},
            },
            /*expected_notifications=*/
            {
                {kCrostiniVm, kCameraNotification},
                {kPluginVm, kCameraAndMicNotification},
            },
        },
        {
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 0}}},
                {kBorealis, {{kCamera, 0}, {kMic, 1}}},
            },
            /*expected_notifications=*/
            {
                {kCrostiniVm, kCameraAndMicNotification},
                {kPluginVm, kCameraNotification},
                {kBorealis, kMicNotification},
            },
        },
    };
  }
};

TEST_P(VmCameraMicManagerNotificationTest, SetActiveAndForwardToStable) {
  const NotificationTestParam& param = GetParam();
  SetActiveAndForwardToStable(param.active_map);

  auto expected_notifications = param.expected_notifications;

  for (auto& id : expected_notifications) {
    id = ash::kPrivacyIndicatorsNotificationIdPrefix + id;
  }

  ExpectNotificationsExist(std::set<std::string>(expected_notifications.begin(),
                                                 expected_notifications.end()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VmCameraMicManagerNotificationTest,
    testing::ValuesIn(VmCameraMicManagerNotificationTest::GetTestValues()));

// For testing the debounce behavior.
class VmCameraMicManagerDebounceTest : public VmCameraMicManagerTest {
 public:
  void ForwardDebounceTime(double factor = 1) {
    task_environment_.FastForwardBy(factor * kDebounceTime);
  }
};

TEST_F(VmCameraMicManagerDebounceTest, Simple) {
  SetMicActive(kPluginVm, true);
  ForwardDebounceTime();
  ExpectNotificationsExist(
      std::set<std::string>{GetNotificationId(kPluginVm, kMicNotification)});

  ForwardToStable();
  ExpectNotificationsExist(
      std::set<std::string>{GetNotificationId(kPluginVm, kMicNotification)});
}

TEST_F(VmCameraMicManagerDebounceTest, CombineCameraAndMic) {
  SetMicActive(kPluginVm, true);
  ForwardDebounceTime(/*factor=*/0.51);
  // Within debounce time, so no notification.
  ExpectNotificationsExist(std::set<std::string>{});

  SetCameraAccessing(kPluginVm, true);
  ForwardDebounceTime(/*factor=*/0.51);
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  ForwardToStable();
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});
}

// This test that if the devices are turned on and then immediately turned off,
// we will still show notifications for at least kDebounceTime.
TEST_F(VmCameraMicManagerDebounceTest, DisplayStageBeforeTarget) {
  SetMicActive(kPluginVm, true);
  SetMicActive(kPluginVm, false);
  SetCameraAccessing(kPluginVm, true);
  SetCameraAccessing(kPluginVm, false);

  ForwardDebounceTime();
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  // Will continue displaying for roughly kDebounceTime.
  ForwardDebounceTime(/*factor=*/0.9);
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  // Eventually display the "target" notification, which is no notification at
  // all.
  ForwardDebounceTime(/*factor=*/0.11);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment_.FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  ExpectNotificationsExist(std::set<std::string>{});

  ForwardToStable();
  ExpectNotificationsExist(std::set<std::string>{});
}

// This test that within debounce time, if the state is reverted back to the
// stable status, then nothing will change.
TEST_F(VmCameraMicManagerDebounceTest, RevertBackToStable) {
  SetMicActive(kPluginVm, true);
  SetCameraAccessing(kPluginVm, true);
  ForwardToStable();
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  // First turn both devices off, and then turn them back up. The state should
  // become stable again so nothing should changed.
  SetMicActive(kPluginVm, false);
  SetCameraAccessing(kPluginVm, false);
  ForwardDebounceTime(/*factor=*/0.5);
  SetMicActive(kPluginVm, true);
  SetCameraAccessing(kPluginVm, true);

  // Nothing changed.
  ForwardDebounceTime();
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  ForwardToStable();
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});
}

// Simulate one of the more complicated case.
TEST_F(VmCameraMicManagerDebounceTest, SimulateSkypeStartingMeeting) {
  // Simulate the waiting to start screen, in which only the camera is active.
  SetCameraAccessing(kPluginVm, true);
  ForwardToStable();
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment_.FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  ExpectNotificationsExist(
      std::set<std::string>{GetNotificationId(kPluginVm, kCameraNotification)});

  // Simulate what happens after clicking the starting button. Skype will turn
  // on and off the devices multiple time. We should expect the notification
  // only changes to "camera and mic" once.
  SetCameraAccessing(kPluginVm, false);
  ForwardDebounceTime(0.2);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment_.FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  SetMicActive(kPluginVm, true);
  ForwardDebounceTime(0.2);
  SetCameraAccessing(kPluginVm, true);
  ForwardDebounceTime(0.7);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment_.FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});
  ForwardToStable();
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  SetCameraAccessing(kPluginVm, false);
  ForwardDebounceTime(0.2);
  SetCameraAccessing(kPluginVm, true);
  ForwardDebounceTime(0.9);
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});
  ForwardToStable();
  ExpectNotificationsExist(std::set<std::string>{
      GetNotificationId(kPluginVm, kCameraAndMicNotification)});
}

}  // namespace ash
