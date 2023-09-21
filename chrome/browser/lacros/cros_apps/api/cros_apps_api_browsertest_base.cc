// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cros_apps/api/cros_apps_api_browsertest_base.h"

#include "chromeos/constants/chromeos_features.h"

CrosAppsApiBrowserTestBase::CrosAppsApiBrowserTestBase() {
  scoped_feature_list_.InitAndEnableFeature(
      chromeos::features::kBlinkExtension);
}

CrosAppsApiBrowserTestBase::~CrosAppsApiBrowserTestBase() = default;
