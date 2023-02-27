// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/multitask_menu_nudge_delegate_lacros.h"

#include "base/json/values_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/prefs.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

using MultitaskMenuNudgeDelegateLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MultitaskMenuNudgeDelegateLacrosBrowserTest, Basics) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::Prefs>());

  crosapi::mojom::PrefsAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>().get());

  absl::optional<base::Value> int_value;
  async_waiter.GetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount,
      &int_value);

  // If the pref cannot be fetched, the ash version may be too old.
  if (!int_value.has_value()) {
    GTEST_SKIP() << "Skipping as the nudge prefs are not available in the "
                    "current version of Ash";
  }

  base::Time expected_time = base::Time();
  async_waiter.SetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount,
      base::Value(2));
  async_waiter.SetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown,
      base::TimeToValue(expected_time));

  absl::optional<base::Value> time_value;
  async_waiter.GetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount,
      &int_value);
  async_waiter.GetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown,
      &time_value);
  ASSERT_TRUE(int_value.has_value());
  EXPECT_EQ(2, int_value.value());

  ASSERT_TRUE(time_value.has_value());
  EXPECT_EQ(expected_time, base::ValueToTime(*time_value).value());
}
