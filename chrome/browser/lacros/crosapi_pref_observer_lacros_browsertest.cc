// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "content/public/test/browser_test.h"

using CrosapiPrefObserverLacrosBrowserTest = InProcessBrowserTest;

// Tests multiple observers reading ash::kAccessibilitySpokenFeedbackEnabled.
// TODO(crbug.com/1157314): Not safe to run with other test since this assumes
// the pref is false and does not change during test.
IN_PROC_BROWSER_TEST_F(CrosapiPrefObserverLacrosBrowserTest, Basics) {
  // Register an observer.
  bool value1 = true;
  base::RunLoop run_loop1;
  CrosapiPrefObserver observer1(
      crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
      base::BindLambdaForTesting([&](base::Value value) {
        value1 = value.GetBool();
        run_loop1.Quit();
      }));
  run_loop1.Run();
  EXPECT_FALSE(value1);

  // Additional observers are OK.
  bool value2 = true;
  base::RunLoop run_loop2;
  CrosapiPrefObserver observer2(
      crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
      base::BindLambdaForTesting([&](base::Value value) {
        value2 = value.GetBool();
        run_loop2.Quit();
      }));
  run_loop2.Run();
  EXPECT_FALSE(value2);

  // OnPrefChanged should fire callback.
  observer1.OnPrefChanged(base::Value(true));
  EXPECT_TRUE(value1);
  EXPECT_FALSE(value2);
}
