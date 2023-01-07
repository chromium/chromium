// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/lacros/field_trial_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

namespace {

class TestFieldTrialObserver : public FieldTrialObserver {
 public:
  using OnActivateCallback = base::RepeatingCallback<void()>;
  explicit TestFieldTrialObserver(OnActivateCallback callback)
      : callback_(callback) {}

 private:
  // crosapi::mojom::FieldTrialObserver:
  void OnFieldTrialGroupActivated(
      std::vector<crosapi::mojom::FieldTrialGroupInfoPtr> info) override {
    callback_.Run();
  }

  const OnActivateCallback callback_;
};

class FieldTrialServiceLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  FieldTrialServiceLacrosBrowserTest() = default;

  FieldTrialServiceLacrosBrowserTest(
      const FieldTrialServiceLacrosBrowserTest&) = delete;
  FieldTrialServiceLacrosBrowserTest& operator=(
      const FieldTrialServiceLacrosBrowserTest&) = delete;
  ~FieldTrialServiceLacrosBrowserTest() override = default;
};

// Smoke test for having ash-chrome send system idle info to lacros-chrome.
IN_PROC_BROWSER_TEST_F(FieldTrialServiceLacrosBrowserTest, Smoke) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);

  if (!lacros_service->IsAvailable<crosapi::mojom::FieldTrialService>()) {
    return;
  }

  bool activated = false;
  base::RunLoop run_loop;
  TestFieldTrialObserver observer(base::BindLambdaForTesting([&]() {
    // Expect this to be called.
    activated = true;
    run_loop.Quit();
  }));
  observer.Start();
  run_loop.Run();
  EXPECT_TRUE(activated);
}

}  // namespace
