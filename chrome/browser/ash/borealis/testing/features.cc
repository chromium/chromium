// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/testing/features.h"

#include "ash/components/settings/cros_settings_names.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"

namespace borealis {

void AllowAndEnableBorealis(Profile* profile,
                            base::test::ScopedFeatureList& feature_list,
                            ash::ScopedCrosSettingsTestHelper& cros_settings,
                            bool should_allow /*=true*/,
                            bool should_enable /*=true*/) {
  AllowBorealis(profile, feature_list, cros_settings, should_allow);
  EnableBorealis(profile, should_enable);
}

void AllowBorealis(Profile* profile,
                   base::test::ScopedFeatureList& feature_list,
                   ash::ScopedCrosSettingsTestHelper& cros_settings,
                   bool should_allow /*=true*/) {
  if (should_allow) {
    feature_list.InitWithFeatures({features::kBorealis}, {});
  } else {
    feature_list.InitWithFeatures({}, {features::kBorealis});
  }

  cros_settings.SetBoolean(ash::kBorealisAllowedForDevice, should_allow);

  profile->GetPrefs()->SetBoolean(prefs::kBorealisAllowedForUser, should_allow);
}

void EnableBorealis(Profile* profile, bool should_enable /*=true*/) {
  profile->GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice,
                                  should_enable);
}

}  // namespace borealis
