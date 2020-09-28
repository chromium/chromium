// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/crostini/crostini_upgrade_available_notification.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_upgrade_available_notification.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace crostini {

class CrostiniUpgradeAvailableNotificationTest
    : public BrowserWithTestWindowTest {
 public:
  CrostiniUpgradeAvailableNotificationTest()
      : BrowserWithTestWindowTest(
            Browser::TYPE_NORMAL,
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  ~CrostiniUpgradeAvailableNotificationTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    chromeos::DBusThreadManager::Initialize();

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);
  }

  void TearDown() override {
    RunUntilIdle();
    display_service_.reset();
    BrowserWithTestWindowTest::TearDown();
    chromeos::DBusThreadManager::Shutdown();
  }

  chromeos::CrostiniUpgraderDialog* GetCrostiniUpgraderDialog() {
    auto url = GURL{chrome::kChromeUICrostiniUpgraderUrl};
    return static_cast<chromeos::CrostiniUpgraderDialog*>(
        chromeos::SystemWebDialogDelegate::FindInstance(url.spec()));
  }

  void SafelyCloseDialog() {
    auto* upgrader_dialog = GetCrostiniUpgraderDialog();

    if (!upgrader_dialog) {
      return;
    }

    // Now there should be enough WebUI hooked up to close properly.
    base::RunLoop run_loop;
    upgrader_dialog->SetDeletionClosureForTesting(run_loop.QuitClosure());
    upgrader_dialog->Close();
    run_loop.Run();
  }

  void ExpectDialog() {
    // A new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_TRUE(crostini_manager()->GetCrostiniDialogStatus(
        crostini::DialogType::UPGRADER));

    EXPECT_NE(nullptr, GetCrostiniUpgraderDialog());
  }

  void ExpectNoDialog() {
    // No new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_FALSE(crostini_manager()->GetCrostiniDialogStatus(
        crostini::DialogType::UPGRADER));
    // Our dialog has really been deleted.
    EXPECT_EQ(nullptr, GetCrostiniUpgraderDialog());
  }

  void DowngradeOSRelease() {
    vm_tools::cicerone::OsRelease os_release;
    os_release.set_id("debian");
    os_release.set_version_id("9");
    auto container_id = crostini::DefaultContainerId();
    crostini_manager()->SetContainerOsRelease(container_id, os_release);
  }

  crostini::CrostiniManager* crostini_manager() {
    return crostini::CrostiniManager::GetForProfile(profile());
  }

  void RunUntilIdle() { task_environment()->RunUntilIdle(); }

  base::Optional<message_center::Notification> GetNotification(std::string id) {
    return display_service_->GetNotification(id);
  }

 private:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  session_manager::SessionManager session_manager_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniUpgradeAvailableNotificationTest);
};

TEST_F(CrostiniUpgradeAvailableNotificationTest, ShowsWhenNotified) {
  base::HistogramTester histogram_tester;

  DowngradeOSRelease();

  base::RunLoop run_loop;
  auto notification = CrostiniUpgradeAvailableNotification::Show(
      profile(), run_loop.QuitClosure());

  ExpectNoDialog();

  // Wait for notification, press Upgrade
  ASSERT_TRUE(notification);
  notification->Get()->delegate()->Click(0, base::nullopt);
  run_loop.Run();

  // Dialog should show because we clicked button 0 (Upgrade).
  ExpectDialog();

  RunUntilIdle();
  // There should no longer be a button to click
  EXPECT_TRUE(notification->Get()->buttons().empty());

  SafelyCloseDialog();
  ExpectNoDialog();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeDialogEvent",
      static_cast<base::HistogramBase::Sample>(
          crostini::UpgradeDialogEvent::kDialogShown),
      1);

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeAvailable",
      static_cast<base::HistogramBase::Sample>(
          crostini::CrostiniUpgradeAvailableNotificationClosed::kUpgradeButton),
      1);
}

}  // namespace crostini
