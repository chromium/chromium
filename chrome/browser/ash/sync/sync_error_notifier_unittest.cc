// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_error_notifier.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

class FakeLoginUI : public LoginUIService::LoginUI {
 public:
  FakeLoginUI() = default;
  ~FakeLoginUI() override = default;

  void FocusUI() override {}
};

class SyncErrorNotifierTest : public BrowserWithTestWindowTest {
 public:
  SyncErrorNotifierTest() = default;

  SyncErrorNotifierTest(const SyncErrorNotifierTest&) = delete;
  SyncErrorNotifierTest& operator=(const SyncErrorNotifierTest&) = delete;

  ~SyncErrorNotifierTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    LoginUIService* login_ui_service =
        LoginUIServiceFactory::GetForProfile(profile());
    login_ui_service->SetLoginUI(&login_ui_);

    error_notifier_ = std::make_unique<SyncErrorNotifier>(&service_, profile());
  }

  void TearDown() override {
    // Explicitly destroy the notifier to ensure it doesn't outlive the profile.
    error_notifier_->Shutdown();
    error_notifier_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  const std::string& GetNotificationId() const {
    return error_notifier_->GetNotificationIdForTesting();
  }

  const message_center::Notification* GetNotification() const {
    return message_center::MessageCenter::Get()->FindNotificationById(
        GetNotificationId());
  }

  void ExpectNotificationShown(bool expected_notification) {
    const message_center::Notification* notification = GetNotification();
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
};

TEST_F(SyncErrorNotifierTest, NoNotificationWhenNoPassphrase) {
  ASSERT_FALSE(service_.GetUserSettings()->IsPassphraseRequired());
  service_.SetInitialSyncFeatureSetupComplete(true);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(false);
}

TEST_F(SyncErrorNotifierTest,
       NotificationShownWhenSyncFeatureDisabledViaDashboard) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kReplaceSyncPromosWithSignInPromos);

  service_.SetSignedIn(signin::ConsentLevel::kSignin);
  service_.GetUserSettings()->SetSyncFeatureDisabledViaDashboard();
  service_.SetInitialSyncFeatureSetupComplete(true);

  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(true);
}

TEST_F(SyncErrorNotifierTest,
       NoNotificationWhenSyncFeatureDisabledViaDashboardWithoutFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  service_.SetSignedIn(signin::ConsentLevel::kSignin);
  service_.GetUserSettings()->SetSyncFeatureDisabledViaDashboard();
  service_.SetInitialSyncFeatureSetupComplete(true);

  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(false);
}

TEST_F(SyncErrorNotifierTest, NotificationShownWhenBrowserSyncEnabled) {
  service_.GetUserSettings()->SetPassphraseRequired();
  service_.SetInitialSyncFeatureSetupComplete(true);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(true);
}

TEST_F(SyncErrorNotifierTest, NotificationShownOnce) {
  service_.GetUserSettings()->SetPassphraseRequired();
  service_.SetInitialSyncFeatureSetupComplete(true);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(true);

  // Close the notification and verify it isn't shown again.
  message_center::MessageCenter::Get()->RemoveNotification(GetNotificationId(),
                                                           /*by_user=*/true);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(false);
}

TEST_F(SyncErrorNotifierTest, NotificationClickRedirectsToSyncSetupByDefault) {
  service_.GetUserSettings()->SetPassphraseRequired();
  service_.SetInitialSyncFeatureSetupComplete(true);

  EXPECT_EQ(SyncErrorNotifier::GetDestinationSubpage(&service_), "syncSetup");
}

TEST_F(
    SyncErrorNotifierTest,
    NotificationClickRedirectsToAccountSettingsWhenFlagEnabledAndNoSyncConsent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kReplaceSyncPromosWithSignInPromos);

  // Consent level kSignin (means HasSyncConsent is false).
  service_.SetSignedIn(signin::ConsentLevel::kSignin);
  service_.GetUserSettings()->SetPassphraseRequired();
  service_.SetInitialSyncFeatureSetupComplete(true);

  EXPECT_EQ(SyncErrorNotifier::GetDestinationSubpage(&service_), "account");
}

TEST_F(SyncErrorNotifierTest,
       NotificationClickRedirectsToSyncSetupWhenFlagEnabledAndHasSyncConsent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kReplaceSyncPromosWithSignInPromos);

  // Consent level kSync (means HasSyncConsent is true).
  service_.SetSignedIn(signin::ConsentLevel::kSync);
  service_.GetUserSettings()->SetPassphraseRequired();
  service_.SetInitialSyncFeatureSetupComplete(true);

  EXPECT_EQ(SyncErrorNotifier::GetDestinationSubpage(&service_), "syncSetup");
}

}  // namespace
}  // namespace ash
