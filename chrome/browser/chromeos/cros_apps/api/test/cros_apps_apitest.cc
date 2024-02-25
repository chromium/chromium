// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/api/test/cros_apps_apitest.h"

#include "chromeos/constants/chromeos_features.h"

CrosAppsApiTest::CrosAppsApiTest() {
  scoped_feature_list_.InitAndEnableFeature(
      chromeos::features::kBlinkExtension);
}

CrosAppsApiTest::~CrosAppsApiTest() = default;
