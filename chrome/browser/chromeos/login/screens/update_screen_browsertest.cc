// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_screen.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/version_updater/version_updater.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

const char kStubWifiGuid[] = "wlan0";

std::string GetDownloadingString(int status_resource_id) {
  return l10n_util::GetStringFUTF8(
      IDS_DOWNLOADING, l10n_util::GetStringUTF16(status_resource_id));
}

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

}  // namespace

class UpdateScreenTest : public MixinBasedInProcessBrowserTest {
 public:
  UpdateScreenTest() = default;
  ~UpdateScreenTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    fake_update_engine_client_ = new FakeUpdateEngineClient();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(fake_update_engine_client_));

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);

    tick_clock_.Advance(base::TimeDelta::FromMinutes(1));

    error_screen_ = GetOobeUI()->GetErrorScreen();
    update_screen_ = std::make_unique<UpdateScreen>(
        GetOobeUI()->GetView<UpdateScreenHandler>(), error_screen_,
        base::BindRepeating(&UpdateScreenTest::HandleScreenExit,
                            base::Unretained(this)));
    version_updater_ = update_screen_->GetVersionUpdaterForTesting();
    version_updater_->set_tick_clock_for_testing(&tick_clock_);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();

    update_screen_.reset();

    base::RunLoop run_loop;
    LoginDisplayHost::default_host()->Finalize(run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  void WaitForScreenResult() {
    if (last_screen_result_.has_value())
      return;

    base::RunLoop run_loop;
    screen_result_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

  std::unique_ptr<UpdateScreen> update_screen_;
  // Version updater - owned by |update_screen_|.
  VersionUpdater* version_updater_ = nullptr;
  // Error screen - owned by OobeUI.
  ErrorScreen* error_screen_ = nullptr;

  FakeUpdateEngineClient* fake_update_engine_client_ = nullptr;  // Unowned.

  base::SimpleTestTickClock tick_clock_;

  base::Optional<UpdateScreen::Result> last_screen_result_;

 private:
  void HandleScreenExit(UpdateScreen::Result result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = result;

    if (screen_result_callback_)
      std::move(screen_result_callback_).Run();
  }

  base::OnceClosure screen_result_callback_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreenTest);
};

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestUpdateCheckDoneBeforeShow) {
  update_screen_->Show();
  // For this test, the show timer is expected not to fire - cancel it
  // immediately.
  EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());
  update_screen_->GetShowTimerForTesting()->Stop();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::IDLE);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::IDLE);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());

  ASSERT_NE(GetOobeUI()->current_screen(), UpdateView::kScreenId);

  // Show another screen, and verify the Update screen in not shown before it.
  GetOobeUI()->GetView<NetworkScreenHandler>()->Show();
  OobeScreenWaiter network_screen_waiter(NetworkScreenView::kScreenId);
  network_screen_waiter.set_assert_next_screen();
  network_screen_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestUpdateNotFoundAfterScreenShow) {
  update_screen_->Show();
  EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::IDLE);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  update_screen_->GetShowTimerForTesting()->FireNow();

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update-md");
  test::OobeJS().ExpectVisiblePath(
      {"oobe-update-md", "checking-for-updates-dialog"});
  test::OobeJS().ExpectHiddenPath(
      {"oobe-update-md", "cellular-permission-dialog"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "updating-dialog"});

  status.set_current_operation(update_engine::Operation::IDLE);
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestUpdateAvailable) {
  update_screen_->set_ignore_update_deadlines_for_testing(true);
  update_screen_->Show();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  status.set_new_version("latest and greatest");
  status.set_new_size(1000000000);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  update_screen_->GetShowTimerForTesting()->FireNow();

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update-md");
  test::OobeJS().ExpectVisiblePath(
      {"oobe-update-md", "checking-for-updates-dialog"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "updating-dialog"});
  test::OobeJS().ExpectHiddenPath(
      {"oobe-update-md", "cellular-permission-dialog"});

  status.set_current_operation(update_engine::Operation::UPDATE_AVAILABLE);
  status.set_progress(0.0);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  status.set_progress(0.0);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  test::OobeJS()
      .CreateWaiter("!$('oobe-update-md').$$('#updating-dialog').hidden")
      ->Wait();
  test::OobeJS().ExpectHiddenPath(
      {"oobe-update-md", "checking-for-updates-dialog"});
  test::OobeJS().ExpectHiddenPath(
      {"oobe-update-md", "cellular-permission-dialog"});

  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "updating-progress"});
  test::OobeJS().ExpectEQ("$('oobe-update-md').$$('#updating-progress').value",
                          14);
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "estimated-time-left"});
  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "progress-message"});
  test::OobeJS().ExpectEQ(
      "$('oobe-update-md').$$('#progress-message').textContent.trim()",
      l10n_util::GetStringUTF8(IDS_INSTALLING_UPDATE));
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "update-complete-msg"});

  tick_clock_.Advance(base::TimeDelta::FromSeconds(60));
  status.set_progress(0.01);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "updating-progress"});
  test::OobeJS().ExpectEQ("$('oobe-update-md').$$('#updating-progress').value",
                          14);
  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "estimated-time-left"});
  test::OobeJS().ExpectEQ(
      "$('oobe-update-md').$$('#estimated-time-left').textContent.trim()",
      GetDownloadingString(IDS_DOWNLOADING_TIME_LEFT_LONG));
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "progress-message"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "update-complete-msg"});

  tick_clock_.Advance(base::TimeDelta::FromSeconds(60));
  status.set_progress(0.08);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "updating-progress"});
  test::OobeJS().ExpectEQ("$('oobe-update-md').$$('#updating-progress').value",
                          18);
  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "estimated-time-left"});
  test::OobeJS().ExpectEQ(
      "$('oobe-update-md').$$('#estimated-time-left').textContent.trim()",
      GetDownloadingString(IDS_DOWNLOADING_TIME_LEFT_STATUS_ONE_HOUR));
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "progress-message"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "update-complete-msg"});

  tick_clock_.Advance(base::TimeDelta::FromSeconds(10));
  status.set_progress(0.7);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "updating-progress"});
  test::OobeJS().ExpectEQ("$('oobe-update-md').$$('#updating-progress').value",
                          56);
  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "estimated-time-left"});
  test::OobeJS().ExpectEQ(
      "$('oobe-update-md').$$('#estimated-time-left').textContent.trim()",
      GetDownloadingString(IDS_DOWNLOADING_TIME_LEFT_SMALL));
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "progress-message"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "update-complete-msg"});

  tick_clock_.Advance(base::TimeDelta::FromSeconds(10));
  status.set_progress(0.9);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "updating-progress"});
  test::OobeJS().ExpectEQ("$('oobe-update-md').$$('#updating-progress').value",
                          68);
  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "estimated-time-left"});
  test::OobeJS().ExpectEQ(
      "$('oobe-update-md').$$('#estimated-time-left').textContent.trim()",
      GetDownloadingString(IDS_DOWNLOADING_TIME_LEFT_SMALL));
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "progress-message"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "update-complete-msg"});

  tick_clock_.Advance(base::TimeDelta::FromSeconds(10));
  status.set_current_operation(update_engine::Operation::VERIFYING);
  status.set_progress(1.0);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "updating-progress"});
  test::OobeJS().ExpectEQ("$('oobe-update-md').$$('#updating-progress').value",
                          74);
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "estimated-time-left"});
  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "progress-message"});
  test::OobeJS().ExpectEQ(
      "$('oobe-update-md').$$('#progress-message').textContent.trim()",
      l10n_util::GetStringUTF8(IDS_UPDATE_VERIFYING));
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "update-complete-msg"});

  tick_clock_.Advance(base::TimeDelta::FromSeconds(10));
  status.set_current_operation(update_engine::Operation::FINALIZING);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "updating-progress"});
  test::OobeJS().ExpectEQ("$('oobe-update-md').$$('#updating-progress').value",
                          81);
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "estimated-time-left"});
  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "progress-message"});
  test::OobeJS().ExpectEQ(
      "$('oobe-update-md').$$('#progress-message').textContent.trim()",
      l10n_util::GetStringUTF8(IDS_UPDATE_FINALIZING));
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "update-complete-msg"});

  tick_clock_.Advance(base::TimeDelta::FromSeconds(10));
  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "updating-progress"});
  test::OobeJS().ExpectEQ("$('oobe-update-md').$$('#updating-progress').value",
                          100);
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "estimated-time-left"});
  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "progress-message"});
  test::OobeJS().ExpectEQ(
      "$('oobe-update-md').$$('#progress-message').textContent.trim()",
      l10n_util::GetStringUTF8(IDS_UPDATE_FINALIZING));
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "update-complete-msg"});

  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, fake_update_engine_client_->reboot_after_update_call_count());

  // Simulate the situation where reboot does not happen in time.
  ASSERT_TRUE(version_updater_->GetRebootTimerForTesting()->IsRunning());
  version_updater_->GetRebootTimerForTesting()->FireNow();

  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "updating-progress"});
  test::OobeJS().ExpectEQ("$('oobe-update-md').$$('#updating-progress').value",
                          100);
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "estimated-time-left"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "progress-message"});
  test::OobeJS().ExpectVisiblePath({"oobe-update-md", "update-complete-msg"});
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorIssuingUpdateCheck) {
  fake_update_engine_client_->set_update_check_result(
      chromeos::UpdateEngineClient::UPDATE_RESULT_FAILED);
  update_screen_->Show();

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorCheckingForUpdate) {
  update_screen_->Show();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::ERROR);
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  fake_update_engine_client_->set_default_status(status);
  version_updater_->UpdateStatusChangedForTesting(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorUpdating) {
  update_screen_->Show();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::ERROR);
  status.set_new_version("latest and greatest");

  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestTemporaryPortalNetwork) {
  // Change ethernet state to offline.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);

  update_screen_->Show();

  // If the network is a captive portal network, error message is shown with a
  // delay.
  EXPECT_TRUE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());
  EXPECT_EQ(OobeScreen::SCREEN_UNKNOWN.AsId(),
            error_screen_->GetParentScreen());

  // If network goes back online, the error message timer should be canceled.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());

  status.set_current_operation(update_engine::Operation::UPDATE_AVAILABLE);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  // Verify that update screen is showing checking for update UI.
  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update-md");
  test::OobeJS().ExpectVisiblePath(
      {"oobe-update-md", "checking-for-updates-dialog"});
  test::OobeJS().ExpectHiddenPath(
      {"oobe-update-md", "cellular-permission-dialog"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "updating-dialog"});

  status.set_current_operation(update_engine::Operation::IDLE);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestTwoOfflineNetworks) {
  // Change ethernet state to portal.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  update_screen_->Show();

  // Update screen will delay error message about portal state because
  // ethernet is behind captive portal. Simulate the delay timing out.
  EXPECT_TRUE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());
  update_screen_->GetErrorMessageTimerForTesting()->FireNow();
  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());

  ASSERT_EQ(UpdateView::kScreenId.AsId(), error_screen_->GetParentScreen());

  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("error-message");
  test::OobeJS().ExpectVisible("error-message-md");
  test::OobeJS().ExpectTrue(
      "$('error-message').classList.contains('ui-state-update')");
  test::OobeJS().ExpectTrue(
      "$('error-message').classList.contains('error-state-portal')");

  // Change active network to the wifi behind proxy.
  network_portal_detector_.SetDefaultNetwork(
      kStubWifiGuid,
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED);

  test::OobeJS()
      .CreateWaiter(
          "$('error-message').classList.contains('error-state-proxy')")
      ->Wait();

  EXPECT_FALSE(last_screen_result_.has_value());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestVoidNetwork) {
  network_portal_detector_.SimulateNoNetwork();

  // First portal detection attempt returns NULL network and undefined
  // results, so detection is restarted.
  update_screen_->Show();

  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());

  network_portal_detector_.WaitForPortalDetectionRequest();
  network_portal_detector_.SimulateNoNetwork();

  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());
  ASSERT_EQ(UpdateView::kScreenId.AsId(), error_screen_->GetParentScreen());
  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  // Second portal detection also returns NULL network and undefined
  // results.  In this case, offline message should be displayed.
  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("error-message");
  test::OobeJS().ExpectVisible("error-message-md");
  test::OobeJS().ExpectTrue(
      "$('error-message').classList.contains('ui-state-update')");
  test::OobeJS().ExpectTrue(
      "$('error-message').classList.contains('error-state-offline')");

  EXPECT_FALSE(last_screen_result_.has_value());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestAPReselection) {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);

  update_screen_->Show();

  // Force timer expiration.
  EXPECT_TRUE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());
  update_screen_->GetErrorMessageTimerForTesting()->FireNow();
  ASSERT_EQ(UpdateView::kScreenId.AsId(), error_screen_->GetParentScreen());
  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      "fake_path", base::DoNothing(), base::DoNothing(),
      false /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);

  ASSERT_EQ(OobeScreen::SCREEN_UNKNOWN.AsId(),
            error_screen_->GetParentScreen());
  EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());
  update_screen_->GetShowTimerForTesting()->FireNow();

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  ASSERT_FALSE(last_screen_result_.has_value());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, UpdateOverCellularAccepted) {
  update_screen_->set_ignore_update_deadlines_for_testing(true);

  update_engine::StatusResult status;
  status.set_current_operation(
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE);
  status.set_new_version("latest and greatest");

  update_screen_->Show();

  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update-md");
  test::OobeJS().ExpectVisiblePath(
      {"oobe-update-md", "cellular-permission-dialog"});
  test::OobeJS().ExpectHiddenPath(
      {"oobe-update-md", "checking-for-updates-dialog"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "updating-dialog"});

  test::OobeJS().TapOnPath({"oobe-update-md", "cellular-permission-next"});

  test::OobeJS()
      .CreateWaiter("!$('oobe-update-md').$$('#updating-dialog').hidden")
      ->Wait();

  test::OobeJS().ExpectHiddenPath(
      {"oobe-update-md", "cellular-permission-dialog"});
  test::OobeJS().ExpectHiddenPath(
      {"oobe-update-md", "checking-for-updates-dialog"});

  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  version_updater_->UpdateStatusChangedForTesting(status);

  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, fake_update_engine_client_->reboot_after_update_call_count());
  ASSERT_FALSE(last_screen_result_.has_value());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, UpdateOverCellularRejected) {
  update_screen_->set_ignore_update_deadlines_for_testing(true);

  update_engine::StatusResult status;
  status.set_current_operation(
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE);
  status.set_new_version("latest and greatest");

  update_screen_->Show();

  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update-md");
  test::OobeJS().ExpectVisiblePath(
      {"oobe-update-md", "cellular-permission-dialog"});
  test::OobeJS().ExpectHiddenPath(
      {"oobe-update-md", "checking-for-updates-dialog"});
  test::OobeJS().ExpectHiddenPath({"oobe-update-md", "updating-dialog"});

  test::OobeJS().ClickOnPath({"oobe-update-md", "cellular-permission-back"});

  WaitForScreenResult();
  EXPECT_EQ(UpdateScreen::Result::UPDATE_ERROR, last_screen_result_.value());
}

}  // namespace chromeos
