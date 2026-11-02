// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/accelerator_utils.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

TEST(AcceleratorUtilsAndroidTest,
     IsChromeAccelerator_UniversalKeyboardHandlingEnabled) {
  base::test::ScopedFeatureList feature_list(
      chrome::android::kUniversalKeyboardHandling);

  EXPECT_TRUE(
      IsChromeAccelerator(ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN)));
  EXPECT_TRUE(IsChromeAccelerator(ui::Accelerator(
      ui::VKEY_DELETE, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)));

  EXPECT_FALSE(IsChromeAccelerator(
      ui::Accelerator(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)));
}

TEST(AcceleratorUtilsAndroidTest,
     IsChromeAccelerator_UniversalKeyboardHandlingDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      chrome::android::kUniversalKeyboardHandling);

  EXPECT_TRUE(
      IsChromeAccelerator(ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN)));
  // Ctrl+Shift+ForwardDelete (VKEY_DELETE) is an alternate CLEAR_BROWSING_DATA
  // shortcut in KeyboardShortcuts.java (KEYCODE_FORWARD_DEL).
  EXPECT_TRUE(IsChromeAccelerator(ui::Accelerator(
      ui::VKEY_DELETE, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)));
  // Shift+Escape (IDC_TASK_MANAGER_SHORTCUT) is only in accelerator_table.cc,
  // not KeyboardShortcuts.java (which only maps Ctrl+Escape).
  EXPECT_FALSE(
      IsChromeAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN)));
  EXPECT_FALSE(IsChromeAccelerator(
      ui::Accelerator(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)));
}
