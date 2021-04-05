// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using testing::UnorderedElementsAre;
using VmType = ash::VmCameraMicManager::VmType;
using DeviceType = ash::VmCameraMicManager::DeviceType;
using NotificationType = ash::VmCameraMicManager::NotificationType;

constexpr VmType kCrostiniVm = VmType::kCrostiniVm;
constexpr VmType kPluginVm = VmType::kPluginVm;

constexpr DeviceType kCamera = DeviceType::kCamera;
constexpr DeviceType kMic = DeviceType::kMic;

constexpr NotificationType kMicNotification =
    ash::VmCameraMicManager::kMicNotification;
constexpr NotificationType kCameraNotification =
    ash::VmCameraMicManager::kCameraNotification;
constexpr NotificationType kCameraAndMicNotification =
    ash::VmCameraMicManager::kCameraAndMicNotification;

constexpr auto kDebounceTime = ash::VmCameraMicManager::kDebounceTime;

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

template <typename T, typename V>
bool contains(const T& container, const V& value) {
  return std::find(container.begin(), container.end(), value) !=
         container.end();
}

}  // namespace

namespace ash {

class VmCameraMicManagerTest : public testing::Test {
 public:
  // Define here to access `VmCameraMicManager` private members.
  struct NotificationTestParam {
    ActiveMap active_map;
    std::set<std::string> expected_notifications;

    NotificationTestParam(
        const ActiveMap& active_map,
        const std::vector<std::pair<VmType, NotificationType>>&
            expected_notifications) {
      this->active_map = active_map;
      for (const auto& vm_notification : expected_notifications) {
        auto result = this->expected_notifications.insert(
            VmCameraMicManager::GetNotificationId(vm_notification.first,
                                                  vm_notification.second));
        CHECK(result.second);
      }
    }
  };

  std::string GetNotificationId(VmType vm, NotificationType type) {
    return VmCameraMicManager::GetNotificationId(vm, type);
  }

  VmCameraMicManagerTest() {
    // Make the profile the primary one.
    auto mock_user_manager =
        std::make_unique<testing::NiceMock<MockUserManager>>();
    mock_user_manager->AddUser(AccountId::FromUserEmailGaiaId(
        testing_profile_.GetProfileUserName(), "id"));
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(mock_user_manager));
    scoped_feature_list_.InitAndEnableFeature(
        features::kVmCameraMicIndicatorsAndNotifications);

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

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile testing_profile_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

  FakeNotificationDisplayService* fake_display_service_;
  std::unique_ptr<VmCameraMicManager> vm_camera_mic_manager_;

  DISALLOW_COPY_AND_ASSIGN(VmCameraMicManagerTest);
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

// Test `IsDeviceActive()` and `IsNotificationActive()`.
class VmCameraMicManagerIsActiveTest
    : public VmCameraMicManagerTest,
      public testing::WithParamInterface<IsActiveTestParam> {};

TEST_P(VmCameraMicManagerIsActiveTest, IsNotificationActive) {
  SetActiveAndForwardToStable(GetParam().active_map);

  for (auto device : {kCamera, kMic}) {
    EXPECT_EQ(vm_camera_mic_manager_->IsDeviceActive(device),
              contains(GetParam().device_expectations, device));
  }

  for (auto notification :
       {kCameraNotification, kMicNotification, kCameraAndMicNotification}) {
    EXPECT_EQ(vm_camera_mic_manager_->IsNotificationActive(notification),
              contains(GetParam().notification_expectations, notification));
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
            },
            /*device_expectations=*/{},
            /*notificatoin_expectations=*/{},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 0}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera},
            /*notificatoin_expectations=*/{kCameraNotification},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera},
            /*notificatoin_expectations=*/{kCameraNotification},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 0}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kMic},
            /*notificatoin_expectations=*/{kMicNotification},
        },
        // Only a crostini "camera icon" notification is displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notificatoin_expectations=*/{kCameraAndMicNotification},
        },
        // Crostini "camera icon" notification and pluginvm mic notification are
        // displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 1}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notificatoin_expectations=*/
            {kCameraAndMicNotification, kMicNotification},
        },
        // Crostini "camera icon" notification and pluginvm camera notification
        // are displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 0}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notificatoin_expectations=*/
            {kCameraAndMicNotification, kCameraNotification},
        },
        // Crostini camera notification and pluginvm mic notification are
        // displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 1}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notificatoin_expectations=*/
            {kCameraNotification, kMicNotification},
        },
        // Crostini and pluginvm "camera icon" notifications are displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 1}}},
            },
            /*device_expectations=*/{kCamera, kMic},
            /*notificatoin_expectations=*/{kCameraAndMicNotification},
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
            },
            /*expected_notifications=*/{},
        },
        {
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
            },
            /*expected_notifications=*/{{kCrostiniVm, kCameraNotification}},
        },
        {
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 1}}},
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
            },
            /*expected_notifications=*/
            {
                {kCrostiniVm, kCameraNotification},
                {kPluginVm, kCameraAndMicNotification},
            },
        },
    };
  }
};

TEST_P(VmCameraMicManagerNotificationTest, SetActiveAndForwardToStable) {
  const NotificationTestParam& param = GetParam();
  SetActiveAndForwardToStable(param.active_map);
  EXPECT_EQ(fake_display_service_->notification_ids(),
            param.expected_notifications);
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
  EXPECT_EQ(
      fake_display_service_->notification_ids(),
      std::set<std::string>{GetNotificationId(kPluginVm, kMicNotification)});

  ForwardToStable();
  EXPECT_EQ(
      fake_display_service_->notification_ids(),
      std::set<std::string>{GetNotificationId(kPluginVm, kMicNotification)});
}

TEST_F(VmCameraMicManagerDebounceTest, CombineCameraAndMic) {
  SetMicActive(kPluginVm, true);
  ForwardDebounceTime(/*factor=*/0.51);
  // Within debounce time, so no notification.
  EXPECT_EQ(fake_display_service_->notification_ids(), std::set<std::string>{});

  SetCameraAccessing(kPluginVm, true);
  ForwardDebounceTime(/*factor=*/0.51);
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
                GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  ForwardToStable();
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
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
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
                GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  // Will continue displaying for roughly kDebounceTime.
  ForwardDebounceTime(/*factor=*/0.9);
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
                GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  // Eventually display the "target" notification, which is no notification at
  // all.
  ForwardDebounceTime(/*factor=*/0.11);
  EXPECT_EQ(fake_display_service_->notification_ids(), std::set<std::string>{});

  ForwardToStable();
  EXPECT_EQ(fake_display_service_->notification_ids(), std::set<std::string>{});
}

// This test that within debounce time, if the state is reverted back to the
// stable status, then nothing will change.
TEST_F(VmCameraMicManagerDebounceTest, RevertBackToStable) {
  SetMicActive(kPluginVm, true);
  SetCameraAccessing(kPluginVm, true);
  ForwardToStable();
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
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
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
                GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  ForwardToStable();
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
                GetNotificationId(kPluginVm, kCameraAndMicNotification)});
}

// Simulate one of the more complicated case.
TEST_F(VmCameraMicManagerDebounceTest, SimulateSkypeStartingMeeting) {
  // Simulate the waiting to start screen, in which only the camera is active.
  SetCameraAccessing(kPluginVm, true);
  ForwardToStable();
  EXPECT_EQ(
      fake_display_service_->notification_ids(),
      std::set<std::string>{GetNotificationId(kPluginVm, kCameraNotification)});

  // Simulate what happens after clicking the starting button. Skype will turn
  // on and off the devices multiple time. We should expect the notification
  // only changes to "camera and mic" once.
  SetCameraAccessing(kPluginVm, false);
  ForwardDebounceTime(0.2);
  SetMicActive(kPluginVm, true);
  ForwardDebounceTime(0.2);
  SetCameraAccessing(kPluginVm, true);
  ForwardDebounceTime(0.7);
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
                GetNotificationId(kPluginVm, kCameraAndMicNotification)});
  ForwardToStable();
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
                GetNotificationId(kPluginVm, kCameraAndMicNotification)});

  SetCameraAccessing(kPluginVm, false);
  ForwardDebounceTime(0.2);
  SetCameraAccessing(kPluginVm, true);
  ForwardDebounceTime(0.9);
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
                GetNotificationId(kPluginVm, kCameraAndMicNotification)});
  ForwardToStable();
  EXPECT_EQ(fake_display_service_->notification_ids(),
            std::set<std::string>{
                GetNotificationId(kPluginVm, kCameraAndMicNotification)});
}

}  // namespace ash
