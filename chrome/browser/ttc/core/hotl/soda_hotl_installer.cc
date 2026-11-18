// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/soda_hotl_installer.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/features.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_util.h"

namespace ttc::hotl {

std::string GetHotlRecognitionLanguage(Profile& profile) {
  PrefService& profile_prefs = CHECK_DEREF(profile.GetPrefs());
  const PrefService::Preference* language_pref =
      profile_prefs.FindPreference(prefs::kLiveCaptionLanguageCode);
  if (language_pref && !language_pref->IsDefaultValue()) {
    return prefs::GetLiveCaptionLanguageCode(&profile_prefs);
  }

  CHECK(g_browser_process);
  return std::string(speech::GetDefaultLiveCaptionLanguage(
      g_browser_process->GetApplicationLocale(), profile_prefs));
}

void PrecacheSodaForHotl(Profile& profile) {
  if (!base::FeatureList::IsEnabled(kTtcHotlClickConfirmation) ||
      !speech::IsOnDeviceSpeechRecognitionSupported()) {
    return;
  }

  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (!soda_installer) {
    CHECK_IS_TEST();
    return;
  }

  CHECK(g_browser_process);
  PrefService* global_prefs = g_browser_process->local_state();
  CHECK(global_prefs);

  const std::string target_language = GetHotlRecognitionLanguage(profile);
  std::optional<speech::SodaLanguagePackComponentConfig> config =
      speech::GetLanguageComponentConfigMatchingLanguageSubtag(target_language);
  if (!config.has_value() ||
      !soda_installer->IsLanguageEnabled(config->language_name)) {
    return;
  }

  if (soda_installer->IsSodaInstalled(config->language_code)) {
    soda_installer->SetUninstallTimer(global_prefs, config->language_name);
    return;
  }

  soda_installer->InstallSoda(global_prefs);
  soda_installer->InstallLanguage(config->language_name, global_prefs);
}

}  // namespace ttc::hotl
