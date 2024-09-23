// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/local_state_reporting_settings.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace reporting {

LocalStateReportingSettings::LocalStateReportingSettings() {
  CHECK(g_browser_process);
  CHECK(g_browser_process->local_state());
  pref_change_registrar_.Init(g_browser_process->local_state());
}

LocalStateReportingSettings::~LocalStateReportingSettings() = default;

base::CallbackListSubscription LocalStateReportingSettings::AddSettingsObserver(
    const std::string& path,
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(g_browser_process->local_state()->FindPreference(path));
  CHECK(callback);

  auto [iterator, added_element] = settings_observers_.emplace(
      path, std::make_unique<base::RepeatingClosureList>());
  if (added_element) {
    // Initialize the pref change registrar for the specified path.
    pref_change_registrar_.Add(
        path, base::BindRepeating(&LocalStateReportingSettings::OnPrefChanged,
                                  weak_ptr_factory_.GetWeakPtr(), path));
  }
  // Add the callback to the pre-existing list.
  return iterator->second->Add(callback);
}

bool LocalStateReportingSettings::PrepareTrustedValues(
    base::OnceClosure callback) {
  // We trust user setting values in the pref store, so we return true.
  return true;
}

bool LocalStateReportingSettings::GetBoolean(const std::string& path,
                                             bool* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* const pref_service = g_browser_process->local_state();
  if (!pref_service->FindPreference(path) ||
      !pref_service->GetValue(path).is_bool()) {
    // Invalid path or data type.
    return false;
  }
  *out_value = pref_service->GetBoolean(path);
  return true;
}

bool LocalStateReportingSettings::GetInteger(const std::string& path,
                                             int* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* const pref_service = g_browser_process->local_state();
  if (!pref_service->FindPreference(path) ||
      !pref_service->GetValue(path).is_int()) {
    // Invalid path or data type.
    return false;
  }
  *out_value = pref_service->GetInteger(path);
  return true;
}

bool LocalStateReportingSettings::GetList(
    const std::string& path,
    const base::Value::List** out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* const pref_service = g_browser_process->local_state();
  if (!pref_service->FindPreference(path) ||
      !pref_service->GetValue(path).is_list()) {
    // Invalid path or data type.
    return false;
  }
  *out_value = &pref_service->GetList(path);
  return true;
}

bool LocalStateReportingSettings::GetReportingEnabled(const std::string& path,
                                                      bool* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value::List* list_value;
  if (GetBoolean(path, out_value)) {
    return true;
  } else if (GetList(path, &list_value)) {
    *out_value = !list_value->empty();
    return true;
  }

  return false;
}

bool LocalStateReportingSettings::IsObservingSettingsForTest(
    const std::string& path) {
  return pref_change_registrar_.IsObserved(path);
}

void LocalStateReportingSettings::OnPrefChanged(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = settings_observers_.find(path);
  CHECK(it != settings_observers_.end());
  it->second->Notify();
}

}  // namespace reporting
