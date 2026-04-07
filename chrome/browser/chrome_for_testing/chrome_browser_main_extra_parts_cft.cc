// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_for_testing/chrome_browser_main_extra_parts_cft.h"

#include "chrome/browser/chrome_for_testing/config.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "components/headless/clipboard/headless_clipboard.h"

namespace chrome_for_testing {

ChromeBrowserMainExtraPartsCft::ChromeBrowserMainExtraPartsCft() = default;
ChromeBrowserMainExtraPartsCft::~ChromeBrowserMainExtraPartsCft() = default;

void ChromeBrowserMainExtraPartsCft::PreBrowserStart() {
  // Enable virtual clipboard if configured in Chrome for Testing, but only if
  // not running in headless mode (where it's always enabled by default).
  if (IsEnableVirtualClipboard() && !headless::IsHeadlessMode()) {
    headless::SetHeadlessClipboardForCurrentThread();
  }
}

}  // namespace chrome_for_testing
