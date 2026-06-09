// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_pref_names.h"

#include <utility>

#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/common/chrome_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/base/accelerators/command.h"

namespace glic::prefs {

GlicActuationOnWebPolicyState GetGlicActuationOnWebPolicyState() {
  auto default_pref_value = features::kGlicActorEnterprisePrefDefault.Get();
  switch (default_pref_value) {
    case features::GlicActorEnterprisePrefDefault::kForcedDisabled:
    case features::GlicActorEnterprisePrefDefault::kDisabledByDefault:
      return GlicActuationOnWebPolicyState::kDisabled;
    case features::GlicActorEnterprisePrefDefault::kEnabledByDefault:
      return GlicActuationOnWebPolicyState::kEnabled;
  }
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kGlicPinnedToTabstrip, true);
  registry->RegisterBooleanPref(kGlicMicrophoneEnabled, false);
  registry->RegisterBooleanPref(kGlicGeolocationEnabled, false);
  registry->RegisterBooleanPref(kGlicTabContextEnabled, false);
  registry->RegisterBooleanPref(kGlicDefaultTabContextEnabled, true);
  registry->RegisterBooleanPref(
      kGlicRolloutEligibility, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kNotStarted));
  registry->RegisterIntegerPref(prefs::kGlicZoomLevel, 100);
  registry->RegisterTimePref(kGlicWindowLastDismissedTime, base::Time());

  // The default value is not used. If not set the default position is
  // calculated based on the entrypoint and current active browser.
  registry->RegisterIntegerPref(kGlicPreviousPositionX, 0,
                                PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(kGlicPreviousPositionY, 0,
                                PrefRegistry::LOSSY_PREF);

  // Dict pref to store GlicUserStatus information.
  registry->RegisterDictionaryPref(prefs::kGlicUserStatus);

  // Boolean pref for the closed captioning setting.
  registry->RegisterBooleanPref(prefs::kGlicClosedCaptioningEnabled, false);

  registry->RegisterIntegerPref(prefs::kGlicSelectionWidgetDismissCount, 0);

  // Boolean pref that determines if errors are allowed to be shown.
  registry->RegisterBooleanPref(prefs::kGlicShowErrorAllowed, false);

  // Boolean pref for the daisy chain new tabs setting.
  registry->RegisterBooleanPref(prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled,
                                true);

  // Boolean pref that enables or disables experimental triggering.
  registry->RegisterBooleanPref(prefs::kGlicExperimentalTriggeringEnabled,
                                false);

  // Integer pref that determines if Glic Spark is enabled.
  // Controlled by enterprise policy.
  registry->RegisterIntegerPref(
      prefs::kGlicSparkPolicySettings,
      std::to_underlying(GlicSparkPolicyState::kDisabled));

  registry->RegisterIntegerPref(
      prefs::kGlicActuationOnWeb,
      std::to_underlying(GetGlicActuationOnWebPolicyState()));

  registry->RegisterListPref(prefs::kGlicActuationOnWebAllowedForURLs);
  registry->RegisterListPref(prefs::kGlicActuationOnWebBlockedForURLs);

  registry->RegisterBooleanPref(prefs::kGlicUserEnabledActuationOnWeb, false);

  registry->RegisterBooleanPref(prefs::kGlicPartitionNeedsCookieSync, true);
  registry->RegisterBooleanPref(prefs::kGlicPreviouslyNotAllowed, false);

  registry->RegisterDictionaryPref(prefs::kGlicGeminiEnterpriseSettings);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlicLauncherEnabled, false);

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterStringPref(
      prefs::kGlicLauncherHotkey,
      ui::Command::AcceleratorToString(
          LocalHotkeyManager::GetDefaultAccelerator(
              LocalHotkeyManager::Command::kOpenGlic)));
#else
  registry->RegisterStringPref(
      prefs::kGlicLauncherHotkey,
      ui::Command::AcceleratorToString(
          GlicLauncherConfiguration::GetDefaultHotkey()));
#endif
  registry->RegisterStringPref(
      prefs::kGlicSelectionHotkey,
      ui::Command::AcceleratorToString(
          GlicLauncherConfiguration::GetDefaultSelectionHotkey()));
  registry->RegisterStringPref(
      prefs::kGlicFocusToggleHotkey,
      ui::Command::AcceleratorToString(
          LocalHotkeyManager::GetDefaultAccelerator(
              LocalHotkeyManager::Command::kFocusToggle)));

  registry->RegisterStringPref(prefs::kGlicGuestUrlPresetAutopush, "");
  registry->RegisterStringPref(prefs::kGlicGuestUrlPresetStaging, "");
  registry->RegisterStringPref(prefs::kGlicGuestUrlPresetPreprod, "");
  registry->RegisterStringPref(prefs::kGlicGuestUrlPresetProd, "");
  registry->RegisterStringPref(
      prefs::kGlicWebContinuityOriginatingHostUrlPreset, "");
#if BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(prefs::kGlicUseAltOSIcon, false);
#endif
}

}  // namespace glic::prefs
