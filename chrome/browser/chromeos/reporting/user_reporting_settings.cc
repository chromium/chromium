// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/user_reporting_settings.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace reporting {

UserReportingSettings::UserReportingSettings(base::WeakPtr<Profile> profile)
    : profile_(profile) {
  DCHECK(profile_);
  pref_change_registrar_.Init(profile_->GetPrefs());
}

UserReportingSettings::~UserReportingSettings() = default;

base::CallbackListSubscription UserReportingSettings::AddSettingsObserver(
    const std::string& path,
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);
  DCHECK(profile_->GetPrefs()->FindPreference(path));
  DCHECK(callback);

  auto [iterator, added_element] = settings_observers_.emplace(
      path, std::make_unique<base::RepeatingClosureList>());
  if (added_element) {
    // Initialize the pref change registrar for the specified path.
    pref_change_registrar_.Add(
        path, base::BindRepeating(&UserReportingSettings::OnPrefChanged,
                                  weak_ptr_factory_.GetWeakPtr(), path));
  }
  // Add the callback to the pre-existing list.
  return iterator->second->Add(callback);
}

bool UserReportingSettings::PrepareTrustedValues(base::OnceClosure callback) {
  // We trust user setting values in the pref store, so we return true.
  return true;
}

bool UserReportingSettings::GetBoolean(const std::string& path,
                                       bool* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);
  const auto* const pref_service = profile_->GetPrefs();
  if (!pref_service->FindPreference(path) ||
      !pref_service->GetValue(path).is_bool()) {
    // Invalid path or data type.
    return false;
  }
  *out_value = pref_service->GetBoolean(path);
  return true;
}

bool UserReportingSettings::GetInteger(const std::string& path,
                                       int* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);
  const auto* const pref_service = profile_->GetPrefs();
  if (!pref_service->FindPreference(path) ||
      !pref_service->GetValue(path).is_int()) {
    // Invalid path or data type.
    return false;
  }
  *out_value = pref_service->GetInteger(path);
  return true;
}

bool UserReportingSettings::GetList(const std::string& path,
                                    const base::Value::List** out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);
  const auto* const pref_service = profile_->GetPrefs();
  if (!pref_service->FindPreference(path) ||
      !pref_service->GetValue(path).is_list()) {
    // Invalid path or data type.
    return false;
  }
  *out_value = &pref_service->GetList(path);
  return true;
}

void UserReportingSettings::OnPrefChanged(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(settings_observers_.contains(path));

  // Notify registered subscribers.
  settings_observers_.find(path)->second->Notify();
}

}  // namespace reporting
