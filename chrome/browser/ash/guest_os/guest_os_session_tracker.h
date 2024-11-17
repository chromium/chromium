// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SESSION_TRACKER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SESSION_TRACKER_H_

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"

namespace guest_os {

struct GuestInfo {
  GuestInfo(GuestId guest_id,
            int64_t cid,
            std::string username,
            base::FilePath homedir,
            std::string ipv4_address,
            uint32_t sftp_vsock_port);
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
  uint32_t sftp_vsock_port;
};

class ContainerStartedObserver : public base::CheckedObserver {
 public:
  // Called when the container has started.
  virtual void OnContainerStarted(const guest_os::GuestId& container_id) = 0;
};

class GuestOsSessionTracker : protected ash::ConciergeClient::VmObserver,
                              protected ash::CiceroneClient::Observer,
                              public KeyedService {
 public:
  explicit GuestOsSessionTracker(std::string owner_id);
  ~GuestOsSessionTracker() override;

  // Runs `callback` when the OnContainerStarted signal arrives for the guest
  // with the given `id`. To cancel the callback (e.g. upon timeout) destroy the
  // returned subscription.
  base::CallbackListSubscription RunOnceContainerStarted(
      const GuestId& id,
      base::OnceCallback<void(GuestInfo)> callback);

  // Runs `callback` when the guest identified by `id` shuts down. To cancel the
  // callback (e.g. upon timeout) destroy the returned subscription.
  base::CallbackListSubscription RunOnShutdown(
      const GuestId& id,
      base::OnceCallback<void()> callback);

  // Returns information about a running guest. Returns nullopt if the guest
  // isn't recognised e.g. it's not running. If you just want to check if a
  // guest is running or not and don't need the info, use `IsRunning` instead
  std::optional<GuestInfo> GetInfo(const GuestId& id);

  // Returns information about a running VM. Returns nullopt if the VM
  // isn't recognised e.g. it's not running.
  std::optional<vm_tools::concierge::VmInfo> GetVmInfo(
      const std::string& vm_name);

  // Given a container_token for a running guest, returns its GuestId. Returns
  // nullopt if the token isn't recognised.
  std::optional<GuestId> GetGuestIdForToken(const std::string& container_token);

  // Returns true if a guest is running, false otherwise.
  bool IsRunning(const GuestId& id);

  // Returns true if a VM is known to be shutting down, false for running or
  // stopped.
  bool IsVmStopping(const std::string& vm_name);

  void AddGuestForTesting(const GuestId& id,
                          const std::string& token = "test_token");
  void AddGuestForTesting(const GuestId& id,
                          const GuestInfo& info,
                          bool notify = false,
                          const std::string& token = "test_token");

  void AddContainerStartedObserver(ContainerStartedObserver* observer);
  void RemoveContainerStartedObserver(ContainerStartedObserver* observer);

 protected:
  // ash::ConciergeClient::VmObserver overrides.
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;
  void OnVmStopping(
      const vm_tools::concierge::VmStoppingSignal& signal) override;

  // ash::CiceroneClient::Observer overrides.
  void OnContainerStarted(
      const vm_tools::cicerone::ContainerStartedSignal& signal) override;
  void OnContainerShutdown(
      const vm_tools::cicerone::ContainerShutdownSignal& signal) override;

 private:
  void OnListVms(std::optional<vm_tools::concierge::ListVmsResponse> response);
  void OnListRunningContainers(
      std::optional<vm_tools::cicerone::ListRunningContainersResponse>
          response);
  void OnGetGarconSessionInfo(
      std::string vm_name,
      std::string container_name,
      std::string container_token,
      std::optional<vm_tools::cicerone::GetGarconSessionInfoResponse> response);
  void HandleNewGuest(const std::string& vm_name,
                      const std::string& container_name,
                      const std::string& container_token,
                      const std::string& username,
                      const std::string& homedir,
                      const std::string& ipv4_address,
                      const uint32_t& sftp_vsock_port);
  void HandleContainerShutdown(const std::string& vm_name,
                               const std::string& container_name);
  std::string owner_id_;
  base::flat_map<std::string, vm_tools::concierge::VmInfo> vms_;
  base::flat_set<std::string> stopping_vms_;
  base::flat_map<GuestId, GuestInfo> guests_;
  base::flat_map<std::string, GuestId> tokens_to_guests_;

  base::flat_map<GuestId,
                 std::unique_ptr<base::OnceCallbackList<void(GuestInfo)>>>
      container_start_callbacks_;
  base::flat_map<GuestId, std::unique_ptr<base::OnceCallbackList<void()>>>
      container_shutdown_callbacks_;

  base::ObserverList<ContainerStartedObserver> container_started_observers_;

  base::WeakPtrFactory<GuestOsSessionTracker> weak_ptr_factory_{this};
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SESSION_TRACKER_H_
