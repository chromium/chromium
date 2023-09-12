// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cros_apps/api/cros_apps_api_browsertest_base.h"

#include "chromeos/constants/chromeos_features.h"
#include "content/public/common/content_switches.h"

CrosAppsApiBrowserTestBase::CrosAppsApiBrowserTestBase() {
  scoped_feature_list_.InitAndEnableFeature(chromeos::features::kCrosAppsApis);
}

CrosAppsApiBrowserTestBase::~CrosAppsApiBrowserTestBase() = default;

void CrosAppsApiBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  InProcessBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "BlinkExtensionChromeOS");
}
