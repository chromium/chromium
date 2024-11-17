// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SHARED_DEVICES_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SHARED_DEVICES_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"

class Profile;

namespace crostini {

// CrostiniSharedDevices keeps track of the sharing status of non-usb devices
// that may be available to containers in a virtual machine (hence "vm_device").
// State is kept in sync with user prefs such that when a container is running
// or starts running, the "true" sharing value as determined by Cicerone is
// reflected in prefs.
class CrostiniSharedDevices : public KeyedService,
                              public guest_os::ContainerStartedObserver {
 public:
  explicit CrostiniSharedDevices(Profile* profile);

  CrostiniSharedDevices(const CrostiniSharedDevices&) = delete;
  CrostiniSharedDevices& operator=(const CrostiniSharedDevices&) = delete;

  ~CrostiniSharedDevices() override;

  // ResultCallback's bool argument is true if the we attempted apply the
  // sharing state via Cicerone.
  using ResultCallback = base::OnceCallback<void(bool)>;

  // Reads user prefs to determine whether a |vm_device| is shared.
  // |vm_device| as a string should a lower-case enum name in VmDevice in
  // cicerone_service.proto, e.g. "microphone".
  bool IsVmDeviceShared(guest_os::GuestId container_id,
                        const std::string& vm_device);

  // Stores |shared| value for |vm_device| in user prefs after attempting to
  // apply this to |container_id|. If it could not be set as desired, stores the
  // actual sharing value as known to Cicerone.
  void SetVmDeviceShared(guest_os::GuestId container_id,
                         const std::string& vm_device,
                         bool shared,
                         ResultCallback callback);

 private:
  void ApplySharingState(guest_os::GuestId container_id,
                         base::Value::Dict next_shared_devices,
                         ResultCallback callback);

  // guest_os::ContainerStartedObserver
  void OnContainerStarted(const guest_os::GuestId& container_id) override;

  void OnUpdateContainerDevices(
      const guest_os::GuestId container_id,
      base::Value::Dict next_shared_devices,
      ResultCallback callback,
      std::optional<vm_tools::cicerone::UpdateContainerDevicesResponse>
          response);

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<CrostiniSharedDevices> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SHARED_DEVICES_H_
