// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_dlc_install_notification/arc_dlc_install_notification_manager.h"

#include <vector>

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace arc {

namespace {

class FakeArcDlcInstallNotificationDelegate
    : public ArcDlcInstallNotificationManager::Delegate {
 public:
  void DisplayNotification(
      const message_center::Notification& notification) override {
    displayed_notifications_.push_back(notification);
  }

  const std::vector<message_center::Notification>& displayed_notifications()
      const {
    return displayed_notifications_;
  }

 private:
  std::vector<message_center::Notification> displayed_notifications_;
};

class ArcDlcInstallNotificationManagerTest : public testing::Test {
 protected:
  ArcDlcInstallNotificationManagerTest() = default;
  ~ArcDlcInstallNotificationManagerTest() override = default;

  void SetUp() override {
    auto account_id = AccountId::FromUserEmailGaiaId("example.com", "123123");
    auto delegate = std::make_unique<FakeArcDlcInstallNotificationDelegate>();
    delegate_ = delegate.get();
    manager_ = std::make_unique<ArcDlcInstallNotificationManager>(
        std::move(delegate), account_id);
  }

  void TearDown() override {
    delegate_ = nullptr;
    manager_.reset();
  }

  ArcDlcInstallNotificationManager* manager() { return manager_.get(); }
  FakeArcDlcInstallNotificationDelegate* delegate() { return delegate_; }

 private:
  raw_ptr<content::BrowserContext> test_profile_ = nullptr;
  std::unique_ptr<ArcDlcInstallNotificationManager> manager_;
  raw_ptr<FakeArcDlcInstallNotificationDelegate> delegate_ = nullptr;
};

TEST_F(ArcDlcInstallNotificationManagerTest, DisplayNotification_Success) {
  manager()->Show(NotificationType::kArcVmPreloadSucceeded);

  const auto& notifications = delegate()->displayed_notifications();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(notifications[0].title(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_NOTIFICATION_TITLE));
  EXPECT_EQ(notifications[0].message(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_SUCCEEDED_MESSAGE));
}

TEST_F(ArcDlcInstallNotificationManagerTest, DisplayNotification_Failure) {
  manager()->Show(NotificationType::kArcVmPreloadFailed);

  const auto& notifications = delegate()->displayed_notifications();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(notifications[0].title(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_NOTIFICATION_TITLE));
  EXPECT_EQ(notifications[0].message(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_FAILED_MESSAGE));
}

TEST_F(ArcDlcInstallNotificationManagerTest,
       DisplayNotification_PreloadStarted) {
  manager()->Show(NotificationType::kArcVmPreloadStarted);

  const auto& notifications = delegate()->displayed_notifications();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(notifications[0].title(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_NOTIFICATION_TITLE));
  EXPECT_EQ(notifications[0].message(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_STARTED_MESSAGE));
}

}  // namespace
}  // namespace arc
