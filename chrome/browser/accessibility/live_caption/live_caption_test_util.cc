// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_test_util.h"

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "media/base/media_switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

namespace captions {

namespace {

// Chrome feature flags that gate Live Caption.
std::vector<base::test::FeatureRef> RequiredFeatureFlags() {
  std::vector<base::test::FeatureRef> features = {
      media::kLiveTranslate, media::kFeatureManagementLiveTranslateCrOS};
#if BUILDFLAG(IS_CHROMEOS)
  features.push_back(ash::features::kOnDeviceSpeechRecognition);
#endif
  return features;
}

}  // namespace

void LiveCaptionBrowserTest::SetUp() {
  scoped_feature_list_.InitWithFeatures(RequiredFeatureFlags(), {});
  InProcessBrowserTest::SetUp();
}

void LiveCaptionBrowserTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
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

void LiveCaptionBrowserTest::SetLiveTranslateEnabled(bool enabled) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                               enabled);
  browser()->profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode,
                                              "en-US");
  browser()->profile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "fr-FR");
}

}  // namespace captions
