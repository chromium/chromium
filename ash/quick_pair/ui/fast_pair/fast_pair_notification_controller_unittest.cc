// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_notification_controller.h"

#include "ash/services/quick_pair/fast_pair_data_parser.h"
#include "ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"

namespace {

const char16_t kTestDeviceName[] = u"Pixel Buds";
const char16_t kTestAppName[] = u"JBLTools";
const char16_t kTestEmail[] = u"testemail@gmail.com";
const char kFastPairErrorNotificationId[] =
    "cros_fast_pair_error_notification_id";
const char kFastPairDiscoveryGuestNotificationId[] =
    "cros_fast_pair_discovery_guest_notification_id";
const char kFastPairApplicationAvailableNotificationId[] =
    "cros_fast_pair_application_available_notification_id";
const char kFastPairApplicationInstalledNotificationId[] =
    "cros_fast_pair_application_installed_notification_id";
const char kFastPairDiscoveryUserNotificationId[] =
    "cros_fast_pair_discovery_user_notification_id";
const char kFastPairPairingNotificationId[] =
    "cros_fast_pair_pairing_notification_id";
const char kFastPairAssociateAccountNotificationId[] =
    "cros_fast_pair_associate_account_notification_id";
const char kFastPairDiscoverySubsequentNotificationId[] =
    "cros_fast_pair_discovery_subsequent_notification_id";

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;

  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;

  ~TestMessageCenter() override = default;

  // message_center::FakeMessageCenter:
  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    EXPECT_FALSE(notification_);
    notification_ = std::move(notification);
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    if (notification_)
      notification_->delegate()->Close(by_user);
  }

  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    if (notification_) {
      EXPECT_EQ(notification_->id(), id);
      return notification_.get();
    }
    return nullptr;
  }

  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(id, notification_->id());

    notification_->delegate()->Click(/*button_index=*/button_index,
                                     /*reply=*/absl::nullopt);
  }

 private:
  std::unique_ptr<message_center::Notification> notification_;
};

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairNotificationControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    fast_pair_notification_controller_ =
        std::make_unique<FastPairNotificationController>(&test_message_center_);
  }

  void TearDown() override {
    fast_pair_notification_controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  TestMessageCenter test_message_center_;
  std::unique_ptr<FastPairNotificationController>
      fast_pair_notification_controller_;
};

TEST_F(FastPairNotificationControllerTest,
       ShowErrorNotification_SettingsClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> launch_bluetooth_pairing_callback;
  EXPECT_CALL(launch_bluetooth_pairing_callback, Run).Times(1);

  fast_pair_notification_controller_->ShowErrorNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), launch_bluetooth_pairing_callback.Get(),
      on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairErrorNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowErrorNotification_RemovedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> launch_bluetooth_pairing_callback;
  EXPECT_CALL(on_close, Run).Times(1);
  EXPECT_CALL(launch_bluetooth_pairing_callback, Run).Times(0);

  fast_pair_notification_controller_->ShowErrorNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), launch_bluetooth_pairing_callback.Get(),
      on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairErrorNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest, ShowErrorNotification_RemovedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> launch_bluetooth_pairing_callback;
  EXPECT_CALL(on_close, Run).Times(1);
  EXPECT_CALL(launch_bluetooth_pairing_callback, Run).Times(0);

  fast_pair_notification_controller_->ShowErrorNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), launch_bluetooth_pairing_callback.Get(),
      on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairErrorNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowUserDiscoveryNotification_ConnectClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(1);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);

  fast_pair_notification_controller_->ShowUserDiscoveryNotification(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowUserDiscoveryNotification_LearnMoreClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(1);

  fast_pair_notification_controller_->ShowUserDiscoveryNotification(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowUserDiscoveryNotification_RemovedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowUserDiscoveryNotification(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowUserDiscoveryNotification_RemovedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowUserDiscoveryNotification(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowGuestDiscoveryNotification_ConnectClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(1);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);

  fast_pair_notification_controller_->ShowGuestDiscoveryNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowGuestDiscoveryNotification_LearnMoreClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(1);

  fast_pair_notification_controller_->ShowGuestDiscoveryNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowGuestDiscoveryNotification_RemovedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowGuestDiscoveryNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowGuestDiscoveryNotification_RemovedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowGuestDiscoveryNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowApplicationAvailableNotification_DownloadClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationAvailableNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_download_clicked;
  EXPECT_CALL(on_download_clicked, Run).Times(1);
  EXPECT_CALL(on_close, Run).Times(0);

  fast_pair_notification_controller_->ShowApplicationAvailableNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), on_download_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationAvailableNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairApplicationAvailableNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowApplicationAvailableNotification_RemovedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationAvailableNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_download_clicked;
  EXPECT_CALL(on_download_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowApplicationAvailableNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), on_download_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationAvailableNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairApplicationAvailableNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowApplicationAvailableNotification_RemovedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationAvailableNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_download_clicked;
  EXPECT_CALL(on_download_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowApplicationAvailableNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), on_download_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationAvailableNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairApplicationAvailableNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowApplicationInstalledNotification_SetupClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_setup_clicked;
  EXPECT_CALL(on_setup_clicked, Run).Times(1);
  EXPECT_CALL(on_close, Run).Times(0);

  fast_pair_notification_controller_->ShowApplicationInstalledNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), kTestAppName, on_setup_clicked.Get(),
      on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairApplicationInstalledNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowApplicationInstalledNotification_RemovedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_setup_clicked;
  EXPECT_CALL(on_setup_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowApplicationInstalledNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), kTestAppName, on_setup_clicked.Get(),
      on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairApplicationInstalledNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowApplicationInstalledNotification_RemovedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_setup_clicked;
  EXPECT_CALL(on_setup_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowApplicationInstalledNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), kTestAppName, on_setup_clicked.Get(),
      on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairApplicationInstalledNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest, ShowPairingNotification) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  fast_pair_notification_controller_->ShowPairingNotification(
      kTestDeviceName,
      /*device_image=*/gfx::Image(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));
}

TEST_F(FastPairNotificationControllerTest,
       ShowAssociateAccount_ConnectClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_save_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_save_clicked, Run).Times(1);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);

  fast_pair_notification_controller_->ShowAssociateAccount(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_save_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairAssociateAccountNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowAssociateAccount_LearnMoreClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_save_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_save_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(1);

  fast_pair_notification_controller_->ShowAssociateAccount(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_save_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairAssociateAccountNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest, ShowAssociateAccount_RemovedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_save_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_save_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowAssociateAccount(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_save_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest, ShowAssociateAccount_RemovedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_save_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_save_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowAssociateAccount(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_save_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowSubsequentDiscoveryNotification_ConnectClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(1);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);

  fast_pair_notification_controller_->ShowSubsequentDiscoveryNotification(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowSubsequentDiscoveryNotification_LearnMoreClicked) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(1);

  fast_pair_notification_controller_->ShowSubsequentDiscoveryNotification(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowSubsequentDiscoveryNotification_RemovedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowSubsequentDiscoveryNotification(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotificationControllerTest,
       ShowSubsequentDiscoveryNotification_RemovedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  base::MockCallback<base::OnceCallback<void(bool)>> on_close;
  base::MockCallback<base::RepeatingClosure> on_connect_clicked;
  base::MockCallback<base::RepeatingClosure> on_learn_more_clicked;
  EXPECT_CALL(on_connect_clicked, Run).Times(0);
  EXPECT_CALL(on_learn_more_clicked, Run).Times(0);
  EXPECT_CALL(on_close, Run).Times(1);

  fast_pair_notification_controller_->ShowSubsequentDiscoveryNotification(
      kTestDeviceName, kTestEmail,
      /*device_image=*/gfx::Image(), on_connect_clicked.Get(),
      on_learn_more_clicked.Get(), on_close.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.RemoveNotification(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash
