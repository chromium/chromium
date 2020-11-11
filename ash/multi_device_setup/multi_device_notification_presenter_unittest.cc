// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_device_setup/multi_device_notification_presenter.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/token.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

const char kTestUserEmail[] = "test@example.com";
const char kTestHostDeviceName[] = "Test Device";
// This is the expected return value from GetChromeOSDeviceName() in tests.
const char kTestDeviceType[] = "Chrome device";

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;
  ~TestMessageCenter() override = default;

  // message_center::FakeMessageCenter:
  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    EXPECT_FALSE(notification_);
    notification_ = std::move(notification);
  }

  void UpdateNotification(
      const std::string& id,
      std::unique_ptr<message_center::Notification> new_notification) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(notification_->id(), id);
    EXPECT_EQ(new_notification->id(), id);
    notification_ = std::move(new_notification);
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(notification_->id(), id);
    notification_.reset();
    for (auto& observer : observer_list())
      observer.OnNotificationRemoved(id, by_user);
  }

  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    if (notification_) {
      EXPECT_EQ(notification_->id(), id);
      return notification_.get();
    }
    return nullptr;
  }

  void ClickOnNotification(const std::string& id) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(id, notification_->id());
    for (auto& observer : observer_list())
      observer.OnNotificationClicked(id, base::nullopt, base::nullopt);
  }

 private:
  std::unique_ptr<message_center::Notification> notification_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageCenter);
};

}  // namespace

class MultiDeviceNotificationPresenterTest : public NoSessionAshTestBase {
 public:
  MultiDeviceNotificationPresenterTest() = default;

  void SetUp() override {
    fake_multidevice_setup_ =
        std::make_unique<chromeos::multidevice_setup::FakeMultiDeviceSetup>();
    auto delegate = std::make_unique<TestShellDelegate>();
    delegate->SetMultiDeviceSetupBinder(base::BindRepeating(
        &chromeos::multidevice_setup::MultiDeviceSetupBase::BindReceiver,
        base::Unretained(fake_multidevice_setup_.get())));
    NoSessionAshTestBase::SetUp(std::move(delegate));

    test_system_tray_client_ = GetSystemTrayClient();

    notification_presenter_ =
        std::make_unique<MultiDeviceNotificationPresenter>(
            &test_message_center_);
  }

  void TearDown() override {
    notification_presenter_.reset();
    NoSessionAshTestBase::TearDown();
  }

  void InvokePendingMojoCalls() { notification_presenter_->FlushForTesting(); }

  void SignIntoAccount() {
    TestSessionControllerClient* test_session_client =
        GetSessionControllerClient();
    test_session_client->AddUserSession(
        kTestUserEmail, user_manager::USER_TYPE_REGULAR,
        true /* provide_pref_service */, false /* is_new_profile */);
    test_session_client->SetSessionState(session_manager::SessionState::ACTIVE);
    test_session_client->SwitchActiveUser(
        AccountId::FromUserEmail(kTestUserEmail));

    InvokePendingMojoCalls();
    EXPECT_TRUE(fake_multidevice_setup_->delegate().is_bound());
  }

  void ShowNewUserNotification() {
    EXPECT_TRUE(fake_multidevice_setup_->delegate().is_bound());
    fake_multidevice_setup_->delegate()->OnPotentialHostExistsForNewUser();
    InvokePendingMojoCalls();
  }

  void TriggerNoLongerNewUserEvent() {
    EXPECT_TRUE(fake_multidevice_setup_->delegate().is_bound());
    fake_multidevice_setup_->delegate()->OnNoLongerNewUser();
    InvokePendingMojoCalls();
  }

  void ShowExistingUserHostSwitchedNotification() {
    EXPECT_TRUE(fake_multidevice_setup_->delegate().is_bound());
    fake_multidevice_setup_->delegate()->OnConnectedHostSwitchedForExistingUser(
        kTestHostDeviceName);
    InvokePendingMojoCalls();
  }

  void ShowExistingUserNewChromebookNotification() {
    EXPECT_TRUE(fake_multidevice_setup_->delegate().is_bound());
    fake_multidevice_setup_->delegate()->OnNewChromebookAddedForExistingUser(
        kTestHostDeviceName);
    InvokePendingMojoCalls();
  }

  void ClickNotification() {
    test_message_center_.ClickOnNotification(
        MultiDeviceNotificationPresenter::kNotificationId);
  }

  void DismissNotification(bool by_user) {
    test_message_center_.RemoveNotification(
        MultiDeviceNotificationPresenter::kNotificationId, by_user);
  }

  void VerifyNewUserPotentialHostExistsNotificationIsVisible() {
    VerifyNotificationIsVisible(
        MultiDeviceNotificationPresenter::Status::kNewUserNotificationVisible);
  }

  void VerifyExistingUserHostSwitchedNotificationIsVisible() {
    VerifyNotificationIsVisible(
        MultiDeviceNotificationPresenter::Status::
            kExistingUserHostSwitchedNotificationVisible);
  }

  void VerifyExistingUserNewChromebookAddedNotificationIsVisible() {
    VerifyNotificationIsVisible(
        MultiDeviceNotificationPresenter::Status::
            kExistingUserNewChromebookNotificationVisible);
  }

  void VerifyNoNotificationIsVisible() {
    EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
        MultiDeviceNotificationPresenter::kNotificationId));
  }

  void AssertPotentialHostBucketCount(std::string histogram, int count) {
    if (histogram_tester_.GetAllSamples(histogram).empty()) {
      EXPECT_EQ(count, 0);
      return;
    }
    histogram_tester_.ExpectBucketCount(
        histogram,
        MultiDeviceNotificationPresenter::
            kNotificationTypeNewUserPotentialHostExists,
        count);
  }

  void AssertHostSwitchedBucketCount(std::string histogram, int count) {
    if (histogram_tester_.GetAllSamples(histogram).empty()) {
      EXPECT_EQ(count, 0);
      return;
    }
    histogram_tester_.ExpectBucketCount(
        histogram,
        MultiDeviceNotificationPresenter::
            kNotificationTypeExistingUserHostSwitched,
        count);
  }

  void AssertNewChromebookBucketCount(std::string histogram, int count) {
    if (histogram_tester_.GetAllSamples(histogram).empty()) {
      EXPECT_EQ(count, 0);
      return;
    }
    histogram_tester_.ExpectBucketCount(
        histogram,
        MultiDeviceNotificationPresenter::
            kNotificationTypeExistingUserNewChromebookAdded,
        count);
  }

  base::HistogramTester histogram_tester_;
  TestSystemTrayClient* test_system_tray_client_;
  TestMessageCenter test_message_center_;
  std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetup>
      fake_multidevice_setup_;
  std::unique_ptr<MultiDeviceNotificationPresenter> notification_presenter_;

 private:
  void VerifyNotificationIsVisible(
      MultiDeviceNotificationPresenter::Status notification_status) {
    const message_center::Notification* kVisibleNotification =
        test_message_center_.FindVisibleNotificationById(
            MultiDeviceNotificationPresenter::kNotificationId);
    base::string16 title;
    base::string16 message;
    switch (notification_status) {
      case MultiDeviceNotificationPresenter::Status::
          kNewUserNotificationVisible:
        title = l10n_util::GetStringUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_TITLE);
        message = l10n_util::GetStringFUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_MESSAGE,
            base::ASCIIToUTF16(kTestDeviceType));
        break;
      case MultiDeviceNotificationPresenter::Status::
          kExistingUserHostSwitchedNotificationVisible:
        title = l10n_util::GetStringFUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_TITLE,
            base::ASCIIToUTF16(kTestHostDeviceName));
        message = l10n_util::GetStringFUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_MESSAGE,
            base::ASCIIToUTF16(kTestDeviceType));
        break;
      case MultiDeviceNotificationPresenter::Status::
          kExistingUserNewChromebookNotificationVisible:
        title = l10n_util::GetStringFUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROME_DEVICE_ADDED_TITLE,
            base::ASCIIToUTF16(kTestHostDeviceName));
        message = l10n_util::GetStringFUTF16(
            IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROME_DEVICE_ADDED_MESSAGE,
            base::ASCIIToUTF16(kTestDeviceType));
        break;
      case MultiDeviceNotificationPresenter::Status::kNoNotificationVisible:
        NOTREACHED();
    }
    EXPECT_EQ(title, kVisibleNotification->title());
    EXPECT_EQ(message, kVisibleNotification->message());
  }

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceNotificationPresenterTest);
};

TEST_F(MultiDeviceNotificationPresenterTest, NotSignedIntoAccount) {
  static const session_manager::SessionState kNonActiveStates[] = {
      session_manager::SessionState::UNKNOWN,
      session_manager::SessionState::OOBE,
      session_manager::SessionState::LOGIN_PRIMARY,
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE,
      session_manager::SessionState::LOCKED,
      session_manager::SessionState::LOGIN_SECONDARY};

  // For each of the states which is not ACTIVE, set the session state. None of
  // these should trigger a SetAccountStatusChangeDelegate() call.
  for (const auto state : kNonActiveStates) {
    GetSessionControllerClient()->SetSessionState(state);
    InvokePendingMojoCalls();
    EXPECT_FALSE(fake_multidevice_setup_->delegate().is_bound());
  }

  SignIntoAccount();
  EXPECT_TRUE(fake_multidevice_setup_->delegate().is_bound());
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostNewUserPotentialHostExistsNotification_RemoveProgrammatically) {
  SignIntoAccount();

  ShowNewUserNotification();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();

  notification_presenter_->RemoveMultiDeviceSetupNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_multi_device_setup_count(), 0);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationClicked", 0);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostNewUserPotentialHostExistsNotification_TapNotification) {
  SignIntoAccount();

  ShowNewUserNotification();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();

  ClickNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_multi_device_setup_count(), 1);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationClicked", 1);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostNewUserPotentialHostExistsNotification_DismissedNotification) {
  SignIntoAccount();

  ShowNewUserNotification();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();

  DismissNotification(true /* by_user */);
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_multi_device_setup_count(), 0);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationDismissed", 1);

  ShowNewUserNotification();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();

  DismissNotification(false /* by_user */);
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_multi_device_setup_count(), 0);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationDismissed", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest, TestNoLongerNewUserEvent) {
  SignIntoAccount();

  ShowNewUserNotification();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();

  TriggerNoLongerNewUserEvent();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_multi_device_setup_count(), 0);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationClicked", 0);
  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostExistingUserHostSwitchedNotification_RemoveProgrammatically) {
  SignIntoAccount();

  ShowExistingUserHostSwitchedNotification();
  VerifyExistingUserHostSwitchedNotificationIsVisible();

  notification_presenter_->RemoveMultiDeviceSetupNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_connected_devices_settings_count(),
            0);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationClicked", 0);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostExistingUserHostSwitchedNotification_TapNotification) {
  SignIntoAccount();

  ShowExistingUserHostSwitchedNotification();
  VerifyExistingUserHostSwitchedNotificationIsVisible();

  ClickNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_connected_devices_settings_count(),
            1);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationClicked", 1);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostExistingUserHostSwitchedNotification_DismissedNotification) {
  SignIntoAccount();

  ShowExistingUserHostSwitchedNotification();
  VerifyExistingUserHostSwitchedNotificationIsVisible();

  DismissNotification(true /* by_user */);
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_multi_device_setup_count(), 0);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationDismissed", 1);

  ShowExistingUserHostSwitchedNotification();
  VerifyExistingUserHostSwitchedNotificationIsVisible();

  DismissNotification(false /* by_user */);
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_multi_device_setup_count(), 0);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationDismissed", 1);
}

TEST_F(
    MultiDeviceNotificationPresenterTest,
    TestHostExistingUserNewChromebookAddedNotification_RemoveProgrammatically) {
  SignIntoAccount();

  ShowExistingUserNewChromebookNotification();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();

  notification_presenter_->RemoveMultiDeviceSetupNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_connected_devices_settings_count(),
            0);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationClicked", 0);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest,
       TestHostExistingUserNewChromebookAddedNotification_TapNotification) {
  SignIntoAccount();

  ShowExistingUserNewChromebookNotification();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();

  ClickNotification();
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_connected_devices_settings_count(),
            1);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationClicked", 1);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(
    MultiDeviceNotificationPresenterTest,
    TestHostExistingUserNewChromebookAddedNotification_DismissedNotification) {
  SignIntoAccount();

  ShowExistingUserNewChromebookNotification();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();

  DismissNotification(true /* by_user */);
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_multi_device_setup_count(), 0);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationDismissed", 1);

  ShowExistingUserNewChromebookNotification();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();

  DismissNotification(false /* by_user */);
  VerifyNoNotificationIsVisible();

  EXPECT_EQ(test_system_tray_client_->show_multi_device_setup_count(), 0);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationDismissed", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest, NotificationsReplaceOneAnother) {
  SignIntoAccount();

  ShowNewUserNotification();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();

  ShowExistingUserHostSwitchedNotification();
  VerifyExistingUserHostSwitchedNotificationIsVisible();

  ShowExistingUserNewChromebookNotification();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();

  ClickNotification();
  VerifyNoNotificationIsVisible();

  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationShown", 1);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationShown", 1);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationShown", 1);
}

TEST_F(MultiDeviceNotificationPresenterTest, NotificationsReplaceThemselves) {
  SignIntoAccount();

  ShowNewUserNotification();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();
  ShowNewUserNotification();
  VerifyNewUserPotentialHostExistsNotificationIsVisible();
  notification_presenter_->RemoveMultiDeviceSetupNotification();

  ShowExistingUserHostSwitchedNotification();
  VerifyExistingUserHostSwitchedNotificationIsVisible();
  ShowExistingUserHostSwitchedNotification();
  VerifyExistingUserHostSwitchedNotificationIsVisible();
  notification_presenter_->RemoveMultiDeviceSetupNotification();

  ShowExistingUserNewChromebookNotification();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();
  ShowExistingUserNewChromebookNotification();
  VerifyExistingUserNewChromebookAddedNotificationIsVisible();
  notification_presenter_->RemoveMultiDeviceSetupNotification();

  AssertPotentialHostBucketCount("MultiDeviceSetup_NotificationShown", 2);
  AssertHostSwitchedBucketCount("MultiDeviceSetup_NotificationShown", 2);
  AssertNewChromebookBucketCount("MultiDeviceSetup_NotificationShown", 2);
}

}  // namespace ash
