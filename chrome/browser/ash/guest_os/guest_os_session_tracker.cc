// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"

#include <memory>

#include "base/callback_list.h"
#include "base/containers/flat_tree.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "content/public/browser/browser_thread.h"

namespace guest_os {

GuestInfo::GuestInfo(GuestId guest_id,
                     int64_t cid,
                     std::string username,
                     base::FilePath homedir,
                     std::string ipv4_address,
                     uint32_t sftp_vsock_port)
    : guest_id(std::move(guest_id)),
      cid(cid),
      username(std::move(username)),
      homedir(std::move(homedir)),
      ipv4_address(std::move(ipv4_address)),
      sftp_vsock_port(sftp_vsock_port) {}
GuestInfo::~GuestInfo() = default;
GuestInfo::GuestInfo(GuestInfo&&) = default;
GuestInfo::GuestInfo(const GuestInfo&) = default;
GuestInfo& GuestInfo::operator=(GuestInfo&&) = default;
GuestInfo& GuestInfo::operator=(const GuestInfo&) = default;

GuestOsSessionTracker::GuestOsSessionTracker(std::string owner_id)
    : owner_id_(std::move(owner_id)) {
  if (!ash::ConciergeClient::Get() || !ash::CiceroneClient::Get()) {
    // These're null in unit tests unless explicitly set up. If missing, don't
    // register as an observer.
    return;
  }
  ash::ConciergeClient::Get()->AddVmObserver(this);
  ash::CiceroneClient::Get()->AddObserver(this);
  vm_tools::concierge::ListVmsRequest request;
  request.set_owner_id(owner_id_);
  ash::ConciergeClient::Get()->ListVms(
      request, base::BindOnce(&GuestOsSessionTracker::OnListVms,
                              weak_ptr_factory_.GetWeakPtr()));
}

GuestOsSessionTracker::~GuestOsSessionTracker() {
  if (!ash::ConciergeClient::Get() || !ash::CiceroneClient::Get()) {
    return;
  }
  ash::ConciergeClient::Get()->RemoveVmObserver(this);
  ash::CiceroneClient::Get()->RemoveObserver(this);
}

void GuestOsSessionTracker::OnListVms(
    std::optional<vm_tools::concierge::ListVmsResponse> response) {
  if (!response) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      LOG(ERROR)
          << "Failed to list VMs, assuming there aren't any already running";
    }
    return;
  }
  for (const auto& vm : response->vms()) {
    vms_[vm.name()] = vm.vm_info();
  }
  vm_tools::cicerone::ListRunningContainersRequest req;
  req.set_owner_id(owner_id_);
  ash::CiceroneClient::Get()->ListRunningContainers(
      req, base::BindOnce(&GuestOsSessionTracker::OnListRunningContainers,
                          weak_ptr_factory_.GetWeakPtr()));
}

void GuestOsSessionTracker::OnListRunningContainers(
    std::optional<vm_tools::cicerone::ListRunningContainersResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to list containers, assuming there aren't any "
                  "already running";
    return;
  }
  for (const auto& container : response->containers()) {
    if (!vms_.contains(container.vm_name())) {
      continue;
    }
    vm_tools::cicerone::GetGarconSessionInfoRequest req;
    req.set_owner_id(owner_id_);
    req.set_vm_name(container.vm_name());
    req.set_container_name(container.container_name());
    ash::CiceroneClient::Get()->GetGarconSessionInfo(
        req, base::BindOnce(&GuestOsSessionTracker::OnGetGarconSessionInfo,
                            weak_ptr_factory_.GetWeakPtr(), container.vm_name(),
                            container.container_name(),
                            container.container_token()));
  }
}

void GuestOsSessionTracker::OnGetGarconSessionInfo(
    std::string vm_name,
    std::string container_name,
    std::string container_token,
    std::optional<vm_tools::cicerone::GetGarconSessionInfoResponse> response) {
  if (!response ||
      response->status() !=
          vm_tools::cicerone::GetGarconSessionInfoResponse::SUCCEEDED) {
    LOG(ERROR) << "Unable to get session info";
    return;
  }
  // Don't need ipv4 address yet so haven't plumbed it through. Once we get
  // around to port forwarding or similar we'll need it though.
  HandleNewGuest(vm_name, container_name, container_token,
                 response->container_username(), response->container_homedir(),
                 "", response->sftp_vsock_port());
}

// Returns information about a running guest. Returns nullopt if the guest
// isn't recognised e.g. it's not running.
std::optional<GuestInfo> GuestOsSessionTracker::GetInfo(const GuestId& id) {
  auto iter = guests_.find(id);
  if (iter == guests_.end()) {
    return std::nullopt;
  }
  return iter->second;
}

std::optional<vm_tools::concierge::VmInfo> GuestOsSessionTracker::GetVmInfo(
    const std::string& vm_name) {
  auto iter = vms_.find(vm_name);
  if (iter == vms_.end()) {
    return std::nullopt;
  }
  return iter->second;
}

std::optional<GuestId> GuestOsSessionTracker::GetGuestIdForToken(
    const std::string& container_token) {
  if (container_token.empty()) {
    return std::nullopt;
  }

  auto iter = tokens_to_guests_.find(container_token);
  if (iter == tokens_to_guests_.end()) {
    return std::nullopt;
  }
  return iter->second;
}

bool GuestOsSessionTracker::IsRunning(const GuestId& id) {
  return guests_.contains(id);
}

bool GuestOsSessionTracker::IsVmStopping(const std::string& vm_name) {
  return stopping_vms_.contains(vm_name);
}

// ash::ConciergeClient::VmObserver overrides.
void GuestOsSessionTracker::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (signal.owner_id() != owner_id_) {
    return;
  }
  vms_[signal.name()] = signal.vm_info();
}

void GuestOsSessionTracker::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (signal.owner_id() != owner_id_) {
    return;
  }
  vms_.erase(signal.name());
  stopping_vms_.erase(signal.name());
  std::vector<GuestId> ids;
  for (const auto& pair : guests_) {
    if (pair.first.vm_name != signal.name()) {
      continue;
    }
    ids.push_back(pair.first);
  }
  for (const auto& id : ids) {
    HandleContainerShutdown(id.vm_name, id.container_name);
  }
}

void GuestOsSessionTracker::OnVmStopping(
    const vm_tools::concierge::VmStoppingSignal& signal) {
  stopping_vms_.insert(signal.name());
}

// ash::CiceroneClient::Observer overrides.
void GuestOsSessionTracker::OnContainerStarted(
    const vm_tools::cicerone::ContainerStartedSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (signal.owner_id() != owner_id_) {
    return;
  }
  HandleNewGuest(signal.vm_name(), signal.container_name(),
                 signal.container_token(), signal.container_username(),
                 signal.container_homedir(), signal.ipv4_address(),
                 signal.sftp_vsock_port());
}

void GuestOsSessionTracker::HandleNewGuest(const std::string& vm_name,
                                           const std::string& container_name,
                                           const std::string& container_token,
                                           const std::string& username,
                                           const std::string& homedir,
                                           const std::string& ipv4_address,
                                           const uint32_t& sftp_vsock_port) {
  auto iter = vms_.find(vm_name);
  if (iter == vms_.end()) {
    LOG(ERROR)
        << "Received ContainerStarted signal for an unexpected VM, ignoring.";
    return;
  }
  vm_tools::apps::VmType vm_type = ToVmType(iter->second.vm_type());
  // TODO(b/294316866): Special-case Bruschetta VMs until cicerone is updated to
  // use the correct vm_type.
  if (vm_name == bruschetta::kBruschettaVmName) {
    vm_type = vm_tools::apps::VmType::BRUSCHETTA;
  }
  GuestId id{vm_type, vm_name, container_name};
  GuestInfo info{id,           iter->second.cid(),
                 username,     base::FilePath(homedir),
                 ipv4_address, sftp_vsock_port};
  guests_.insert_or_assign(id, info);

  if (container_token.length() == 0) {
    LOG(ERROR) << "Received ContainerStarted signal with no container token "
                  "specified.";
  } else {
    tokens_to_guests_.emplace(container_token, id);
  }

  // If there're any pending container start callbacks for this guest run them.
  auto cb_list = container_start_callbacks_.find(id);
  if (cb_list != container_start_callbacks_.end()) {
    cb_list->second->Notify(info);
  }
  // Let the observers of ANY guest know too.
  for (auto& observer : container_started_observers_) {
    observer.OnContainerStarted(id);
  }
}

void GuestOsSessionTracker::OnContainerShutdown(
    const vm_tools::cicerone::ContainerShutdownSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (signal.owner_id() != owner_id_) {
    return;
  }
  HandleContainerShutdown(signal.vm_name(), signal.container_name());
}

void GuestOsSessionTracker::HandleContainerShutdown(
    const std::string& vm_name,
    const std::string& container_name) {
  GuestId id{VmType::UNKNOWN, vm_name, container_name};
  guests_.erase(id);

  auto iter = std::find_if(tokens_to_guests_.begin(), tokens_to_guests_.end(),
                           [&id](const auto& it) { return it.second == id; });
  if (iter == tokens_to_guests_.end()) {
    LOG(ERROR) << "Attempted to remove token from the map which does not "
                  "exist, ignoring";
  } else {
    tokens_to_guests_.erase(iter);
  }

  auto cb_list = container_shutdown_callbacks_.find(id);
  if (cb_list != container_shutdown_callbacks_.end()) {
    cb_list->second->Notify();
  }
}

base::CallbackListSubscription GuestOsSessionTracker::RunOnceContainerStarted(
    const GuestId& id,
    base::OnceCallback<void(GuestInfo)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto iter = guests_.find(id);
  if (iter != guests_.end()) {
    std::move(callback).Run(iter->second);
    return base::CallbackListSubscription();
  }
  auto& cb_list = container_start_callbacks_[id];
  if (!cb_list) {
    cb_list = std::make_unique<base::OnceCallbackList<void(GuestInfo)>>();
  }
  return cb_list->Add(std::move(callback));
}

void GuestOsSessionTracker::AddGuestForTesting(const GuestId& id,
                                               const GuestInfo& info,
                                               bool notify,
                                               const std::string& token) {
  vm_tools::concierge::VmInfo vm_info;
  vm_info.set_cid(info.cid);
  vms_.insert_or_assign(id.vm_name, vm_info);
  guests_.insert_or_assign(id, info);
  tokens_to_guests_.insert_or_assign(token, id);
  if (notify) {
    for (auto& observer : container_started_observers_) {
      observer.OnContainerStarted(id);
    }
  }
}

void GuestOsSessionTracker::AddGuestForTesting(const GuestId& id,
                                               const std::string& token) {
  AddGuestForTesting(id, GuestInfo{id, {}, {}, {}, {}, {}}, false, token);
}

base::CallbackListSubscription GuestOsSessionTracker::RunOnShutdown(
    const GuestId& id,
    base::OnceCallback<void()> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto& cb_list = container_shutdown_callbacks_[id];
  if (!cb_list) {
    cb_list = std::make_unique<base::OnceCallbackList<void()>>();
  }
  return cb_list->Add(std::move(callback));
}

void GuestOsSessionTracker::AddContainerStartedObserver(
    ContainerStartedObserver* observer) {
  container_started_observers_.AddObserver(observer);
}

void GuestOsSessionTracker::RemoveContainerStartedObserver(
    ContainerStartedObserver* observer) {
  container_started_observers_.RemoveObserver(observer);
}

}  // namespace guest_os
