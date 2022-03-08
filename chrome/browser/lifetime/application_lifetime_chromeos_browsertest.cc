// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chrome {

class ApplicationLifetimeTest : public InProcessBrowserTest,
                                public BrowserListObserver {
 public:
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

 private:
  void OnBrowserClosing(Browser* browser) override {
    if (quits_on_browser_closing_)
      quits_on_browser_closing_->Quit();
  }

  absl::optional<base::RunLoop> quits_on_browser_closing_;
};

IN_PROC_BROWSER_TEST_F(ApplicationLifetimeTest,
                       AttemptRestartRestartsBrowserInPlace) {
  AttemptRestart();

  // Session Manager is not going to stop session.
  EXPECT_FALSE(IsAttemptingShutdown());
  auto* fake_session_manager_client = chromeos::FakeSessionManagerClient::Get();
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

IN_PROC_BROWSER_TEST_F(ApplicationLifetimeTest, AttemptRelaunchRelaunchesOs) {
  AttemptRelaunch();

  // Session Manager is not going to stop session.
  EXPECT_FALSE(IsAttemptingShutdown());
  auto* fake_session_manager_client = chromeos::FakeSessionManagerClient::Get();
  EXPECT_FALSE(fake_session_manager_client->session_stopped());

  // Reboot has been requested.
  auto* fake_power_manager_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_GE(fake_power_manager_client->num_request_restart_calls(), 1);

  // Restart flags are set.
  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsRestarting());

  WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(ApplicationLifetimeTest,
                       AttemptExitSendsStopRequestToSessionManager) {
  AttemptExit();

  // Session Manager has received stop session request.
  EXPECT_TRUE(IsAttemptingShutdown());
  auto* fake_session_manager_client = chromeos::FakeSessionManagerClient::Get();
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

}  // namespace chrome
