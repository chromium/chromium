// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/cert_provisioning_ash.h"

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler_user_service.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

using ash::cert_provisioning::CertProvisioningScheduler;
using ash::cert_provisioning::CertProvisioningWorkerState;

namespace crosapi {

CertProvisioningAsh::CertProvisioningAsh() {
  // Unretained(this) is safe because `observers_` is owned by `this` and will
  // never outlive it.
  observers_.set_disconnect_handler(base::BindRepeating(
      &CertProvisioningAsh::OnObserverDisconnected, base::Unretained(this)));
}

CertProvisioningAsh::~CertProvisioningAsh() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CertProvisioningAsh::BindReceiver(
    mojo::PendingReceiver<mojom::CertProvisioning> pending_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  receivers_.Add(this, std::move(pending_receiver));
}

void CertProvisioningAsh::AddObserver(
    mojo::PendingRemote<mojom::CertProvisioningObserver> observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (observers_.empty()) {
    ObserveSchedulers();
  }
  observers_.Add(
      mojo::Remote<mojom::CertProvisioningObserver>(std::move(observer)));
}

void CertProvisioningAsh::OnObserverDisconnected(mojo::RemoteSetElementId) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (observers_.empty()) {
    StopObservingSchedulers();
  }
}

void CertProvisioningAsh::ObserveSchedulers() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Unretained(this) is safe because the subscriptions never outlive `this`.
  if (CertProvisioningScheduler* user_scheduler = GetUserScheduler()) {
    user_subscription_ = user_scheduler->AddObserver(base::BindRepeating(
        &CertProvisioningAsh::OnSchedulersChanged, base::Unretained(this)));
  }
  if (CertProvisioningScheduler* device_scheduler = GetDeviceScheduler()) {
    device_subscription_ = device_scheduler->AddObserver(base::BindRepeating(
        &CertProvisioningAsh::OnSchedulersChanged, base::Unretained(this)));
  }
}

void CertProvisioningAsh::StopObservingSchedulers() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Destroy the subscriptions so the schedulers stop sending the notifications.
  user_subscription_ = {};
  device_subscription_ = {};
}

void CertProvisioningAsh::OnSchedulersChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& observer : observers_) {
    observer->OnStateChanged();
  }
}

void CertProvisioningAsh::UpdateOneProcess(const std::string& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CertProvisioningScheduler* user_scheduler = GetUserScheduler();
  if (user_scheduler && user_scheduler->UpdateOneWorker(cert_profile_id)) {
    return;
  }

  CertProvisioningScheduler* device_scheduler = GetDeviceScheduler();
  if (device_scheduler) {
    device_scheduler->UpdateOneWorker(cert_profile_id);
  }
}

void CertProvisioningAsh::ResetOneProcess(const std::string& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CertProvisioningScheduler* user_scheduler = GetUserScheduler();
  if (user_scheduler && user_scheduler->ResetOneWorker(cert_profile_id)) {
    return;
  }

  CertProvisioningScheduler* device_scheduler = GetDeviceScheduler();
  if (device_scheduler && device_scheduler->ResetOneWorker(cert_profile_id)) {
    return;
  }

  if (user_scheduler || device_scheduler) {
    LOG(ERROR) << "resetting cert_profile_id was not found. id:"
               << cert_profile_id << " user_scheduler:" << bool(user_scheduler)
               << " device_scheduler:" << bool(device_scheduler);
    return;
  }
}

void CertProvisioningAsh::InjectForTesting(
    ash::cert_provisioning::CertProvisioningScheduler* user_scheduler,
    ash::cert_provisioning::CertProvisioningScheduler* device_scheduler) {
  user_scheduler_for_testing_ = user_scheduler;
  device_scheduler_for_testing_ = device_scheduler;
}

CertProvisioningScheduler* CertProvisioningAsh::GetUserScheduler() {
  if (user_scheduler_for_testing_.has_value()) {
    return user_scheduler_for_testing_.value();
  }

  Profile* user_profile = ProfileManager::GetPrimaryUserProfile();
  if (!user_profile) {
    return nullptr;
  }

  ash::cert_provisioning::CertProvisioningSchedulerUserService* user_service =
      ash::cert_provisioning::CertProvisioningSchedulerUserServiceFactory::
          GetForProfile(user_profile);
  if (!user_service) {
    return nullptr;
  }

  return user_service->scheduler();
}

CertProvisioningScheduler* CertProvisioningAsh::GetDeviceScheduler() {
  if (device_scheduler_for_testing_.has_value()) {
    return device_scheduler_for_testing_.value();
  }

  Profile* user_profile = ProfileManager::GetPrimaryUserProfile();
  if (!user_profile) {
    return nullptr;
  }

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(user_profile);
  if (!user || !user->IsAffiliated()) {
    return nullptr;
  }

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetDeviceCertProvisioningScheduler();
}

}  // namespace crosapi
