// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace bruschetta {

class BruschettaLauncher;

// A service to hold the separate modules that provide Bruschetta
// (third-party/generic VM) support within Chrome (files app integration, app
// service integration, etc).
class BruschettaService : public KeyedService {
 public:
  explicit BruschettaService(Profile* profile);
  ~BruschettaService() override;

  // Helper method to get the service instance for the given profile.
  static BruschettaService* GetForProfile(Profile* profile);

  // Register an existing bruschetta instance with the terminal app.
  void RegisterWithTerminal(const guest_os::GuestId& guest_id);

  // Register a new bruschetta instance in prefs. `config_id` controls which
  // enterprise policy manages this instance.
  void RegisterInPrefs(const guest_os::GuestId& guest_id,
                       const std::string& config_id);

  // Returns a handle to the launcher for the vm specified by `vm_name`. Will
  // return a null pointer if the name isn't recognised.
  base::WeakPtr<BruschettaLauncher> GetLauncher(std::string vm_name);

  void SetLauncherForTesting(std::string vm_name,
                             std::unique_ptr<BruschettaLauncher> launcher);

 private:
  struct VmRegistration {
    std::unique_ptr<BruschettaLauncher> launcher;
    guest_os::GuestOsMountProviderRegistry::Id mount_id;
    // We don't track the terminal registration because that should remain in
    // place even if the VM is blocked from launching.

    VmRegistration(std::unique_ptr<BruschettaLauncher>,
                   guest_os::GuestOsMountProviderRegistry::Id);
    VmRegistration(VmRegistration&&);
    VmRegistration& operator=(VmRegistration&&);
    VmRegistration(const VmRegistration&) = delete;
    VmRegistration& operator=(const VmRegistration&) = delete;
    ~VmRegistration();
  };

  void OnPolicyChanged();
  void AllowLaunch(guest_os::GuestId guest_id);
  void BlockLaunch(guest_os::GuestId guest_id);

  base::flat_map<std::string, VmRegistration> runnable_vms_;

  PrefChangeRegistrar pref_observer_;

  Profile* const profile_;
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_H_
