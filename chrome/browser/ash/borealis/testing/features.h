// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_TESTING_FEATURES_H_
#define CHROME_BROWSER_ASH_BOREALIS_TESTING_FEATURES_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"

class Profile;

namespace borealis {

// Convenience function for allowing and enabling Borealis at once.
void AllowAndEnableBorealis(Profile* profile,
                            base::test::ScopedFeatureList& feature_list,
                            ash::ScopedCrosSettingsTestHelper& cros_settings,
                            bool should_allow = true,
                            bool should_enable = true);
// Sets the prefs, features and settings so that Borealis is "allowed" on the
// device. Note that |feature_list| will be re-initiated during this
// function.
void AllowBorealis(Profile* profile,
                   base::test::ScopedFeatureList& feature_list,
                   ash::ScopedCrosSettingsTestHelper& cros_settings,
                   bool should_allow = true);
// Sets the prefs so that Borealis is "enabled".
void EnableBorealis(Profile* profile, bool should_enable = true);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_TESTING_FEATURES_H_
