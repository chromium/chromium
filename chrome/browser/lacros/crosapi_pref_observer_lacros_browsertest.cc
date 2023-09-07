// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest-spi.h"

using CrosapiPrefObserverLacrosBrowserTest = InProcessBrowserTest;

// Tests multiple observers reading ash::kAccessibilitySpokenFeedbackEnabled.
// TODO(crbug.com/1157314): Not safe to run with other test since this assumes
// the pref is false and does not change during test.
IN_PROC_BROWSER_TEST_F(CrosapiPrefObserverLacrosBrowserTest, Basics) {
  // Register an observer.
  base::test::RepeatingTestFuture<base::Value> pref_observer1_future;
  CrosapiPrefObserver observer1(
      crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
      pref_observer1_future.GetCallback());
  EXPECT_FALSE(pref_observer1_future.Take().GetBool());

  // Additional observers are OK.
  static base::test::RepeatingTestFuture<base::Value> pref_observer2_future;
  CrosapiPrefObserver observer2(
      crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
      pref_observer2_future.GetCallback());
  EXPECT_FALSE(pref_observer2_future.Take().GetBool());

  // OnPrefChanged should fire callback.
  observer1.OnPrefChanged(base::Value(true));
  EXPECT_TRUE(pref_observer1_future.Take().GetBool());

  // Browser tests use a `ScopedRunLoopTimeout` to automatically fail a test
  // when a timeout happens, so we use EXPECT_NONFATAL_FAILURE to handle it.
  // EXPECT_NONFATAL_FAILURE only works on static objects.
  static bool success = false;
  EXPECT_NONFATAL_FAILURE({ success = pref_observer2_future.Wait(); },
                          "timed out");
  EXPECT_FALSE(success);
}
