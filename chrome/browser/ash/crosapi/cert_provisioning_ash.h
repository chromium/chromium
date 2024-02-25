// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CERT_PROVISIONING_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CERT_PROVISIONING_ASH_H_

#include "base/callback_list.h"
#include "chromeos/crosapi/mojom/cert_provisioning.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::cert_provisioning {
class CertProvisioningScheduler;
}

namespace crosapi {

class CertProvisioningAsh : public mojom::CertProvisioning {
 public:
  CertProvisioningAsh();
  CertProvisioningAsh(const CertProvisioningAsh&) = delete;
  CertProvisioningAsh& operator=(const CertProvisioningAsh&) = delete;
  ~CertProvisioningAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::CertProvisioning> receiver);

  // mojom::CertProvisioning
  void AddObserver(
      mojo::PendingRemote<mojom::CertProvisioningObserver> observer) override;
  void GetStatus(GetStatusCallback callback) override;
  void UpdateOneProcess(const std::string& cert_profile_id) override;
  void ResetOneProcess(const std::string& cert_profile_id) override;

  // Inject schedulers for testing. Passing nullptr simulates that a scheduler
  // is not found / available.
  void InjectForTesting(
      ash::cert_provisioning::CertProvisioningScheduler* user_scheduler,
      ash::cert_provisioning::CertProvisioningScheduler* device_scheduler);

 private:
  // CertProvisioningAsh can survive across sign-in/sign-out, different users
  // have access to different schedulers, so they should not be cached.
  ash::cert_provisioning::CertProvisioningScheduler* GetUserScheduler();
  ash::cert_provisioning::CertProvisioningScheduler* GetDeviceScheduler();

  void AppendWorkerStatuses(
      ash::cert_provisioning::CertProvisioningScheduler* scheduler,
      bool is_device_wide,
      std::vector<mojom::CertProvisioningProcessStatusPtr>& result);

  // Called when one of the schedulers notifies about new changes.
  void OnSchedulersChanged();

  // Called when one of the observers added by AddObserver has disconnected.
  void OnObserverDisconnected(mojo::RemoteSetElementId);

  // Start/stop observing cert provisioning schedulers. This class only observes
  // them when it itself has active observers. Lacros cannot survive across
  // sign-in/sign-out (unlike this class), so the observing of the user
  // scheduler will be reset automatically.
  void ObserveSchedulers();
  void StopObservingSchedulers();

  // The observers that will receive notifications about cert changes in Ash.
  mojo::RemoteSet<mojom::CertProvisioningObserver> observers_;

  std::optional<ash::cert_provisioning::CertProvisioningScheduler*>
      user_scheduler_for_testing_;
  std::optional<ash::cert_provisioning::CertProvisioningScheduler*>
      device_scheduler_for_testing_;

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::CertProvisioning> receivers_;

  // As long as these subscriptions are alive and initialized, this class can
  // receive notifications from the schedulers about changes. Should be
  // destroyed before any members that are needed to process notifications.
  base::CallbackListSubscription user_subscription_;
  base::CallbackListSubscription device_subscription_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CERT_PROVISIONING_ASH_H_
