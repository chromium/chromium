// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_remover.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace crostini {

CrostiniRemover::CrostiniRemover(
    Profile* profile,
    std::string vm_name,
    CrostiniManager::RemoveCrostiniCallback callback)
    : profile_(profile),
      vm_name_(std::move(vm_name)),
      callback_(std::move(callback)) {}

CrostiniRemover::~CrostiniRemover() = default;

void CrostiniRemover::RemoveCrostini() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CrostiniManager::GetForProfile(profile_)->StopVm(
      vm_name_, base::BindOnce(&CrostiniRemover::StopVmFinished, this));
}

void CrostiniRemover::StopVmFinished(CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to stop VM";
    std::move(callback_).Run(result);
    return;
  }

  VLOG(1) << "Clearing application list";
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
      ->ClearApplicationList(guest_os::VmType::TERMINA, vm_name_, "");
  guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile_)
      ->ClearMimeTypes(vm_name_, "");
  VLOG(1) << "Destroying disk image";
  CrostiniManager::GetForProfile(profile_)->DestroyDiskImage(
      vm_name_,
      base::BindOnce(&CrostiniRemover::DestroyDiskImageFinished, this));
}

void CrostiniRemover::DestroyDiskImageFinished(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    LOG(ERROR) << "Failed to destroy disk image";
    std::move(callback_).Run(CrostiniResult::DESTROY_DISK_IMAGE_FAILED);
    return;
  }

  VLOG(1) << "Uninstalling Termina";
  CrostiniManager::GetForProfile(profile_)->UninstallTermina(
      base::BindOnce(&CrostiniRemover::UninstallTerminaFinished, this));
}

void CrostiniRemover::UninstallTerminaFinished(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to uninstall Termina";
    std::move(callback_).Run(CrostiniResult::UNINSTALL_TERMINA_FAILED);
    return;
  }

  profile_->GetPrefs()->SetBoolean(prefs::kCrostiniEnabled, false);
  profile_->GetPrefs()->ClearPref(prefs::kCrostiniLastDiskSize);
  guest_os::RemoveVmFromPrefs(profile_, kCrostiniDefaultVmType);
  profile_->GetPrefs()->ClearPref(prefs::kCrostiniDefaultContainerConfigured);
  std::move(callback_).Run(CrostiniResult::SUCCESS);
}

}  // namespace crostini
