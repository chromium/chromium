// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "ash/constants/ash_features.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ProfileRegisterProfilePrefsOsSyncTest
    : public testing::TestWithParam<bool> {
 public:
  ProfileRegisterProfilePrefsOsSyncTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          ash::features::kOsSyncAccessibilitySettingsBatch2);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ash::features::kOsSyncAccessibilitySettingsBatch2);
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_P(ProfileRegisterProfilePrefsOsSyncTest,
       CaptionPrefsRespectBatch2FeatureFlag) {
  constexpr auto kCaptionPrefs = std::to_array<const char*>({
      prefs::kAccessibilityCaptionsTextSize,
      prefs::kAccessibilityCaptionsTextFont,
      prefs::kAccessibilityCaptionsTextColor,
      prefs::kAccessibilityCaptionsTextOpacity,
      prefs::kAccessibilityCaptionsBackgroundColor,
      prefs::kAccessibilityCaptionsTextShadow,
      prefs::kAccessibilityCaptionsBackgroundOpacity,
  });

  scoped_refptr<user_prefs::PrefRegistrySyncable> registry =
      base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  Profile::RegisterProfilePrefs(registry.get());

  const bool expect_sync = GetParam();
  for (const char* pref_name : kCaptionPrefs) {
    const uint32_t flags = registry->GetRegistrationFlags(pref_name);
    if (expect_sync) {
      EXPECT_NE(0u, flags & user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF)
          << pref_name;
    } else {
      EXPECT_EQ(0u, flags & user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF)
          << pref_name;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProfileRegisterProfilePrefsOsSyncTest,
                         ::testing::Values(true, false));
