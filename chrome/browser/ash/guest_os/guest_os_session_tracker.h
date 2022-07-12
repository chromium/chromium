// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SESSION_TRACKER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SESSION_TRACKER_H_

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"

namespace guest_os {

struct GuestInfo {
  GuestInfo(GuestId guest_id,
            int64_t cid,
            std::string username,
            base::FilePath homedir,
            std::string ipv4_address);
  ~GuestInfo();
  GuestInfo(GuestInfo&&);
  GuestInfo(const GuestInfo&);
  GuestInfo& operator=(GuestInfo&&);
  GuestInfo& operator=(const GuestInfo&);
  GuestId guest_id;
  int64_t cid;
  std::string username;
  base::FilePath homedir;
  std::string ipv4_address;
};

class GuestOsSessionTracker : protected ash::ConciergeClient::VmObserver,
                              protected ash::CiceroneClient::Observer,
                              public KeyedService {
 public:
  static GuestOsSessionTracker* GetForProfile(Profile* profile);
  explicit GuestOsSessionTracker(std::string owner_id);
  ~GuestOsSessionTracker() override;

  // Runs `callback` when the OnContainerStarted signal arrives for the guest
  // with the given `id`. To cancel the callback (e.g. upon timeout) destroy the
  // returned subscription.
  // TODO(b/231390254): If Chrome crashes while a container is running then
  // we'll never get another OnContainerStarted message, which means
  // RunOnceContainerStarted hangs forever. We need to list running containers
  // and adopt them, the same as we do for VMs.
  base::CallbackListSubscription RunOnceContainerStarted(
      GuestId id,
      base::OnceCallback<void(GuestInfo)> callback);

  // Returns information about a running guest. Returns nullopt if the guest
  // isn't recognised e.g. it's not running.
  absl::optional<GuestInfo> GetInfo(const GuestId& id);

  void AddGuestForTesting(const GuestId& id, const GuestInfo& info);

 protected:
  // ash::ConciergeClient::VmObserver overrides.
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  // ash::CiceroneClient::Observer overrides.
  void OnContainerStarted(
      const vm_tools::cicerone::ContainerStartedSignal& signal) override;
  void OnContainerShutdown(
      const vm_tools::cicerone::ContainerShutdownSignal& signal) override;

 private:
  void OnListVms(absl::optional<vm_tools::concierge::ListVmsResponse> response);
  std::string owner_id_;
  base::flat_map<std::string, vm_tools::concierge::VmInfo> vms_;
  base::flat_map<GuestId, GuestInfo> guests_;

  base::flat_map<GuestId,
                 std::unique_ptr<base::OnceCallbackList<void(GuestInfo)>>>
      container_start_callbacks_;

  base::WeakPtrFactory<GuestOsSessionTracker> weak_ptr_factory_{this};
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SESSION_TRACKER_H_
