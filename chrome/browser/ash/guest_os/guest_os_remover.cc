// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_remover.h"

#include <string>
#include <utility>

#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace guest_os {

GuestOsRemover::GuestOsRemover(Profile* profile,
                               guest_os::VmType vm_type,
                               std::string vm_name,
                               base::OnceCallback<void(Result)> callback)
    : profile_(profile),
      vm_type_(vm_type),
      vm_name_(std::move(vm_name)),
      callback_(std::move(callback)) {}

GuestOsRemover::~GuestOsRemover() = default;

void GuestOsRemover::RemoveVm() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  vm_tools::concierge::StopVmRequest request;
  request.set_owner_id(ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_name(vm_name_);

  ash::ConciergeClient::Get()->StopVm(
      std::move(request),
      base::BindOnce(&GuestOsRemover::StopVmFinished, this));
}

void GuestOsRemover::StopVmFinished(
    std::optional<vm_tools::concierge::StopVmResponse> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    LOG(ERROR) << "Failed to stop termina vm. Empty response.";
    std::move(callback_).Run(Result::kStopVmNoResponse);
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to stop VM: " << response->failure_reason();
    std::move(callback_).Run(Result::kStopVmFailed);
    return;
  }

  VLOG(1) << "Clearing application list";
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
      ->ClearApplicationList(vm_type_, vm_name_, "");
  guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile_)
      ->ClearMimeTypes(vm_name_, "");
  VLOG(1) << "Destroying disk image";

  vm_tools::concierge::DestroyDiskImageRequest request;
  request.set_cryptohome_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_vm_name(std::move(vm_name_));

  ash::ConciergeClient::Get()->DestroyDiskImage(
      std::move(request),
      base::BindOnce(&GuestOsRemover::DestroyDiskImageFinished, this));
}

void GuestOsRemover::DestroyDiskImageFinished(
    std::optional<vm_tools::concierge::DestroyDiskImageResponse> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!response) {
    LOG(ERROR) << "Failed to destroy disk image. Empty response.";
    std::move(callback_).Run(Result::kDestroyDiskImageFailed);
    return;
  }

  if (response->status() != vm_tools::concierge::DISK_STATUS_DESTROYED &&
      response->status() != vm_tools::concierge::DISK_STATUS_DOES_NOT_EXIST) {
    LOG(ERROR) << "Failed to destroy disk image: "
               << response->failure_reason();
    std::move(callback_).Run(Result::kDestroyDiskImageFailed);
    return;
  }

  // Remove mic pref (maybe others too?)
  switch (vm_type_) {
    case VmType::TERMINA:
      profile_->GetPrefs()->ClearPref(crostini::prefs::kCrostiniMicAllowed);
      break;
    case VmType::BRUSCHETTA:
      profile_->GetPrefs()->ClearPref(bruschetta::prefs::kBruschettaMicAllowed);
      break;
    default:
      break;
  }

  std::move(callback_).Run(Result::kSuccess);
  return;
}

}  // namespace guest_os
