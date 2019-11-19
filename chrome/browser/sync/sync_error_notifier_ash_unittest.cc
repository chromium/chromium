// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_error_notifier_ash.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Notification ID corresponding to kProfileSyncNotificationId + the test
// profile's name.
const char kNotificationId[] = "chrome://settings/sync/testing_profile";

class FakeLoginUIService: public LoginUIService {
 public:
  FakeLoginUIService() : LoginUIService(nullptr) {}
  ~FakeLoginUIService() override = default;
};

class FakeLoginUI : public LoginUIService::LoginUI {
 public:
  FakeLoginUI() = default;
  ~FakeLoginUI() override = default;

  void FocusUI() override {}
};

std::unique_ptr<KeyedService> BuildFakeLoginUIService(
    content::BrowserContext* profile) {
  return std::make_unique<FakeLoginUIService>();
}

class SyncErrorNotifierTest : public BrowserWithTestWindowTest {
 public:
  SyncErrorNotifierTest() {}
  ~SyncErrorNotifierTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    FakeLoginUIService* login_ui_service = static_cast<FakeLoginUIService*>(
        LoginUIServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildFakeLoginUIService)));
    login_ui_service->SetLoginUI(&login_ui_);

    error_notifier_ = std::make_unique<SyncErrorNotifier>(&service_, profile());

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

  void TearDown() override {
    error_notifier_->Shutdown();

    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  // Utility function to test that SyncErrorNotifier behaves correctly for the
  // given error condition.
  void VerifySyncErrorNotifierResult(GoogleServiceAuthError::State error_state,
                                     bool is_signed_in,
                                     bool is_error,
                                     bool expected_notification) {
    service_.SetFirstSetupComplete(is_signed_in);
    service_.SetAuthError(GoogleServiceAuthError(error_state));
    ASSERT_EQ(is_error, sync_ui_util::ShouldShowPassphraseError(&service_));

    error_notifier_->OnStateChanged(&service_);

    base::Optional<message_center::Notification> notification =
        display_service_->GetNotification(kNotificationId);
    if (expected_notification) {
      ASSERT_TRUE(notification);
      EXPECT_FALSE(notification->title().empty());
      EXPECT_FALSE(notification->message().empty());
    } else {
      ASSERT_FALSE(notification);
    }
  }

  std::unique_ptr<SyncErrorNotifier> error_notifier_;
  syncer::TestSyncService service_;
  FakeLoginUI login_ui_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncErrorNotifierTest);
};

// Test that SyncErrorNotifier shows an notification if a passphrase is
// required.
TEST_F(SyncErrorNotifierTest, PassphraseNotification) {
  user_manager::ScopedUserManager scoped_enabler(
      std::make_unique<chromeos::MockUserManager>());
  ASSERT_FALSE(display_service_->GetNotification(kNotificationId));

  service_.SetPassphraseRequired(true);
  service_.SetPassphraseRequiredForPreferredDataTypes(true);
  {
    SCOPED_TRACE("Expected a notification for passphrase error");
    VerifySyncErrorNotifierResult(GoogleServiceAuthError::NONE,
                                  true /* signed in */, true /* error */,
                                  true /* expecting notification */);
  }

  // Simulate discarded notification and check that notification is not shown.
  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       kNotificationId, true /* by_user */);
  {
    SCOPED_TRACE("Not expecting notification, one was already discarded");
    VerifySyncErrorNotifierResult(GoogleServiceAuthError::NONE,
                                  true /* signed in */, true /* error */,
                                  false /* not expecting notification */);
  }

  // Check that no notification is shown if there is no error.
  service_.SetPassphraseRequired(false);
  service_.SetPassphraseRequiredForPreferredDataTypes(false);
  {
    SCOPED_TRACE("Not expecting notification since no error exists");
    VerifySyncErrorNotifierResult(GoogleServiceAuthError::NONE,
                                  true /* signed in */, false /* no error */,
                                  false /* not expecting notification */);
  }

  // Check that no notification is shown if sync setup is not completed.
  service_.SetPassphraseRequired(true);
  service_.SetPassphraseRequiredForPreferredDataTypes(true);
  {
    SCOPED_TRACE("Not expecting notification since sync setup is incomplete");
    VerifySyncErrorNotifierResult(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
        false /* not signed in */, false /* no error */,
        false /* not expecting notification */);
  }
}

}  // namespace
