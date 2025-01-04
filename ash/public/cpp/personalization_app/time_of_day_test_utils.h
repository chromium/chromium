// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PERSONALIZATION_APP_TIME_OF_DAY_TEST_UTILS_H_
#define ASH_PUBLIC_CPP_PERSONALIZATION_APP_TIME_OF_DAY_TEST_UTILS_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/test/scoped_feature_list.h"

namespace ash::personalization_app {

// Returns all features required for enabling or disabling time-of-day.
ASH_PUBLIC_EXPORT std::vector<base::test::FeatureRef> GetTimeOfDayFeatures();

}  // namespace ash::personalization_app

#endif  // ASH_PUBLIC_CPP_PERSONALIZATION_APP_TIME_OF_DAY_TEST_UTILS_H_
