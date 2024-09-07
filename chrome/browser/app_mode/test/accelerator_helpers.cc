// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_mode/test/accelerator_helpers.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

bool PressCloseTabAccelerator(Browser* browser) {
  // Ctrl + W.
  return BrowserView::GetBrowserViewForBrowser(browser)->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));
}

bool PressCloseWindowAccelerator(Browser* browser) {
  // Ctrl + Shift + W.
  return BrowserView::GetBrowserViewForBrowser(browser)->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
}

