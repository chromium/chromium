// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/personalization_app/time_of_day_test_utils.h"

#include "ash/constants/ash_features.h"

namespace ash::personalization_app {

std::vector<base::test::FeatureRef> GetTimeOfDayFeatures() {
  return {features::kFeatureManagementTimeOfDayWallpaper,
          features::kFeatureManagementTimeOfDayScreenSaver};
}

}  // namespace ash::personalization_app
