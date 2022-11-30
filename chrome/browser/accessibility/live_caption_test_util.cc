// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption_test_util.h"

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "media/base/media_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace captions {

namespace {
// Chrome feature flags that gate Live Caption.
std::vector<base::test::FeatureRef> RequiredFeatureFlags() {
  std::vector<base::test::FeatureRef> features = {media::kLiveCaption};
#if BUILDFLAG(IS_CHROMEOS_ASH)
  features.push_back(ash::features::kOnDeviceSpeechRecognition);
#endif
  return features;
}
}  // namespace

void LiveCaptionBrowserTest::SetUp() {
  scoped_feature_list_.InitWithFeatures(RequiredFeatureFlags(), {});
  InProcessBrowserTest::SetUp();
}

void LiveCaptionBrowserTest::SetLiveCaptionEnabled(bool enabled) {
  SetLiveCaptionEnabledOnProfile(enabled, browser()->profile());
}

void LiveCaptionBrowserTest::SetLiveCaptionEnabledOnProfile(bool enabled,
                                                            Profile* profile) {
  profile->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, enabled);
  if (enabled) {
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
        speech::LanguageCode::kEnUs);
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  }
}

}  // namespace captions
