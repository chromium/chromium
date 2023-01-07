// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/safe_browsing/safe_browsing_prefs.h"

namespace ntp {
namespace prefs {
const char kSafeBrowsingModuleShownCount[] =
    "safebrowsing.ntp.module_shown_count";
const char kSafeBrowsingModuleLastCooldownStartAt[] =
    "safebrowsing.ntp.last_cooldown_start_timestamp";
const char kSafeBrowsingModuleOpened[] = "safebrowsing.ntp.user_opened_module";
}  // namespace prefs
}  // namespace ntp
