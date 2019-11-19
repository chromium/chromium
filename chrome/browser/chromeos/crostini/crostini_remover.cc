// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_remover.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
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
  if (CrostiniManager::GetForProfile(profile_)->IsCrosTerminaInstalled()) {
    CrostiniManager::GetForProfile(profile_)->InstallTerminaComponent(
        base::BindOnce(&CrostiniRemover::OnComponentLoaded, this));
  } else {
    // Crostini installation didn't install the component. Concierge should not
    // be running, nor should there be any VMs.
    CrostiniRemover::StopConciergeFinished(true);
  }
}

void CrostiniRemover::OnComponentLoaded(CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    std::move(callback_).Run(result);
    return;
  }
  CrostiniManager::GetForProfile(profile_)->StartConcierge(
      base::BindOnce(&CrostiniRemover::OnConciergeStarted, this));
}

void CrostiniRemover::OnConciergeStarted(bool is_successful) {
  if (!is_successful) {
    std::move(callback_).Run(CrostiniResult::UNKNOWN_ERROR);
    return;
  }
  CrostiniManager::GetForProfile(profile_)->StopVm(
      vm_name_, base::BindOnce(&CrostiniRemover::StopVmFinished, this));
}

void CrostiniRemover::StopVmFinished(CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != CrostiniResult::SUCCESS) {
    std::move(callback_).Run(result);
    return;
  }

  CrostiniRegistryServiceFactory::GetForProfile(profile_)->ClearApplicationList(
      vm_name_, "");
  CrostiniMimeTypesServiceFactory::GetForProfile(profile_)->ClearMimeTypes(
      vm_name_, "");
  CrostiniManager::GetForProfile(profile_)->DestroyDiskImage(
      base::FilePath(vm_name_),
      base::BindOnce(&CrostiniRemover::DestroyDiskImageFinished, this));
}

void CrostiniRemover::DestroyDiskImageFinished(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback_).Run(CrostiniResult::DESTROY_DISK_IMAGE_FAILED);
    return;
  }
  // Only set kCrostiniEnabled to false once cleanup is completely finished.
  CrostiniManager::GetForProfile(profile_)->StopConcierge(
      base::BindOnce(&CrostiniRemover::StopConciergeFinished, this));
}

void CrostiniRemover::StopConciergeFinished(bool is_successful) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The is_successful parameter is never set by debugd.

  // UninstallTerminaComponent returns false both if Termina wasn't installed
  // and if the uninstall failed, so we explicitly reset the relevant
  // preferences even if it's already uninstalled
  if (!CrostiniManager::GetForProfile(profile_)->IsCrosTerminaInstalled() ||
      CrostiniManager::GetForProfile(profile_)->UninstallTerminaComponent()) {
    profile_->GetPrefs()->SetBoolean(prefs::kCrostiniEnabled, false);
    profile_->GetPrefs()->ClearPref(prefs::kCrostiniLastDiskSize);
    profile_->GetPrefs()->Set(prefs::kCrostiniContainers,
                              base::Value(base::Value::Type::LIST));
  }
  std::move(callback_).Run(CrostiniResult::SUCCESS);
}

}  // namespace crostini
