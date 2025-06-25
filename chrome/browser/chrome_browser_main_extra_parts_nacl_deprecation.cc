// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ppapi_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

// TODO(crbug.com/423859723): Remove this file.

BASE_FEATURE(kNaclAllow, "NaclAllow", base::FEATURE_DISABLED_BY_DEFAULT);

void ChromeBrowserMainExtraPartsNaclDeprecation::PostEarlyInitialization() {
}

void ChromeBrowserMainExtraPartsNaclDeprecation::PostMainMessageLoopRun() {
}

void ChromeBrowserMainExtraPartsNaclDeprecation::NaclAllowedChanged() {
}
