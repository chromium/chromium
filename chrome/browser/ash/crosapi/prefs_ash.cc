// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/prefs_ash.h"

#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/search_engines/default_search_manager.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {
namespace {

// List of all mojom::PrefPaths associated with profile prefs, and their
// corresponding paths in the prefstore.
std::string_view GetProfilePrefNameForPref(mojom::PrefPath path) {
  static constexpr auto kProfilePrefPathToName =
      base::MakeFixedFlatMap<mojom::PrefPath, std::string_view>({
          {mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
           ash::prefs::kAccessibilitySpokenFeedbackEnabled},
          {mojom::PrefPath::kAccessibilityReducedAnimationsEnabled,
           ash::prefs::kAccessibilityReducedAnimationsEnabled},
          {mojom::PrefPath::kUserGeolocationAccessLevel,
           ash::prefs::kUserGeolocationAccessLevel},
          {mojom::PrefPath::kQuickAnswersEnabled,
           quick_answers::prefs::kQuickAnswersEnabled},
          {mojom::PrefPath::kQuickAnswersConsentStatus,
           quick_answers::prefs::kQuickAnswersConsentStatus},
          {mojom::PrefPath::kQuickAnswersDefinitionEnabled,
           quick_answers::prefs::kQuickAnswersDefinitionEnabled},
          {mojom::PrefPath::kQuickAnswersTranslationEnabled,
           quick_answers::prefs::kQuickAnswersTranslationEnabled},
          {mojom::PrefPath::kQuickAnswersUnitConversionEnabled,
           quick_answers::prefs::kQuickAnswersUnitConversionEnabled},
          {mojom::PrefPath::kQuickAnswersNoticeImpressionCount,
           quick_answers::prefs::kQuickAnswersNoticeImpressionCount},
          {mojom::PrefPath::kQuickAnswersNoticeImpressionDuration,
           quick_answers::prefs::kQuickAnswersNoticeImpressionDuration},
          {mojom::PrefPath::kPreferredLanguages,
           language::prefs::kPreferredLanguages},
          {mojom::PrefPath::kApplicationLocale,
           language::prefs::kApplicationLocale},
          {mojom::PrefPath::kSharedStorage, prefs::kSharedStorage},
          {mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount,
           ash::prefs::kMultitaskMenuNudgeClamshellShownCount},
          {mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown,
           ash::prefs::kMultitaskMenuNudgeClamshellLastShown},
          {mojom::PrefPath::kAccessCodeCastDevices,
           media_router::prefs::kAccessCodeCastDevices},
          {mojom::PrefPath::kAccessCodeCastDeviceAdditionTime,
           media_router::prefs::kAccessCodeCastDeviceAdditionTime},
          {mojom::PrefPath::kDefaultSearchProviderDataPrefName,
           DefaultSearchManager::kDefaultSearchProviderDataPrefName},
          {mojom::PrefPath::kIsolatedWebAppsEnabled,
           ash::prefs::kIsolatedWebAppsEnabled},
          {mojom::PrefPath::kHmrEnabled, ash::prefs::kHmrEnabled},
          {mojom::PrefPath::kUserCameraAllowed, ash::prefs::kUserCameraAllowed},
          {mojom::PrefPath::kUserMicrophoneAllowed,
           ash::prefs::kUserMicrophoneAllowed},
          {mojom::PrefPath::kHMRConsentStatus, ash::prefs::kHMRConsentStatus},
          {mojom::PrefPath::kHMRConsentWindowDismissCount,
           ash::prefs::kHMRConsentWindowDismissCount},

      });
  auto pref_name = kProfilePrefPathToName.find(path);
  DCHECK(pref_name != kProfilePrefPathToName.end());
  return pref_name->second;
}

// List of all mojom::PrefPaths associated with extension controlled prefs,
// and their corresponding paths in the prefstore.
std::string_view GetExtensionPrefNameForPref(mojom::PrefPath path) {
  static constexpr auto kExtensionPrefPathToName =
      base::MakeFixedFlatMap<mojom::PrefPath, std::string_view>(
          {{mojom::PrefPath::kDockedMagnifierEnabled,
            ash::prefs::kDockedMagnifierEnabled},
           {mojom::PrefPath::kAccessibilityAutoclickEnabled,
            ash::prefs::kAccessibilityAutoclickEnabled},
           {mojom::PrefPath::kAccessibilityCaretHighlightEnabled,
            ash::prefs::kAccessibilityCaretHighlightEnabled},
           {mojom::PrefPath::kAccessibilityCursorColorEnabled,
            ash::prefs::kAccessibilityCursorColorEnabled},
           {mojom::PrefPath::kAccessibilityCursorHighlightEnabled,
            ash::prefs::kAccessibilityCursorHighlightEnabled},
           {mojom::PrefPath::kAccessibilityDictationEnabled,
            ash::prefs::kAccessibilityDictationEnabled},
           {mojom::PrefPath::kAccessibilityFocusHighlightEnabled,
            ash::prefs::kAccessibilityFocusHighlightEnabled},
           {mojom::PrefPath::kAccessibilityHighContrastEnabled,
            ash::prefs::kAccessibilityHighContrastEnabled},
           {mojom::PrefPath::kAccessibilityLargeCursorEnabled,
            ash::prefs::kAccessibilityLargeCursorEnabled},
           {mojom::PrefPath::kAccessibilityScreenMagnifierEnabled,
            ash::prefs::kAccessibilityScreenMagnifierEnabled},
           {mojom::PrefPath::kAccessibilitySelectToSpeakEnabled,
            ash::prefs::kAccessibilitySelectToSpeakEnabled},
           {mojom::PrefPath::kExtensionAccessibilitySpokenFeedbackEnabled,
            ash::prefs::kAccessibilitySpokenFeedbackEnabled},
           {mojom::PrefPath::kAccessibilityStickyKeysEnabled,
            ash::prefs::kAccessibilityStickyKeysEnabled},
           {mojom::PrefPath::kAccessibilitySwitchAccessEnabled,
            ash::prefs::kAccessibilitySwitchAccessEnabled},
           {mojom::PrefPath::kAccessibilityVirtualKeyboardEnabled,
            ash::prefs::kAccessibilityVirtualKeyboardEnabled},
           {mojom::PrefPath::kProxy, ash::prefs::kProxy}});
  auto pref_name = kExtensionPrefPathToName.find(path);
  DCHECK(pref_name != kExtensionPrefPathToName.end());
  return pref_name->second;
}

}  // namespace

PrefsAsh::PrefsAsh(ProfileManager* profile_manager, PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(profile_manager);
  DCHECK(local_state_);

  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(
          base::BindOnce(&PrefsAsh::OnAppTerminating, base::Unretained(this)));

  profile_manager_observation_.Observe(profile_manager);
  local_state_registrar_.Init(local_state_);
}

PrefsAsh::~PrefsAsh() = default;

void PrefsAsh::BindReceiver(mojo::PendingReceiver<mojom::Prefs> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PrefsAsh::GetPref(mojom::PrefPath path, GetPrefCallback callback) {
  const base::Value* value = GetValueForState(GetState(path));
  std::move(callback).Run(value ? std::optional<base::Value>(value->Clone())
                                : std::nullopt);
}

void PrefsAsh::GetExtensionPrefWithControl(
    mojom::PrefPath path,
    GetExtensionPrefWithControlCallback callback) {
  auto state = GetState(path);
  const base::Value* value = GetValueForState(state);

  if (!state || !value) {
    // Not a valid prefpath
    std::move(callback).Run(std::nullopt,
                            mojom::PrefControlState::kDefaultUnknown);
    return;
  }

  if (state->pref_source != AshPrefSource::kExtensionControlled) {
    // Not extension controlled
    std::move(callback).Run(
        std::optional<base::Value>(value->Clone()),
        mojom::PrefControlState::kNotExtensionControlledPrefPath);
    return;
  }

  mojom::PrefControlState pref_control_state;
  // Extension controlled.
  const PrefService::Preference* preference =
      state->pref_service->FindPreference(state->path);
  DCHECK(preference != nullptr);
  if (!preference->IsStandaloneBrowserModifiable()) {
    pref_control_state = mojom::PrefControlState::kNotExtensionControllable;
  } else if (preference->IsStandaloneBrowserControlled()) {
    // Lacros has already set this pref. It could be set by any extension
    // in lacros.
    pref_control_state = mojom::PrefControlState::kLacrosExtensionControlled;
  } else {
    // Lacros could control this.
    pref_control_state = mojom::PrefControlState::kLacrosExtensionControllable;
  }
  std::move(callback).Run(std::optional<base::Value>(value->Clone()),
                          pref_control_state);
}

void PrefsAsh::SetPref(mojom::PrefPath path,
                       base::Value value,
                       SetPrefCallback callback) {
  auto state = GetState(path);
  if (state && state->pref_source != AshPrefSource::kCrosSettings) {
    if (state->pref_source == AshPrefSource::kExtensionControlled) {
      state->pref_service->SetStandaloneBrowserPref(state->path, value);
    } else {
      state->pref_service->Set(state->path, value);
    }
  } else {
    LOG(WARNING) << "CrosSettings can't be changed via PrefsAsh";
  }
  std::move(callback).Run();
}

void PrefsAsh::ClearExtensionControlledPref(
    mojom::PrefPath path,
    ClearExtensionControlledPrefCallback callback) {
  auto state = GetState(path);
  if (state && state->pref_source == AshPrefSource::kExtensionControlled) {
    state->pref_service->RemoveStandaloneBrowserPref(state->path);
  } else {
    // Only logging to be robust against version skew (lacros ahead of ash)
    LOG(WARNING) << "Tried to clear a pref that is not extension controlled";
  }
  std::move(callback).Run();
}

void PrefsAsh::AddObserver(mojom::PrefPath path,
                           mojo::PendingRemote<mojom::PrefObserver> observer) {
  auto state = GetState(path);
  const base::Value* value = GetValueForState(state);
  if (!value) {
    return;
  }

  // Fire the observer with the initial value.
  mojo::Remote<mojom::PrefObserver> remote(std::move(observer));
  remote->OnPrefChanged(value->Clone());

  bool did_register = false;
  if (state->pref_source == AshPrefSource::kCrosSettings) {
    if (cros_settings_subs_.find(path) == cros_settings_subs_.end()) {
      // Unretained() is safe since CrosSettings is destroyed after all the
      // threads are stopped and PrefsAsh is destroyed while stopping all the
      // threads.
      cros_settings_subs_.emplace(
          path,
          ash::CrosSettings::Get()->AddSettingsObserver(
              state->path, base::BindRepeating(&PrefsAsh::OnPrefChanged,
                                               base::Unretained(this), path)));
      did_register = true;
    }
  } else {
    DCHECK(state->registrar);
    if (!state->registrar->IsObserved(state->path)) {
      // Unretained() is safe since PrefChangeRegistrar and RemoteSet within
      // observers_ are owned by this and wont invoke if PrefsAsh is destroyed.
      state->registrar->Add(state->path,
                            base::BindRepeating(&PrefsAsh::OnPrefChanged,
                                                base::Unretained(this), path));
      did_register = true;
    }
  }
  if (did_register) {
    observers_[path].set_disconnect_handler(base::BindRepeating(
        &PrefsAsh::OnDisconnect, base::Unretained(this), path));
  }
  observers_[path].Add(std::move(remote));
}

void PrefsAsh::OnProfileAdded(Profile* profile) {
  if (!ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return;
  }

  OnPrimaryProfileReady(profile);
}

std::optional<PrefsAsh::State> PrefsAsh::GetState(mojom::PrefPath path) {
  switch (path) {
    case mojom::PrefPath::kUnknown:
    case mojom::PrefPath::kProtectedContentDefaultDeprecated:
    case mojom::PrefPath::kDnsOverHttpsTemplates:
    case mojom::PrefPath::kDnsOverHttpsTemplatesWithIdentifiers:
    case mojom::PrefPath::kDnsOverHttpsSalt:
    case mojom::PrefPath::kAccessibilityPdfOcrAlwaysActiveDeprecated:
    case mojom::PrefPath::kMahiEnabledDeprecated:
      LOG(WARNING) << "Unknown pref path: " << path;
      return std::nullopt;
    case mojom::PrefPath::kMetricsReportingEnabled:
      return State{local_state_, &local_state_registrar_,
                   AshPrefSource::kNormal,
                   metrics::prefs::kMetricsReportingEnabled};
    case mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled:
    case mojom::PrefPath::kUserGeolocationAccessLevel:
    case mojom::PrefPath::kQuickAnswersEnabled:
    case mojom::PrefPath::kQuickAnswersConsentStatus:
    case mojom::PrefPath::kQuickAnswersDefinitionEnabled:
    case mojom::PrefPath::kQuickAnswersTranslationEnabled:
    case mojom::PrefPath::kQuickAnswersUnitConversionEnabled:
    case mojom::PrefPath::kQuickAnswersNoticeImpressionCount:
    case mojom::PrefPath::kQuickAnswersNoticeImpressionDuration:
    case mojom::PrefPath::kPreferredLanguages:
    case mojom::PrefPath::kApplicationLocale:
    case mojom::PrefPath::kSharedStorage:
    case mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount:
    case mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown:
    case mojom::PrefPath::kAccessCodeCastDevices:
    case mojom::PrefPath::kAccessCodeCastDeviceAdditionTime:
    case mojom::PrefPath::kDefaultSearchProviderDataPrefName:
    case mojom::PrefPath::kIsolatedWebAppsEnabled:
    case mojom::PrefPath::kAccessibilityReducedAnimationsEnabled:
    case mojom::PrefPath::kHmrEnabled:
    case mojom::PrefPath::kUserCameraAllowed:
    case mojom::PrefPath::kUserMicrophoneAllowed:
    case mojom::PrefPath::kHMRConsentStatus:
    case mojom::PrefPath::kHMRConsentWindowDismissCount: {
      if (!profile_prefs_registrar_) {
        LOG(WARNING) << "Primary profile is not yet initialized";
        return std::nullopt;
      }
      std::string pref_name(GetProfilePrefNameForPref(path));
      return State{profile_prefs_registrar_->prefs(),
                   profile_prefs_registrar_.get(), AshPrefSource::kNormal,
                   pref_name};
    }
    case mojom::PrefPath::kDeviceSystemWideTracingEnabled:
      return State{local_state_, &local_state_registrar_,
                   AshPrefSource::kNormal,
                   ash::prefs::kDeviceSystemWideTracingEnabled};
    case mojom::PrefPath::kDnsOverHttpsMode:
      return State{local_state_, &local_state_registrar_,
                   AshPrefSource::kNormal, prefs::kDnsOverHttpsMode};
    case mojom::PrefPath::kDnsOverHttpsEffectiveTemplatesChromeOS:
      return State{local_state_, &local_state_registrar_,
                   AshPrefSource::kNormal,
                   prefs::kDnsOverHttpsEffectiveTemplatesChromeOS};
    case mojom::PrefPath::kOverscrollHistoryNavigationEnabled:
      return State{local_state_, &local_state_registrar_,
                   AshPrefSource::kNormal,
                   prefs::kOverscrollHistoryNavigationEnabled};
    case mojom::PrefPath::kDockedMagnifierEnabled:
    case mojom::PrefPath::kAccessibilityAutoclickEnabled:
    case mojom::PrefPath::kAccessibilityCaretHighlightEnabled:
    case mojom::PrefPath::kAccessibilityCursorColorEnabled:
    case mojom::PrefPath::kAccessibilityCursorHighlightEnabled:
    case mojom::PrefPath::kAccessibilityDictationEnabled:
    case mojom::PrefPath::kAccessibilityFocusHighlightEnabled:
    case mojom::PrefPath::kAccessibilityHighContrastEnabled:
    case mojom::PrefPath::kAccessibilityLargeCursorEnabled:
    case mojom::PrefPath::kAccessibilityScreenMagnifierEnabled:
    case mojom::PrefPath::kAccessibilitySelectToSpeakEnabled:
    case mojom::PrefPath::kExtensionAccessibilitySpokenFeedbackEnabled:
    case mojom::PrefPath::kAccessibilityStickyKeysEnabled:
    case mojom::PrefPath::kAccessibilitySwitchAccessEnabled:
    case mojom::PrefPath::kAccessibilityVirtualKeyboardEnabled:
    case mojom::PrefPath::kProxy: {
      if (!profile_prefs_registrar_) {
        LOG(WARNING) << "Primary profile is not yet initialized";
        return std::nullopt;
      }
      std::string pref_name(GetExtensionPrefNameForPref(path));
      return State{profile_prefs_registrar_->prefs(),
                   profile_prefs_registrar_.get(),
                   AshPrefSource::kExtensionControlled, pref_name};
    }
    case mojom::PrefPath::kAttestationForContentProtectionEnabled: {
      return State{nullptr, nullptr, AshPrefSource::kCrosSettings,
                   ash::kAttestationForContentProtectionEnabled};
    }
    case mojom::PrefPath::kAccessToGetAllScreensMediaInSessionAllowedForUrls:
      if (!profile_prefs_registrar_) {
        LOG(WARNING) << "Primary profile is not yet initialized";
        return std::nullopt;
      }
      return State{
          .pref_service = profile_prefs_registrar_->prefs(),
          .registrar = profile_prefs_registrar_.get(),
          .pref_source = AshPrefSource::kNormal,
          .path =
              prefs::kManagedAccessToGetAllScreensMediaInSessionAllowedForUrls};
  }
}

const base::Value* PrefsAsh::GetValueForState(std::optional<State> state) {
  if (!state) {
    return nullptr;
  }

  if (state->pref_source == AshPrefSource::kCrosSettings) {
    return ash::CrosSettings::Get()->GetPref(state->path);
  }

  return &state->pref_service->GetValue(state->path);
}

void PrefsAsh::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void PrefsAsh::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  profile_prefs_registrar_.reset();
}

void PrefsAsh::OnPrefChanged(mojom::PrefPath path) {
  auto state = GetState(path);
  const base::Value* value = GetValueForState(state);
  if (value) {
    for (auto& observer : observers_[path]) {
      observer->OnPrefChanged(value->Clone());
    }
  }
}

void PrefsAsh::OnDisconnect(mojom::PrefPath path, mojo::RemoteSetElementId id) {
  const auto& it = observers_.find(path);
  if (it != observers_.end() && it->second.empty()) {
    if (auto state = GetState(path)) {
      if (state->pref_source == AshPrefSource::kCrosSettings) {
        cros_settings_subs_.erase(path);
      } else {
        state->registrar->Remove(state->path);
      }
    }
    observers_.erase(it);
  }
}

void PrefsAsh::OnPrimaryProfileReady(Profile* profile) {
  profile_manager_observation_.Reset();
  profile_prefs_registrar_ = std::make_unique<PrefChangeRegistrar>();
  profile_prefs_registrar_->Init(profile->GetPrefs());
}

void PrefsAsh::OnAppTerminating() {
  profile_prefs_registrar_.reset();
}

}  // namespace crosapi
