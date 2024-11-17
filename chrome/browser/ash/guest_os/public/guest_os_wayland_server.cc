// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_security_delegate.h"
#include "chrome/browser/ash/crostini/crostini_security_delegate.h"
#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/vm_wl/wl.pb.h"
#include "components/exo/server/wayland_server_controller.h"
#include "components/exo/server/wayland_server_handle.h"

namespace guest_os {

GuestOsWaylandServer::ScopedServer::ScopedServer(
    std::unique_ptr<exo::WaylandServerHandle> handle,
    base::WeakPtr<GuestOsSecurityDelegate> security_delegate)
    : handle_(std::move(handle)), security_delegate_(security_delegate) {}

GuestOsWaylandServer::ScopedServer::~ScopedServer() = default;

// static
void GuestOsWaylandServer::ListenOnSocket(
    const vm_tools::wl::ListenOnSocketRequest& request,
    base::ScopedFD socket_fd,
    base::OnceCallback<void(std::optional<std::string>)> response_callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || ash::ProfileHelper::GetUserIdHashFromProfile(profile) !=
                      request.desc().owner_id()) {
    std::move(response_callback).Run({"Invalid owner_id"});
    return;
  }
  GuestOsServiceFactory::GetForProfile(profile)->WaylandServer()->Listen(
      std::move(socket_fd), request.desc().type(), request.desc().name(),
      std::move(response_callback));
}

// static
void GuestOsWaylandServer::CloseSocket(
    const vm_tools::wl::CloseSocketRequest& request,
    base::OnceCallback<void(std::optional<std::string>)> response_callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || ash::ProfileHelper::GetUserIdHashFromProfile(profile) !=
                      request.desc().owner_id()) {
    std::move(response_callback).Run({"Invalid owner_id"});
    return;
  }
  GuestOsServiceFactory::GetForProfile(profile)->WaylandServer()->Close(
      request.desc().type(), request.desc().name(),
      std::move(response_callback));
}

GuestOsWaylandServer::GuestOsWaylandServer(Profile* profile)
    : profile_(profile) {
  // Cleanup is best-effort, so don't bother if for some reason we
  // can't get a handle to the service (like tests).
  if (auto* concierge = ash::ConciergeClient::Get(); concierge) {
    concierge->AddObserver(this);
  }
}

GuestOsWaylandServer::~GuestOsWaylandServer() {
  // ConciergeClient may be destroyed prior to GuestOsWaylandServer in tests.
  // Therefore we do this instead of ScopedObservation.
  if (auto* concierge = ash::ConciergeClient::Get(); concierge) {
    concierge->RemoveObserver(this);
  }
}

// Returns a weak handle to the security delegate for the VM with the given
// |name| and |type|, if one exists, and nullptr otherwise.
base::WeakPtr<GuestOsSecurityDelegate> GuestOsWaylandServer::GetDelegate(
    vm_tools::apps::VmType type,
    const std::string& name) const {
  auto type_iter = servers_.find(type);
  if (type_iter == servers_.end()) {
    return nullptr;
  }
  auto name_iter = type_iter->second.find(name);
  if (name_iter == type_iter->second.end()) {
    return nullptr;
  }
  return name_iter->second->security_delegate();
}

void GuestOsWaylandServer::Listen(base::ScopedFD fd,
                                  vm_tools::apps::VmType type,
                                  const std::string& name,
                                  ResponseCallback callback) {
  if (servers_[type].erase(name) > 0) {
    LOG(WARNING) << "Re-binding wayland server for " << name << "(type=" << type
                 << ") while in-use";
  }
  switch (type) {
    case vm_tools::apps::TERMINA:
      crostini::CrostiniSecurityDelegate::Build(
          profile_, name,
          base::BindOnce(&GuestOsWaylandServer::OnSecurityDelegateCreated,
                         weak_factory_.GetWeakPtr(), std::move(fd), type, name,
                         std::move(callback)));
      return;
    case vm_tools::apps::BOREALIS:
      borealis::BorealisSecurityDelegate::Build(
          profile_, name,
          base::BindOnce(&GuestOsWaylandServer::OnSecurityDelegateCreated,
                         weak_factory_.GetWeakPtr(), std::move(fd), type, name,
                         std::move(callback)));
      return;
    default:
      // For all other VMs, provide the minimal capability-set.
      OnSecurityDelegateCreated(
          std::move(fd), type, name, std::move(callback),
          std::make_unique<GuestOsSecurityDelegate>(name));
      return;
  }
}

void GuestOsWaylandServer::Close(vm_tools::apps::VmType type,
                                 const std::string& name,
                                 ResponseCallback callback) {
  if (servers_[type].erase(name) == 0) {
    LOG(WARNING) << "Trying to close non-existent server for " << name
                 << "(type=" << type << ")";
  }
  std::move(callback).Run(std::nullopt);
}

void GuestOsWaylandServer::OnSecurityDelegateCreated(
    base::ScopedFD fd,
    vm_tools::apps::VmType type,
    std::string name,
    ResponseCallback callback,
    std::unique_ptr<GuestOsSecurityDelegate> delegate) {
  if (!delegate) {
    std::move(callback).Run({"Failed to get security privileges"});
    return;
  }
  GuestOsSecurityDelegate::MakeServerWithFd(
      std::move(delegate), std::move(fd),
      base::BindOnce(&GuestOsWaylandServer::OnServerCreated,
                     weak_factory_.GetWeakPtr(), type, std::move(name),
                     std::move(callback)));
}

void GuestOsWaylandServer::OnServerCreated(
    vm_tools::apps::VmType type,
    std::string name,
    ResponseCallback callback,
    base::WeakPtr<GuestOsSecurityDelegate> delegate,
    std::unique_ptr<exo::WaylandServerHandle> handle) {
  if (!handle) {
    std::move(callback).Run({"Failed to create wayland server"});
    return;
  }
  servers_[type].insert_or_assign(
      std::move(name),
      std::make_unique<ScopedServer>(std::move(handle), delegate));
  std::move(callback).Run(std::nullopt);
}

void GuestOsWaylandServer::ConciergeServiceStarted() {
  // Do nothing.
}

void GuestOsWaylandServer::ConciergeServiceStopped() {
  servers_.clear();
}

}  // namespace guest_os
