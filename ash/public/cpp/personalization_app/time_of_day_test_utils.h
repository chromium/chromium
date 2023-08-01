// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PERSONALIZATION_APP_TIME_OF_DAY_TEST_UTILS_H_
#define ASH_PUBLIC_CPP_PERSONALIZATION_APP_TIME_OF_DAY_TEST_UTILS_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/test/scoped_feature_list.h"

namespace ash::personalization_app {

// Returns all features required for enabling time-of-day. Note jelly is
// automatically included in the returned list because time-of-day depends on
// it. Enabling time-of-day without Jelly is unsupported and not a code path
// that should ever be taken in the real world.
ASH_PUBLIC_EXPORT std::vector<base::test::FeatureRef>
GetTimeOfDayEnabledFeatures();

// Returns all features required for disabling time-of-day. Note jelly is
// not included in the returned list because it is possible for jelly to be
// enabled without time-of-day some device models. If the caller wishes to
// disable jelly as well, they can append to the returned list themselves.
ASH_PUBLIC_EXPORT std::vector<base::test::FeatureRef>
GetTimeOfDayDisabledFeatures();

}  // namespace ash::personalization_app

#endif  // ASH_PUBLIC_CPP_PERSONALIZATION_APP_TIME_OF_DAY_TEST_UTILS_H_
