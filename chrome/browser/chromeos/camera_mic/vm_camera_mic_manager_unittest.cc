// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/camera_mic/vm_camera_mic_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using testing::UnorderedElementsAre;
using VmType = chromeos::VmCameraMicManager::VmType;
using DeviceType = chromeos::VmCameraMicManager::DeviceType;

constexpr VmType kCrostiniVm = VmType::kCrostiniVm;
constexpr VmType kPluginVm = VmType::kPluginVm;

constexpr DeviceType kCamera = DeviceType::kCamera;
constexpr DeviceType kMic = DeviceType::kMic;

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
  DeviceActiveMap device_expectations;
  DeviceActiveMap notification_expectations;
};

}  // namespace

namespace chromeos {

class VmCameraMicManagerTest : public testing::Test {
 public:
  // Define here to access `VmCameraMicManager` private members.
  using NotificationType = VmCameraMicManager::NotificationType;
  static constexpr NotificationType kMicNotification =
      VmCameraMicManager::kMicNotification;
  static constexpr NotificationType kCameraNotification =
      VmCameraMicManager::kCameraNotification;
  static constexpr NotificationType kCameraWithMicNotification =
      VmCameraMicManager::kCameraWithMicNotification;
  struct NotificationTestParam {
    ActiveMap active_map;
    std::set<std::string> expected_notifications;

    NotificationTestParam(
        const ActiveMap& active_map,
        const std::vector<std::pair<VmType, NotificationType>>& notifications) {
      this->active_map = active_map;
      for (const auto& vm_notification : notifications) {
        auto result =
            expected_notifications.insert(VmCameraMicManager::GetNotificationId(
                vm_notification.first, vm_notification.second));
        CHECK(result.second);
      }
    }
  };

  using VmInfo = VmCameraMicManager::VmInfo;

  VmCameraMicManagerTest() {
    // Make the profile the primary one.
    auto mock_user_manager =
        std::make_unique<testing::NiceMock<chromeos::MockUserManager>>();
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
    vm_camera_mic_manager_->UpdateVmInfoAndNotifications(
        vm, &VmInfo::SetCameraAccessing, value);
  }

  void SetCameraPrivacyIsOn(VmType vm, bool value) {
    vm_camera_mic_manager_->UpdateVmInfoAndNotifications(
        vm, &VmInfo::SetCameraPrivacyIsOn, value);
  }

  void SetMicActive(VmType vm, bool value) {
    vm_camera_mic_manager_->UpdateVmInfoAndNotifications(
        vm, &VmInfo::SetMicActive, value);
  }

  // Note that camera privacy is always set to off by this function.
  void SetActive(const ActiveMap& active_map) {
    for (const auto& vm_and_device_active_map : active_map) {
      VmType vm = vm_and_device_active_map.first;
      SetCameraPrivacyIsOn(vm, false);
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
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

  FakeNotificationDisplayService* fake_display_service_;
  std::unique_ptr<VmCameraMicManager> vm_camera_mic_manager_;

  DISALLOW_COPY_AND_ASSIGN(VmCameraMicManagerTest);
};

TEST_F(VmCameraMicManagerTest, CameraPrivacy) {
  SetCameraAccessing(kPluginVm, false);
  SetCameraPrivacyIsOn(kPluginVm, false);
  EXPECT_FALSE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->IsNotificationActive(kCamera));

  SetCameraAccessing(kPluginVm, true);
  SetCameraPrivacyIsOn(kPluginVm, false);
  EXPECT_TRUE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_TRUE(vm_camera_mic_manager_->IsNotificationActive(kCamera));

  SetCameraAccessing(kPluginVm, false);
  SetCameraPrivacyIsOn(kPluginVm, true);
  EXPECT_FALSE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->IsNotificationActive(kCamera));

  SetCameraAccessing(kPluginVm, true);
  SetCameraPrivacyIsOn(kPluginVm, true);
  EXPECT_FALSE(vm_camera_mic_manager_->IsDeviceActive(kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->IsNotificationActive(kCamera));
}

// Test `IsDeviceActive()` and `IsNotificationActive()`.
class VmCameraMicManagerIsActiveTest
    : public VmCameraMicManagerTest,
      public testing::WithParamInterface<IsActiveTestParam> {};

TEST_P(VmCameraMicManagerIsActiveTest, IsNotificationActive) {
  SetActive(GetParam().active_map);

  for (const auto& device_and_expectation : GetParam().device_expectations) {
    EXPECT_EQ(
        vm_camera_mic_manager_->IsDeviceActive(device_and_expectation.first),
        device_and_expectation.second);
  }

  for (const auto& device_and_expectation :
       GetParam().notification_expectations) {
    EXPECT_EQ(vm_camera_mic_manager_->IsNotificationActive(
                  device_and_expectation.first),
              device_and_expectation.second);
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
            /*device_expectations=*/{{kCamera, 0}, {kMic, 0}},
            /*notificatoin_expectations=*/{{kCamera, 0}, {kMic, 0}},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 0}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 0}}},
            },
            /*device_expectations=*/{{kCamera, 1}, {kMic, 0}},
            /*notificatoin_expectations=*/{{kCamera, 1}, {kMic, 0}},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{{kCamera, 1}, {kMic, 0}},
            /*notificatoin_expectations=*/{{kCamera, 1}, {kMic, 0}},
        },
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 0}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{{kCamera, 0}, {kMic, 1}},
            /*notificatoin_expectations=*/{{kCamera, 0}, {kMic, 1}},
        },
        // Only a crostini "camera icon" notification is displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 0}}},
            },
            /*device_expectations=*/{{kCamera, 1}, {kMic, 1}},
            /*notificatoin_expectations=*/{{kCamera, 1}, {kMic, 0}},
        },
        // Crostini "camera icon" notification and pluginvm mic notification are
        // displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 1}}},
            },
            /*device_expectations=*/{{kCamera, 1}, {kMic, 1}},
            /*notificatoin_expectations=*/{{kCamera, 1}, {kMic, 1}},
        },
        // Crostini "camera icon" notification and pluginvm camera notification
        // are displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 0}}},
            },
            /*device_expectations=*/{{kCamera, 1}, {kMic, 1}},
            /*notificatoin_expectations=*/{{kCamera, 1}, {kMic, 0}},
        },
        // Crostini camera notification and pluginvm mic notification are
        // displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 0}}},
                {kPluginVm, {{kCamera, 0}, {kMic, 1}}},
            },
            /*device_expectations=*/{{kCamera, 1}, {kMic, 1}},
            /*notificatoin_expectations=*/{{kCamera, 1}, {kMic, 1}},
        },
        // Crostini and pluginvm "camera icon" notifications are displayed.
        IsActiveTestParam{
            /*active_map=*/{
                {kCrostiniVm, {{kCamera, 1}, {kMic, 1}}},
                {kPluginVm, {{kCamera, 1}, {kMic, 1}}},
            },
            /*device_expectations=*/{{kCamera, 1}, {kMic, 1}},
            /*notificatoin_expectations=*/{{kCamera, 1}, {kMic, 0}},
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
                {kPluginVm, kCameraWithMicNotification},
            },
        },
    };
  }
};

TEST_P(VmCameraMicManagerNotificationTest, SetActive) {
  const NotificationTestParam& param = GetParam();
  SetActive(param.active_map);
  EXPECT_EQ(fake_display_service_->notification_ids(),
            param.expected_notifications);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VmCameraMicManagerNotificationTest,
    testing::ValuesIn(VmCameraMicManagerNotificationTest::GetTestValues()));

}  // namespace chromeos
