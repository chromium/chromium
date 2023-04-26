// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/idle_service.mojom.h"
#include "chromeos/crosapi/mojom/power.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace {

// Verifies that all required Crosapi dependencies of the test are available.
bool ValidateCrosapi() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service) {
    return false;
  }

  if (!lacros_service->IsAvailable<crosapi::mojom::Power>()) {
    return false;
  }

  int interface_version = chromeos::LacrosService::Get()
                              ->GetInterfaceVersion<crosapi::mojom::Power>();
  if (interface_version <
      int(crosapi::mojom::Power::kReportActivityMinVersion)) {
    return false;
  }

  if (!lacros_service->IsAvailable<crosapi::mojom::IdleService>()) {
    return false;
  }

  return true;
}

class TestIdleInfoObserver : public crosapi::mojom::IdleInfoObserver {
 public:
  explicit TestIdleInfoObserver(base::RepeatingClosure callback)
      : callback_(callback) {}

  TestIdleInfoObserver(const TestIdleInfoObserver&) = delete;
  TestIdleInfoObserver& operator=(const TestIdleInfoObserver&) = delete;

  void Start() {
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::IdleService>()
        ->AddIdleInfoObserver(receiver_.BindNewPipeAndPassRemoteWithVersion());
  }

  base::TimeTicks GetLastUserActivityTime() { return last_activity_time_; }

 private:
  // crosapi::mojom::IdleInfoObserver:
  void OnIdleInfoChanged(crosapi::mojom::IdleInfoPtr info) override {
    last_activity_time_ = info->last_activity_time;
    callback_.Run();
  }

  base::RepeatingClosure callback_;
  base::TimeTicks last_activity_time_;
  mojo::Receiver<crosapi::mojom::IdleInfoObserver> receiver_{this};
};

class PowerApiLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  PowerApiLacrosBrowserTest() = default;

  PowerApiLacrosBrowserTest(const PowerApiLacrosBrowserTest&) = delete;
  PowerApiLacrosBrowserTest& operator=(const PowerApiLacrosBrowserTest&) =
      delete;
  ~PowerApiLacrosBrowserTest() override = default;
};

// Calling ReportActivity() will update the last user activity.
IN_PROC_BROWSER_TEST_F(PowerApiLacrosBrowserTest,
                       ReportActivityUpdatesLastUserActivity) {
  if (!ValidateCrosapi()) {
    return;
  }

  // Report user activity and remember the time before.
  base::TimeTicks initial_time = base::TimeTicks::Now();
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::Power>()
      .get()
      ->ReportActivity();

  // TestIdleInfoObserver will receive the last user activity when added via a
  // call to its OnIdleInfoChanged() method.
  base::RunLoop run_loop;
  TestIdleInfoObserver observer(run_loop.QuitClosure());
  observer.Start();
  run_loop.Run();

  // User activity was reported after calling ReportActivity().
  EXPECT_GT(observer.GetLastUserActivityTime(), initial_time);
}

// Checks that the reported last user activity does not update without a
// ReportActivity() call.
IN_PROC_BROWSER_TEST_F(PowerApiLacrosBrowserTest,
                       LastUserActivityNotUpdatedWithoutCall) {
  if (!ValidateCrosapi()) {
    return;
  }

  // Save initial time to check no activity was reported during the test.
  base::TimeTicks initial_time = base::TimeTicks::Now();

  // TestIdleInfoObserver will receive the last user activity when added via a
  // call to its OnIdleInfoChanged() method.
  base::RunLoop run_loop;
  TestIdleInfoObserver observer(run_loop.QuitClosure());
  observer.Start();
  run_loop.Run();

  // No user activity reported without calling ReportActivity().
  EXPECT_LT(observer.GetLastUserActivityTime(), initial_time);
}

}  // namespace
