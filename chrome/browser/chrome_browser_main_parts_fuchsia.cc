// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_parts_fuchsia.h"

#include "base/check.h"
#include "base/notreached.h"

ChromeBrowserMainPartsFuchsia::ChromeBrowserMainPartsFuchsia(
    const content::MainFunctionParams& parameters,
    StartupData* startup_data)
    : ChromeBrowserMainParts(parameters, startup_data) {}

void ChromeBrowserMainPartsFuchsia::ShowMissingLocaleMessageBox() {
  // Locale data should be bundled for all possible platform locales,
  // so crash here to make missing-locale states more visible.
  CHECK(false);
}
