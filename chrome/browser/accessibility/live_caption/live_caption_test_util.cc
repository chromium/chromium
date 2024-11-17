// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_test_util.h"

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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#endif

namespace captions {

namespace {

// Chrome feature flags that gate Live Caption.
std::vector<base::test::FeatureRef> RequiredFeatureFlags() {
  std::vector<base::test::FeatureRef> features = {
      media::kLiveTranslate, media::kFeatureManagementLiveTranslateCrOS,
      media::kLiveCaptionAutomaticLanguageDownload};
#if BUILDFLAG(IS_CHROMEOS_ASH)
  features.push_back(ash::features::kOnDeviceSpeechRecognition);
#endif
  return features;
}

// LaCrOS learns about ondevice-speech support via BrowserInitParams.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
void SetRequiredLacrosInitParams() {
  crosapi::mojom::BrowserInitParamsPtr init_params =
      chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->is_ondevice_speech_supported = true;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
}
#endif

}  // namespace

void LiveCaptionBrowserTest::SetUp() {
  scoped_feature_list_.InitWithFeatures(RequiredFeatureFlags(), {});
  InProcessBrowserTest::SetUp();
}

void LiveCaptionBrowserTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SetRequiredLacrosInitParams();
#endif
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
