// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/chrome_browser_main_extra_parts_mac.h"

#include "ui/display/screen.h"

ChromeBrowserMainExtraPartsMac::ChromeBrowserMainExtraPartsMac() = default;
ChromeBrowserMainExtraPartsMac::~ChromeBrowserMainExtraPartsMac() = default;

void ChromeBrowserMainExtraPartsMac::PreEarlyInitialization() {
  screen_ = std::make_unique<display::ScopedNativeScreen>();
}
