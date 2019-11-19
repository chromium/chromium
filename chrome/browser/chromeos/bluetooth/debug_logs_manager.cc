// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/debug_logs_manager.h"

#include "base/feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

namespace bluetooth {

namespace {
const char kVerboseLoggingEnablePrefName[] = "bluetooth.verboseLogging.enable";
}  // namespace

DebugLogsManager::DebugLogsManager(const std::string& primary_user_email,
                                   PrefService* pref_service)
    : primary_user_email_(primary_user_email), pref_service_(pref_service) {}

DebugLogsManager::~DebugLogsManager() = default;

// static
void DebugLogsManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kVerboseLoggingEnablePrefName,
                                false /* default_value */);
}

DebugLogsManager::DebugLogsState DebugLogsManager::GetDebugLogsState() const {
  if (!AreDebugLogsSupported())
    return DebugLogsState::kNotSupported;

  return pref_service_->GetBoolean(kVerboseLoggingEnablePrefName)
             ? DebugLogsState::kSupportedAndEnabled
             : DebugLogsState::kSupportedButDisabled;
}

mojom::DebugLogsChangeHandlerPtr DebugLogsManager::GenerateInterfacePtr() {
  mojom::DebugLogsChangeHandlerPtr interface_ptr;
  bindings_.AddBinding(this, mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void DebugLogsManager::ChangeDebugLogsState(bool should_debug_logs_be_enabled) {
  DCHECK_NE(GetDebugLogsState(), DebugLogsState::kNotSupported);

  pref_service_->SetBoolean(kVerboseLoggingEnablePrefName,
                            should_debug_logs_be_enabled);
  // TODO(crbug.com/734152): On login, enable logs based on this value
}

bool DebugLogsManager::AreDebugLogsSupported() const {
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kShowBluetoothDebugLogToggle)) {
    return false;
  }

  return gaia::IsGoogleInternalAccountEmail(primary_user_email_);
}

}  // namespace bluetooth

}  // namespace chromeos
