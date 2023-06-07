// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/crosapi_system_log_source.h"

#include "base/test/bind.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

class CrosapiSystemLogSourceTest : public InProcessBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    if (ash_starter_.HasLacrosArgument()) {
      ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
    }
  }

  void SetUpOnMainThread() override {
    if (ash_starter_.HasLacrosArgument()) {
      ash_starter_.StartLacros(this);
    }
  }

 protected:
  test::AshBrowserTestStarter ash_starter_;
};

IN_PROC_BROWSER_TEST_F(CrosapiSystemLogSourceTest, LacrosWaylandStateDump) {
  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }
  base::RunLoop loop;
  std::unique_ptr<system_logs::SystemLogsResponse> response;
  auto fetch_callback =
      [&](std::unique_ptr<system_logs::SystemLogsResponse> cb_response) {
        response = std::move(cb_response);
        loop.Quit();
      };

  system_logs::CrosapiSystemLogSource source;
  source.Fetch(base::BindLambdaForTesting(fetch_callback));
  loop.Run();
  EXPECT_EQ(1u, response->count("Lacros ozone-wayland-state"));
}
