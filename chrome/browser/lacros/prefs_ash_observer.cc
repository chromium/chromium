// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/prefs_ash_observer.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "components/content_settings/core/common/pref_names.h"

PrefsAshObserver::PrefsAshObserver(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

PrefsAshObserver::~PrefsAshObserver() = default;

void PrefsAshObserver::Init() {
  // Initial values are obtained when the observers are created, there is no
  // need to do so explcitly.

  // Local state prefs:
  doh_mode_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kDnsOverHttpsMode,
      base::BindRepeating(&PrefsAshObserver::OnDnsOverHttpsModeChanged,
                          base::Unretained(this)));

  doh_templates_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kDnsOverHttpsEffectiveTemplatesChromeOS,
      base::BindRepeating(
          &PrefsAshObserver::OnDnsOverHttpsEffectiveTemplatesChromeOSChanged,
          base::Unretained(this)));

  mahi_prefs_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kHmrEnabled,
      base::BindRepeating(&PrefsAshObserver::OnMahiEnabledChanged,
                          base::Unretained(this)));

  // TODO(acostinas, b/328566515) Remove `deprecated_doh_templates_observer_` in
  // version 126. In the meantime, monitor the pref kDnsOverHttpsTemplates
  // (which is deprecated in Lacros) to support older version of Ash.
  deprecated_doh_templates_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kDnsOverHttpsTemplates,
      base::BindRepeating(
          &PrefsAshObserver::OnDeprecatedDnsOverHttpsTemplatesChanged,
          base::Unretained(this)));

  access_to_get_all_screens_media_in_session_allowed_for_urls_observer_ =
      std::make_unique<CrosapiPrefObserver>(
          crosapi::mojom::PrefPath::
              kAccessToGetAllScreensMediaInSessionAllowedForUrls,
          base::BindRepeating(
              &PrefsAshObserver::OnUserProfileValueChanged,
              base::Unretained(this),
              prefs::
                  kManagedAccessToGetAllScreensMediaInSessionAllowedForUrls));

  // User prefs (need caching before user profile is initialized):
  post_profile_initialized_handlers_
      [prefs::kManagedAccessToGetAllScreensMediaInSessionAllowedForUrls] =
          base::BindRepeating(PrefsAshObserver::ListChangedHandler);
}

void PrefsAshObserver::InitPostProfileInitialized(Profile* profile) {
  if (!profile || !profile->IsMainProfile()) {
    DVLOG(1) << "No primary user profile";
    return;
  }

  PrefService* const pref_service = profile->GetPrefs();
  CHECK(pref_service);

  auto pre_profile_initialized_values =
      std::move(pre_profile_initialized_values_);
  for (auto& [pref_name, value] : pre_profile_initialized_values) {
    if (post_profile_initialized_handlers_.find(pref_name) ==
        post_profile_initialized_handlers_.end()) {
      DVLOG(1) << "Post profile handler was not found";
      continue;
    }
    const auto& post_profile_initialized_handler =
        post_profile_initialized_handlers_[pref_name];
    post_profile_initialized_handler.Run(pref_service, pref_name,
                                         std::move(value));
  }
  is_profile_initialized_ = true;
}

void PrefsAshObserver::OnDnsOverHttpsModeChanged(base::Value value) {
  if (!value.is_string()) {
    LOG(WARNING) << "Unexpected value type: "
                 << base::Value::GetTypeName(value.type());
    return;
  }
  local_state_->SetString(prefs::kDnsOverHttpsMode, value.GetString());
}

void PrefsAshObserver::OnDnsOverHttpsEffectiveTemplatesChromeOSChanged(
    base::Value value) {
  effective_chromeos_secure_dns_settings_active_ = true;
  if (!value.is_string()) {
    LOG(WARNING) << "Unexpected value type: "
                 << base::Value::GetTypeName(value.type());
    return;
  }
  local_state_->SetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS,
                          value.GetString());
}

void PrefsAshObserver::OnMahiEnabledChanged(base::Value value) {
  if (!value.is_bool()) {
    LOG(WARNING) << "Unexpected value type: "
                 << base::Value::GetTypeName(value.type());
    return;
  }
  chromeos::MahiWebContentsManager::Get()->set_mahi_pref_lacros(
      value.GetBool());
}

void PrefsAshObserver::OnDeprecatedDnsOverHttpsTemplatesChanged(
    base::Value value) {
  if (effective_chromeos_secure_dns_settings_active_) {
    return;
  }
  if (!value.is_string()) {
    LOG(WARNING) << "Unexpected value type: "
                 << base::Value::GetTypeName(value.type());
    return;
  }
  local_state_->SetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS,
                          value.GetString());
}

void PrefsAshObserver::OnUserProfileValueChanged(const std::string& target_pref,
                                                 base::Value value) {
  if (is_profile_initialized_) {
    if (post_profile_initialized_handlers_.find(target_pref) ==
        post_profile_initialized_handlers_.end()) {
      DVLOG(1) << "Post profile handler was not found";
      return;
    }
    post_profile_initialized_handlers_[target_pref].Run(
        ProfileManager::GetPrimaryUserProfile()->GetPrefs(), target_pref,
        std::move(value));
  } else {
    pre_profile_initialized_values_[target_pref] = std::move(value);
  }
}

void PrefsAshObserver::ListChangedHandler(PrefService* pref_service,
                                          const std::string& pref_name,
                                          base::Value value) {
  CHECK(pref_service);
  const base::Value::List* const value_list = value.GetIfList();
  if (!value_list) {
    DVLOG(1) << "Passed value is not a list";
    return;
  }

  pref_service->SetList(pref_name, value_list->Clone());
}
