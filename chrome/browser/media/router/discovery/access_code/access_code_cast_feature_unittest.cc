// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {
TEST(AccessCodeCastFeatureTest, GetAccessCodeCastEnabledPref) {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterBooleanPref(prefs::kAccessCodeCastEnabled,
                                                false);
  EXPECT_FALSE(GetAccessCodeCastEnabledPref(pref_service.get()));

  // Setting the pref to true should now return true.
  pref_service->SetManagedPref(prefs::kAccessCodeCastEnabled,
                               std::make_unique<base::Value>(true));
  EXPECT_TRUE(GetAccessCodeCastEnabledPref(pref_service.get()));

  // Removing the set value should now return the default value (false).
  pref_service->RemoveManagedPref(prefs::kAccessCodeCastEnabled);
  EXPECT_FALSE(GetAccessCodeCastEnabledPref(pref_service.get()));
}

TEST(AccessCodeCastFeatureTest, GetAccessCodeDeviceDurationPref) {
  const int non_default = 10;
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterBooleanPref(prefs::kAccessCodeCastEnabled,
                                                false);
  pref_service->registry()->RegisterIntegerPref(
      prefs::kAccessCodeCastDeviceDuration, 0);

  pref_service->SetManagedPref(prefs::kAccessCodeCastEnabled,
                               std::make_unique<base::Value>(true));

  // Defaults to 0.
  EXPECT_EQ(base::Seconds(0),
            GetAccessCodeDeviceDurationPref(pref_service.get()));

  // Setting to a non-zero value should cause the return value to match.
  pref_service->SetManagedPref(prefs::kAccessCodeCastDeviceDuration,
                               std::make_unique<base::Value>(non_default));
  EXPECT_EQ(base::Seconds(non_default),
            GetAccessCodeDeviceDurationPref(pref_service.get()));

  // Disabling the feature overall in policy now makes this return 0.
  pref_service->SetManagedPref(prefs::kAccessCodeCastEnabled,
                               std::make_unique<base::Value>(false));
  EXPECT_EQ(base::Seconds(0),
            GetAccessCodeDeviceDurationPref(pref_service.get()));

  pref_service->SetManagedPref(prefs::kAccessCodeCastEnabled,
                               std::make_unique<base::Value>(true));
  // Removing the set value should return the default.
  pref_service->RemoveManagedPref(prefs::kAccessCodeCastDeviceDuration);
  EXPECT_EQ(base::Seconds(0),
            GetAccessCodeDeviceDurationPref(pref_service.get()));
}
}  // namespace media_router
