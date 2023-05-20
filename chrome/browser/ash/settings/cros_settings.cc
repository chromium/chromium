// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/cros_settings.h"

#include <stddef.h>

#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/supervised_user_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/system_settings_provider.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

static CrosSettings* g_cros_settings = nullptr;

// Calling SetForTesting sets this flag. This flag means that the production
// code which calls Initialize and Shutdown will have no effect - the test
// install attributes will remain in place until ShutdownForTesting is called.
bool g_using_cros_settings_for_testing = false;

// static
void CrosSettings::Initialize(PrefService* local_state) {
  // Don't reinitialize if a specific instance has already been set for test.
  if (g_using_cros_settings_for_testing)
    return;

  CHECK(!g_cros_settings);
  g_cros_settings = new CrosSettings(DeviceSettingsService::Get(), local_state);
}

// static
bool CrosSettings::IsInitialized() {
  return g_cros_settings;
}

// static
void CrosSettings::Shutdown() {
  if (g_using_cros_settings_for_testing)
    return;

  DCHECK(g_cros_settings);
  delete g_cros_settings;
  g_cros_settings = nullptr;
}

// static
CrosSettings* CrosSettings::Get() {
  CHECK(g_cros_settings);
  return g_cros_settings;
}

// static
void CrosSettings::SetForTesting(CrosSettings* test_instance) {
  DCHECK(!g_cros_settings);
  DCHECK(!g_using_cros_settings_for_testing);
  g_cros_settings = test_instance;
  g_using_cros_settings_for_testing = true;
}

// static
void CrosSettings::ShutdownForTesting() {
  DCHECK(g_using_cros_settings_for_testing);
  // Don't delete the test instance, we are not the owner.
  g_cros_settings = nullptr;
  g_using_cros_settings_for_testing = false;
}

CrosSettings::CrosSettings() = default;

CrosSettings::CrosSettings(DeviceSettingsService* device_settings_service,
                           PrefService* local_state) {
  CrosSettingsProvider::NotifyObserversCallback notify_cb(
      base::BindRepeating(&CrosSettings::FireObservers,
                          // This is safe since |this| is never deleted.
                          base::Unretained(this)));

  auto supervised_user_cros_provider =
      std::make_unique<SupervisedUserCrosSettingsProvider>(notify_cb);
  supervised_user_cros_settings_provider_ = supervised_user_cros_provider.get();

  AddSettingsProvider(std::move(supervised_user_cros_provider));
  AddSettingsProvider(std::make_unique<DeviceSettingsProvider>(
      notify_cb, device_settings_service, local_state));
  AddSettingsProvider(std::make_unique<SystemSettingsProvider>(notify_cb));
}

CrosSettings::~CrosSettings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool CrosSettings::IsCrosSettings(base::StringPiece path) {
  return base::StartsWith(path, kCrosSettingsPrefix,
                          base::CompareCase::SENSITIVE);
}

const base::Value* CrosSettings::GetPref(base::StringPiece path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CrosSettingsProvider* provider = GetProvider(path);
  if (provider)
    return provider->Get(path);
  NOTREACHED() << path << " preference was not found in the signed settings.";
  return nullptr;
}

CrosSettingsProvider::TrustedStatus CrosSettings::PrepareTrustedValues(
    base::OnceClosure callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < providers_.size(); ++i) {
    CrosSettingsProvider::TrustedStatus status =
        providers_[i]->PrepareTrustedValues(&callback);
    if (status != CrosSettingsProvider::TRUSTED)
      return status;
  }
  return CrosSettingsProvider::TRUSTED;
}

bool CrosSettings::GetBoolean(base::StringPiece path, bool* bool_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_bool()) {
    *bool_value = value->GetBool();
    return true;
  }
  return false;
}

bool CrosSettings::GetInteger(base::StringPiece path, int* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_int()) {
    *out_value = value->GetInt();
    return true;
  }
  return false;
}

bool CrosSettings::GetDouble(base::StringPiece path, double* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `GetIfDouble` incapsulates type check.
  absl::optional<double> maybe_value = GetPref(path)->GetIfDouble();
  if (maybe_value.has_value()) {
    *out_value = maybe_value.value();
    return true;
  }
  return false;
}

bool CrosSettings::GetString(base::StringPiece path,
                             std::string* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_string()) {
    *out_value = value->GetString();
    return true;
  }
  return false;
}

bool CrosSettings::GetList(base::StringPiece path,
                           const base::Value::List** out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_list()) {
    *out_value = &value->GetList();
    return true;
  }
  return false;
}

bool CrosSettings::GetDictionary(base::StringPiece path,
                                 const base::Value::Dict** out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_dict()) {
    *out_value = &value->GetDict();
    return true;
  }
  return false;
}

bool CrosSettings::IsUserAllowlisted(
    const std::string& username,
    bool* wildcard_match,
    const absl::optional<user_manager::UserType>& user_type) const {
  // Skip allowlist check for tests.
  if (switches::ShouldSkipOobePostLogin()) {
    return true;
  }

  bool allow_new_user = false;
  GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return true;

  if (FindEmailInList(kAccountsPrefUsers, username, wildcard_match))
    return true;

  bool family_link_allowed = false;
  GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed, &family_link_allowed);
  return family_link_allowed && user_type == user_manager::USER_TYPE_CHILD;
}

bool CrosSettings::FindEmailInList(const std::string& path,
                                   const std::string& email,
                                   bool* wildcard_match) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value::List* list;
  if (!GetList(path, &list)) {
    if (wildcard_match)
      *wildcard_match = false;
    return false;
  }

  return FindEmailInList(*list, email, wildcard_match);
}

// static
bool CrosSettings::FindEmailInList(const base::Value::List& list,
                                   const std::string& email,
                                   bool* wildcard_match) {
  std::string canonicalized_email(
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(email)));
  std::string wildcard_email;
  std::string::size_type at_pos = canonicalized_email.find('@');
  if (at_pos != std::string::npos) {
    wildcard_email =
        std::string("*").append(canonicalized_email.substr(at_pos));
  }

  if (wildcard_match)
    *wildcard_match = false;

  bool found_wildcard_match = false;
  for (const auto& entry : list) {
    if (!entry.is_string()) {
      NOTREACHED();
      continue;
    }
    std::string canonicalized_entry(
        gaia::CanonicalizeEmail(gaia::SanitizeEmail(entry.GetString())));

    if (canonicalized_entry != wildcard_email &&
        canonicalized_entry == canonicalized_email) {
      return true;
    }

    // If there is a wildcard match, don't exit early. There might be an exact
    // match further down the list that should take precedence if present.
    if (canonicalized_entry == wildcard_email)
      found_wildcard_match = true;
  }

  if (wildcard_match)
    *wildcard_match = found_wildcard_match;

  return found_wildcard_match;
}

bool CrosSettings::AddSettingsProvider(
    std::unique_ptr<CrosSettingsProvider> provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CrosSettingsProvider* provider_ptr = provider.get();
  providers_.push_back(std::move(provider));

  // Allow the provider to notify this object when settings have changed.
  // Providers instantiated inside this class will have the same callback
  // passed to their constructor, but doing it here allows for providers
  // to be instantiated outside this class.
  CrosSettingsProvider::NotifyObserversCallback notify_cb(base::BindRepeating(
      &CrosSettings::FireObservers, base::Unretained(this)));
  provider_ptr->SetNotifyObserversCallback(notify_cb);
  return true;
}

std::unique_ptr<CrosSettingsProvider> CrosSettings::RemoveSettingsProvider(
    CrosSettingsProvider* provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = base::ranges::find(providers_, provider,
                               &std::unique_ptr<CrosSettingsProvider>::get);
  if (it != providers_.end()) {
    std::unique_ptr<CrosSettingsProvider> ptr = std::move(*it);
    providers_.erase(it);
    return ptr;
  }
  return nullptr;
}

base::CallbackListSubscription CrosSettings::AddSettingsObserver(
    const std::string& path,
    base::RepeatingClosure callback) {
  DCHECK(!path.empty());
  DCHECK(callback);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetProvider(path));

  // Get the callback registry associated with the path.
  base::RepeatingClosureList* registry = nullptr;
  auto observer_iterator = settings_observers_.find(path);
  if (observer_iterator == settings_observers_.end()) {
    settings_observers_[path] = std::make_unique<base::RepeatingClosureList>();
    registry = settings_observers_[path].get();
  } else {
    registry = observer_iterator->second.get();
  }

  return registry->Add(std::move(callback));
}

CrosSettingsProvider* CrosSettings::GetProvider(base::StringPiece path) const {
  for (const auto& provider : providers_) {
    if (provider->HandlesSetting(path)) {
      return provider.get();
    }
  }
  return nullptr;
}

void CrosSettings::FireObservers(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto observer_iterator = settings_observers_.find(path);
  if (observer_iterator == settings_observers_.end())
    return;

  observer_iterator->second->Notify();
}

ScopedTestCrosSettings::ScopedTestCrosSettings(PrefService* local_state) {
  CrosSettings::Initialize(local_state);
}

ScopedTestCrosSettings::~ScopedTestCrosSettings() {
  CrosSettings::Shutdown();
}

}  // namespace ash
