// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

class ApplicationLifetimeTest : public InProcessBrowserTest,
                                public BrowserListObserver {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    fake_update_engine_client_ =
        ash::UpdateEngineClient::InitializeFakeForTest();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    BrowserList::AddObserver(this);
  }

  void TearDownOnMainThread() override {
    BrowserList::RemoveObserver(this);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void WaitForBrowserToClose() {
    quits_on_browser_closing_.emplace();
    quits_on_browser_closing_->Run();
  }

 protected:
  void FakePendingUpdate() {
    update_engine::StatusResult update_status;
    update_status.set_current_operation(
        update_engine::Operation::UPDATED_NEED_REBOOT);
    fake_update_engine_client_->set_default_status(update_status);

    // Shutdown code in termination_notification.cc is only run once.
    // Exit Chrome after reboot after update would have been sent to
    // update_engine. Otherwise the browsertest framework wont be able to stop
    // Chrome gracefully.
    fake_update_engine_client_->set_reboot_after_update_callback(
        base::BindOnce(&ExitIgnoreUnloadHandlers));
  }

  bool RequestedRebootAfterUpdate() {
    return fake_update_engine_client_->reboot_after_update_call_count() > 0;
  }

 private:
  void OnBrowserClosing(Browser* browser) override {
    if (quits_on_browser_closing_) {
      quits_on_browser_closing_->Quit();
    }
  }

  std::optional<base::RunLoop> quits_on_browser_closing_;
  raw_ptr<ash::FakeUpdateEngineClient, DanglingUntriaged>
      fake_update_engine_client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ApplicationLifetimeTest,
                       AttemptRestartRestartsBrowserInPlace) {
  AttemptRestart();

  // Session Manager is not going to stop session.
  EXPECT_FALSE(IsSendingStopRequestToSessionManager());
  auto* fake_session_manager_client = ash::FakeSessionManagerClient::Get();
  EXPECT_FALSE(fake_session_manager_client->session_stopped());

  // No reboot requested.
  auto* fake_power_manager_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(fake_power_manager_client->num_request_restart_calls(), 0);

  // Restart flags are set.
  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsRestarting());

  WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(ApplicationLifetimeTest,
                       AttemptRestartWithPendingUpdateReboots) {
  FakePendingUpdate();

  AttemptRestart();

  // Session Manager is not going to stop session.
  EXPECT_FALSE(IsSendingStopRequestToSessionManager());
  auto* fake_session_manager_client = ash::FakeSessionManagerClient::Get();
  EXPECT_FALSE(fake_session_manager_client->session_stopped());

  // No reboot requested via power manager.
  auto* fake_power_manager_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(fake_power_manager_client->num_request_restart_calls(), 0);

  // Reboot requested via update engine client.
  EXPECT_TRUE(RequestedRebootAfterUpdate());

  // Restart flags are set.
  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsRestarting());

  WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(ApplicationLifetimeTest, AttemptRelaunchRelaunchesOs) {
  AttemptRelaunch();

  // Session Manager is not going to stop session.
  EXPECT_FALSE(IsSendingStopRequestToSessionManager());
  auto* fake_session_manager_client = ash::FakeSessionManagerClient::Get();
  EXPECT_FALSE(fake_session_manager_client->session_stopped());

  // Reboot has been requested.
  auto* fake_power_manager_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_GE(fake_power_manager_client->num_request_restart_calls(), 1);

  // No restart flags set.
  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsRestarting());

  WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(ApplicationLifetimeTest,
                       AttemptExitSendsStopRequestToSessionManager) {
  AttemptExit();

  // Session Manager has received stop session request.
  EXPECT_TRUE(IsSendingStopRequestToSessionManager());
  auto* fake_session_manager_client = ash::FakeSessionManagerClient::Get();
  EXPECT_TRUE(fake_session_manager_client->session_stopped());

  // No reboot requested.
  auto* fake_power_manager_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(fake_power_manager_client->num_request_restart_calls(), 0);

  // No restart flags set.
  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsRestarting());

  WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(ApplicationLifetimeTest, RelaunchForUpdate) {
  FakePendingUpdate();
  RelaunchForUpdate();

  // Reboot requested via update engine client.
  EXPECT_TRUE(RequestedRebootAfterUpdate());

  WaitForBrowserToClose();
}

}  // namespace chrome
