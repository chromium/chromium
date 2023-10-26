// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/user_reporting_settings.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace reporting {

UserReportingSettings::UserReportingSettings(base::WeakPtr<Profile> profile)
    : profile_(profile) {
  if (profile_) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(profile_->GetPrefs());
    scoped_profile_observer_.Observe(profile_.get());
  }
}

UserReportingSettings::~UserReportingSettings() = default;

base::CallbackListSubscription UserReportingSettings::AddSettingsObserver(
    const std::string& path,
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile_);
  CHECK(profile_->GetPrefs()->FindPreference(path));
  CHECK(callback);
  CHECK(pref_change_registrar_);

  auto [iterator, added_element] = settings_observers_.emplace(
      path, std::make_unique<base::RepeatingClosureList>());
  if (added_element) {
    // Initialize the pref change registrar for the specified path.
    pref_change_registrar_->Add(
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
  CHECK(profile_);
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
  CHECK(profile_);
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
  CHECK(profile_);
  const auto* const pref_service = profile_->GetPrefs();
  if (!pref_service->FindPreference(path) ||
      !pref_service->GetValue(path).is_list()) {
    // Invalid path or data type.
    return false;
  }
  *out_value = &pref_service->GetList(path);
  return true;
}

bool UserReportingSettings::GetReportingEnabled(const std::string& path,
                                                bool* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile_);

  const base::Value::List* list_value;
  if (GetBoolean(path, out_value)) {
    return true;
  } else if (GetList(path, &list_value)) {
    *out_value = !list_value->empty();
    return true;
  }

  return false;
}

bool UserReportingSettings::IsObservingSettingsForTest(
    const std::string& path) {
  if (!pref_change_registrar_) {
    return false;
  }
  return pref_change_registrar_->IsObserved(path);
}

void UserReportingSettings::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Dispose the change registrar on profile destruction to prevent dangling
  // pointer references to the user pref store.
  pref_change_registrar_.reset();
  scoped_profile_observer_.Reset();
}

void UserReportingSettings::OnPrefChanged(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = settings_observers_.find(path);
  CHECK(it != settings_observers_.end());
  it->second->Notify();
}

}  // namespace reporting
