// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_win.h"

#include "chrome/browser/active_use_util.h"

BrowserProcessPlatformPart::BrowserProcessPlatformPart() = default;
BrowserProcessPlatformPart::~BrowserProcessPlatformPart() = default;

void BrowserProcessPlatformPart::OnBrowserLaunch() {
  if constexpr (kShouldRecordActiveUse) {
    if (!did_run_updater_) {
      did_run_updater_.emplace();
    }
  }
}
