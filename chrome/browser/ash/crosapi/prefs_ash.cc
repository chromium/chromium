// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/prefs_ash.h"

#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {
namespace {

// List of all mojom::PrefPaths associated with profile prefs, and their
// corresponding paths in the prefstore. Initialized on first use.
const std::string& GetProfilePrefNameForPref(mojom::PrefPath path) {
  static base::NoDestructor<std::map<mojom::PrefPath, std::string>>
      profile_prefpath_to_name({
          {mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
           ash::prefs::kAccessibilitySpokenFeedbackEnabled},
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
      });
  auto pref_name = profile_prefpath_to_name->find(path);
  DCHECK(pref_name != profile_prefpath_to_name->end());
  return pref_name->second;
}

// List of all mojom::PrefPaths associated with extension controlled prefs,
// and their corresponding paths in the prefstore. Initialized on first use.
const std::string& GetExtensionPrefNameForPref(mojom::PrefPath path) {
  static base::NoDestructor<std::map<mojom::PrefPath, std::string>>
      extension_prefpath_to_name(
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
           {mojom::PrefPath::kProtectedContentDefault,
            prefs::kProtectedContentDefault}});
  auto pref_name = extension_prefpath_to_name->find(path);
  DCHECK(pref_name != extension_prefpath_to_name->end());
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
  auto state = GetState(path);
  const base::Value* value =
      state ? &state->pref_service->GetValue(state->path) : nullptr;
  std::move(callback).Run(value ? absl::optional<base::Value>(value->Clone())
                                : absl::nullopt);
}

void PrefsAsh::GetExtensionPrefWithControl(
    mojom::PrefPath path,
    GetExtensionPrefWithControlCallback callback) {
  auto state = GetState(path);

  if (!state) {
    // Not a valid prefpath
    std::move(callback).Run(absl::nullopt,
                            mojom::PrefControlState::kDefaultUnknown);
    return;
  }

  const base::Value& value = state->pref_service->GetValue(state->path);

  if (!state->is_extension_controlled_pref) {
    // Not extension controlled
    std::move(callback).Run(
        absl::optional<base::Value>(value.Clone()),
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
  std::move(callback).Run(absl::optional<base::Value>(value.Clone()),
                          pref_control_state);
}

void PrefsAsh::SetPref(mojom::PrefPath path,
                       base::Value value,
                       SetPrefCallback callback) {
  auto state = GetState(path);
  if (state) {
    if (state->is_extension_controlled_pref) {
      state->pref_service->SetStandaloneBrowserPref(state->path, value);
    } else {
      state->pref_service->Set(state->path, value);
    }
  }
  std::move(callback).Run();
}

void PrefsAsh::ClearExtensionControlledPref(
    mojom::PrefPath path,
    ClearExtensionControlledPrefCallback callback) {
  auto state = GetState(path);
  if (state && state->is_extension_controlled_pref) {
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
  const base::Value* value =
      state ? &state->pref_service->GetValue(state->path) : nullptr;
  if (!value) {
    return;
  }

  // Fire the observer with the initial value.
  mojo::Remote<mojom::PrefObserver> remote(std::move(observer));
  remote->OnPrefChanged(value->Clone());

  DCHECK(state->registrar);
  if (!state->registrar->IsObserved(state->path)) {
    // Unretained() is safe since PrefChangeRegistrar and RemoteSet within
    // observers_ are owned by this and wont invoke if PrefsAsh is destroyed.
    state->registrar->Add(state->path,
                          base::BindRepeating(&PrefsAsh::OnPrefChanged,
                                              base::Unretained(this), path));
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

absl::optional<PrefsAsh::State> PrefsAsh::GetState(mojom::PrefPath path) {
  switch (path) {
    case mojom::PrefPath::kUnknown:
      LOG(WARNING) << "Unknown pref path: " << path;
      return absl::nullopt;
    case mojom::PrefPath::kMetricsReportingEnabled:
      return State{local_state_, &local_state_registrar_, false,
                   metrics::prefs::kMetricsReportingEnabled};
    case mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled:
    case mojom::PrefPath::kQuickAnswersEnabled:
    case mojom::PrefPath::kQuickAnswersConsentStatus:
    case mojom::PrefPath::kQuickAnswersDefinitionEnabled:
    case mojom::PrefPath::kQuickAnswersTranslationEnabled:
    case mojom::PrefPath::kQuickAnswersUnitConversionEnabled:
    case mojom::PrefPath::kQuickAnswersNoticeImpressionCount:
    case mojom::PrefPath::kQuickAnswersNoticeImpressionDuration:
    case mojom::PrefPath::kPreferredLanguages:
    case mojom::PrefPath::kApplicationLocale:
    case mojom::PrefPath::kSharedStorage: {
      if (!profile_prefs_registrar_) {
        LOG(WARNING) << "Primary profile is not yet initialized";
        return absl::nullopt;
      }
      std::string pref_name = GetProfilePrefNameForPref(path);
      return State{profile_prefs_registrar_->prefs(),
                   profile_prefs_registrar_.get(), false, pref_name};
    }
    case mojom::PrefPath::kDeviceSystemWideTracingEnabled:
      return State{local_state_, &local_state_registrar_, false,
                   ash::prefs::kDeviceSystemWideTracingEnabled};
    case mojom::PrefPath::kDnsOverHttpsMode:
      return State{local_state_, &local_state_registrar_, false,
                   prefs::kDnsOverHttpsMode};
    case mojom::PrefPath::kDnsOverHttpsSalt:
      return State{local_state_, &local_state_registrar_, false,
                   prefs::kDnsOverHttpsSalt};
    case mojom::PrefPath::kDnsOverHttpsTemplates:
      return State{local_state_, &local_state_registrar_, false,
                   prefs::kDnsOverHttpsTemplates};
    case mojom::PrefPath::kDnsOverHttpsTemplatesWithIdentifiers:
      return State{local_state_, &local_state_registrar_, false,
                   prefs::kDnsOverHttpsTemplatesWithIdentifiers};
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
    case mojom::PrefPath::kProtectedContentDefault: {
      if (!profile_prefs_registrar_) {
        LOG(WARNING) << "Primary profile is not yet initialized";
        return absl::nullopt;
      }
      std::string pref_name = GetExtensionPrefNameForPref(path);
      return State{profile_prefs_registrar_->prefs(),
                   profile_prefs_registrar_.get(), true, pref_name};
    }
  }
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
  const base::Value* value =
      state ? &state->pref_service->GetValue(state->path) : nullptr;
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
      state->registrar->Remove(state->path);
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
