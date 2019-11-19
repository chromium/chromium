// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCHEDULER_CONFIGURATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SCHEDULER_CONFIGURATION_MANAGER_H_

#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/system/scheduler_configuration_manager_base.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

class DebugDaemonClient;

// Tracks scheduler configuration as provided by the respective local state pref
// and sends D-Bus IPC to reconfigure the system on config changes.
//
// This is the policy to enable and disable Hyper-Threading (H/T) on Intel CPUs.
// Conservative -> Hyper-Threading disabled.
// Performance -> Hyper-Threading enabled.
// For more information on why H/T is configurable, see
// https://www.chromium.org/chromium-os/mds-on-chromeos
//
class SchedulerConfigurationManager : public SchedulerConfigurationManagerBase {
 public:
  SchedulerConfigurationManager(DebugDaemonClient* debug_daemon_client,
                                PrefService* local_state);
  ~SchedulerConfigurationManager() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // SchedulerConfigurationManagerBase overrides:
  base::Optional<std::pair<bool, size_t>> GetLastReply() const override;

 private:
  void OnDebugDaemonReady(bool service_is_ready);
  void OnPrefChange();
  void OnConfigurationSet(bool result, size_t num_cores_disabled);

  DebugDaemonClient* debug_daemon_client_ = nullptr;
  PrefChangeRegistrar observer_;
  bool debug_daemon_ready_ = false;
  base::Optional<std::pair<bool, size_t>> last_reply_;

  base::WeakPtrFactory<SchedulerConfigurationManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SchedulerConfigurationManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCHEDULER_CONFIGURATION_MANAGER_H_
