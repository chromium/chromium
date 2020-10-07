// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/camera_mic/vm_camera_mic_manager.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/camera_mic/vm_camera_mic_manager_factory.h"
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

class FakeNotificationDisplayService : public NotificationDisplayService {
 public:
  FakeNotificationDisplayService() = default;
  ~FakeNotificationDisplayService() override = default;

  void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    notification_id_to_notifier_id_[notification.id()] =
        notification.notifier_id().id;
  }

  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override {
    size_t count = notification_id_to_notifier_id_.erase(notification_id);
    CHECK(count == 1);
  }

  void GetDisplayed(DisplayedNotificationsCallback callback) override {}
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  size_t CountNotifier(const std::string& notifier) {
    return std::count_if(
        notification_id_to_notifier_id_.begin(),
        notification_id_to_notifier_id_.end(),
        [&notifier](auto& pair) { return pair.second == notifier; });
  }

 private:
  std::map<std::string, std::string> notification_id_to_notifier_id_;
};

}  // namespace

namespace chromeos {

class VmCameraMicManagerTest : public testing::Test {
 public:
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

    vm_camera_mic_manager_ =
        VmCameraMicManagerFactory::GetForProfile(&testing_profile_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

  FakeNotificationDisplayService* fake_display_service_;
  VmCameraMicManager* vm_camera_mic_manager_;

  DISALLOW_COPY_AND_ASSIGN(VmCameraMicManagerTest);
};

TEST_F(VmCameraMicManagerTest, GetActive) {
  EXPECT_FALSE(vm_camera_mic_manager_->GetActive(VmType::kCrostiniVm,
                                                 DeviceType::kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->GetActive(VmType::kPluginVm,
                                                 DeviceType::kCamera));

  vm_camera_mic_manager_->SetActive(VmType::kCrostiniVm, DeviceType::kCamera,
                                    true);
  EXPECT_TRUE(vm_camera_mic_manager_->GetActive(VmType::kCrostiniVm,
                                                DeviceType::kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->GetActive(VmType::kPluginVm,
                                                 DeviceType::kCamera));

  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kCamera,
                                    true);
  EXPECT_TRUE(vm_camera_mic_manager_->GetActive(VmType::kCrostiniVm,
                                                DeviceType::kCamera));
  EXPECT_TRUE(vm_camera_mic_manager_->GetActive(VmType::kPluginVm,
                                                DeviceType::kCamera));

  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kCamera,
                                    false);
  EXPECT_TRUE(vm_camera_mic_manager_->GetActive(VmType::kCrostiniVm,
                                                DeviceType::kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->GetActive(VmType::kPluginVm,
                                                 DeviceType::kCamera));
}

TEST_F(VmCameraMicManagerTest, GetDeviceActive) {
  EXPECT_FALSE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kMic));

  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kCamera,
                                    true);
  EXPECT_TRUE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kMic));

  vm_camera_mic_manager_->SetActive(VmType::kCrostiniVm, DeviceType::kCamera,
                                    true);
  EXPECT_TRUE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kMic));

  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kCamera,
                                    false);
  EXPECT_TRUE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kMic));

  vm_camera_mic_manager_->SetActive(VmType::kCrostiniVm, DeviceType::kCamera,
                                    false);
  EXPECT_FALSE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kCamera));
  EXPECT_FALSE(vm_camera_mic_manager_->GetDeviceActive(DeviceType::kMic));
}

TEST_F(VmCameraMicManagerTest, SetActiveTriggerNotifications) {
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmCameraNotifierId), 0);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmMicNotifierId), 0);

  vm_camera_mic_manager_->SetActive(VmType::kCrostiniVm, DeviceType::kCamera,
                                    true);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmCameraNotifierId), 1);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmMicNotifierId), 0);

  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kCamera,
                                    true);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmCameraNotifierId), 2);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmMicNotifierId), 0);

  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kMic, true);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmCameraNotifierId), 2);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmMicNotifierId), 1);

  // No new notification for already active (VmType, DeviceType) combination.
  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kMic, true);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmCameraNotifierId), 2);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmMicNotifierId), 1);

  vm_camera_mic_manager_->SetActive(VmType::kCrostiniVm, DeviceType::kCamera,
                                    false);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmCameraNotifierId), 1);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmMicNotifierId), 1);

  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kMic, false);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmCameraNotifierId), 1);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmMicNotifierId), 0);

  // Ignore already inactive (VmType, DeviceType) combination.
  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kMic, false);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmCameraNotifierId), 1);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmMicNotifierId), 0);

  vm_camera_mic_manager_->SetActive(VmType::kPluginVm, DeviceType::kCamera,
                                    false);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmCameraNotifierId), 0);
  EXPECT_EQ(fake_display_service_->CountNotifier(ash::kVmMicNotifierId), 0);
}

}  // namespace chromeos
