// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scheduler_configuration_manager.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace chromeos {

namespace {
constexpr base::FeatureParam<std::string> kSchedulerConfigurationParam{
    &features::kSchedulerConfiguration, "config", ""};
}  // namespace

SchedulerConfigurationManager::SchedulerConfigurationManager(
    DebugDaemonClient* debug_daemon_client,
    PrefService* local_state)
    : debug_daemon_client_(debug_daemon_client) {
  observer_.Init(local_state);
  observer_.Add(
      prefs::kSchedulerConfiguration,
      base::BindRepeating(&SchedulerConfigurationManager::OnPrefChange,
                          base::Unretained(this)));
  debug_daemon_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&SchedulerConfigurationManager::OnDebugDaemonReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

SchedulerConfigurationManager::~SchedulerConfigurationManager() {}

// static
void SchedulerConfigurationManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Ideally the pref would be registered specifying the default provided via
  // the feature parameter. This is unfortunately not possible though because
  // the feature API initialization depends on the local state PrefService, so
  // this function runs before feature parameters are available.
  registry->RegisterStringPref(prefs::kSchedulerConfiguration, std::string());
}

base::Optional<std::pair<bool, size_t>>
SchedulerConfigurationManager::GetLastReply() const {
  return last_reply_;
}

void SchedulerConfigurationManager::OnDebugDaemonReady(bool service_is_ready) {
  if (!service_is_ready) {
    LOG(ERROR) << "Debug daemon unavailable";
    return;
  }

  // Initialize the system.
  debug_daemon_ready_ = true;
  OnPrefChange();
}

void SchedulerConfigurationManager::OnPrefChange() {
  // No point in calling debugd if it isn't ready yet. The ready callback will
  // will call this function again to set the initial configuration.
  if (!debug_daemon_ready_) {
    return;
  }

  // Determine the effective configuration name. Prefer the value from local
  // state if present, feature parameter if otherwise, hard-coded default if
  // no configuration is present.
  std::string config_name;
  PrefService* local_state = observer_.prefs();
  std::string feature_param_value = kSchedulerConfigurationParam.Get();
  if (local_state->HasPrefPath(prefs::kSchedulerConfiguration)) {
    config_name = local_state->GetString(prefs::kSchedulerConfiguration);
  } else if (!feature_param_value.empty()) {
    config_name = feature_param_value;
  } else {
    config_name = debugd::scheduler_configuration::kConservativeScheduler;
  }

  // NB: Also send an update when the config gets reset to let the system pick
  // whatever default. Note that the value we read in this case will be the
  // default specified on pref registration, e.g. empty string.
  debug_daemon_client_->SetSchedulerConfigurationV2(
      config_name,
      /*lock_policy=*/false,
      base::BindOnce(&SchedulerConfigurationManager::OnConfigurationSet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SchedulerConfigurationManager::OnConfigurationSet(
    bool result,
    size_t num_cores_disabled) {
  last_reply_ = std::make_pair(result, num_cores_disabled);

  if (result) {
    VLOG(1) << num_cores_disabled << " logical CPU cores are disabled";
  } else {
    LOG(ERROR) << "Failed to update scheduler configuration";
  }
  for (Observer& obs : observer_list_)
    obs.OnConfigurationSet(result, num_cores_disabled);
}

}  // namespace chromeos
