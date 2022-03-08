// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/prefs_ash.h"

#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"

namespace crosapi {
namespace {

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
            ash::prefs::kAccessibilityVirtualKeyboardEnabled}});
  auto pref_name = extension_prefpath_to_name->find(path);
  DCHECK(pref_name != extension_prefpath_to_name->end());
  return pref_name->second;
}

// On non login case, ProfileManager::GetPrimaryUserProfile() returns
// a Profile instance which is different from logged in user profile.
Profile* GetPrimaryLoggedInUserProfile() {
  // Check login state first.
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return nullptr;
  }

  return ProfileManager::GetPrimaryUserProfile();
}

}  // namespace

PrefsAsh::PrefsAsh(ProfileManager* profile_manager, PrefService* local_state)
    : profile_manager_(profile_manager), local_state_(local_state) {
  DCHECK(profile_manager_);
  DCHECK(local_state_);

  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());

  profile_manager_->AddObserver(this);
  local_state_registrar_.Init(local_state_);

  Profile* primary_profile = GetPrimaryLoggedInUserProfile();
  if (primary_profile)
    OnPrimaryProfileReady(primary_profile);
}

PrefsAsh::~PrefsAsh() {
  // Remove this observer, if the Primary logged in profile is not yet created.
  // On actual shutdown, the ProfileManager will destruct before CrosapiManager.
  if (ProfileManagerObserver::IsInObserverList() && profile_manager_) {
    profile_manager_->RemoveObserver(this);
  }
}

void PrefsAsh::BindReceiver(mojo::PendingReceiver<mojom::Prefs> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PrefsAsh::GetPref(mojom::PrefPath path, GetPrefCallback callback) {
  auto state = GetState(path);
  const base::Value* value =
      state ? state->pref_service->Get(state->path) : nullptr;
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

  const base::Value* value = state->pref_service->Get(state->path);

  if (!state->is_extension_controlled_pref) {
    // Not extension controlled
    std::move(callback).Run(
        absl::optional<base::Value>(value->Clone()),
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
  std::move(callback).Run(absl::optional<base::Value>(value->Clone()),
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
      state ? state->pref_service->Get(state->path) : nullptr;
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
  Profile* primary_profile = GetPrimaryLoggedInUserProfile();
  if (!primary_profile) {
    // Primary profile is not yet available. Wait for another invocation
    // to capture its creation.
    return;
  }

  OnPrimaryProfileReady(primary_profile);
}

absl::optional<PrefsAsh::State> PrefsAsh::GetState(mojom::PrefPath path) {
  switch (path) {
    case mojom::PrefPath::kMetricsReportingEnabled:
      return State{local_state_, &local_state_registrar_, false,
                   metrics::prefs::kMetricsReportingEnabled};
    case mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled:
      if (!profile_prefs_registrar_) {
        LOG(WARNING) << "Primary profile is not yet initialized";
        return absl::nullopt;
      }
      return State{profile_prefs_registrar_->prefs(),
                   profile_prefs_registrar_.get(), false,
                   ash::prefs::kAccessibilitySpokenFeedbackEnabled};
    case mojom::PrefPath::kDeviceSystemWideTracingEnabled:
      return State{local_state_, &local_state_registrar_, false,
                   ash::prefs::kDeviceSystemWideTracingEnabled};
    case mojom::PrefPath::kDnsOverHttpsMode:
      return State{local_state_, &local_state_registrar_, false,
                   prefs::kDnsOverHttpsMode};
    case mojom::PrefPath::kDnsOverHttpsTemplates:
      return State{local_state_, &local_state_registrar_, false,
                   prefs::kDnsOverHttpsTemplates};
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
    case mojom::PrefPath::kAccessibilityVirtualKeyboardEnabled: {
      if (!profile_prefs_registrar_) {
        LOG(WARNING) << "Primary profile is not yet initialized";
        return absl::nullopt;
      }
      std::string pref_name = GetExtensionPrefNameForPref(path);
      return State{profile_prefs_registrar_->prefs(), nullptr, true, pref_name};
    }
    default:
      LOG(WARNING) << "Unknown pref path: " << path;
      return absl::nullopt;
  }
}

void PrefsAsh::OnProfileManagerDestroying() {
  profile_manager_ = nullptr;
}

void PrefsAsh::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  profile_prefs_registrar_.reset();
}

void PrefsAsh::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  profile_prefs_registrar_.reset();
}

void PrefsAsh::OnPrefChanged(mojom::PrefPath path) {
  auto state = GetState(path);
  const base::Value* value =
      state ? state->pref_service->Get(state->path) : nullptr;
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
  profile_manager_->RemoveObserver(this);
  profile_prefs_registrar_ = std::make_unique<PrefChangeRegistrar>();
  profile_prefs_registrar_->Init(profile->GetPrefs());
}

}  // namespace crosapi
