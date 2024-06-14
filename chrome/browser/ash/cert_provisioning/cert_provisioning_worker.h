// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "components/policy/proto/device_management_backend.pb.h"

class Profile;
class PrefService;

namespace ash {
namespace cert_provisioning {

class CertProvisioningInvalidator;

// A OnceCallback that is invoked when the CertProvisioningWorker is done and
// has a result (which could be success or failure).
using CertProvisioningWorkerCallback =
    base::OnceCallback<void(CertProfile profile,
                            std::string process_id,
                            CertProvisioningWorkerState state)>;

class CertProvisioningWorker;

struct BackendServerError {
  // info on the last failed DMServer call attempt.
  BackendServerError(policy::DeviceManagementStatus dm_status,
                     base::Time error_time)
      : time(error_time), status(dm_status) {}

  base::Time time;
  policy::DeviceManagementStatus status;
};

class CertProvisioningWorkerFactory {
 public:
  virtual ~CertProvisioningWorkerFactory() = default;

  static CertProvisioningWorkerFactory* Get();

  virtual std::unique_ptr<CertProvisioningWorker> Create(
      std::string process_id,
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      const CertProfile& cert_profile,
      CertProvisioningClient* cert_provisioning_client,
      std::unique_ptr<CertProvisioningInvalidator> invalidator,
      base::RepeatingClosure state_change_callback,
      CertProvisioningWorkerCallback result_callback);

  virtual std::unique_ptr<CertProvisioningWorker> Deserialize(
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      const base::Value::Dict& saved_worker,
      CertProvisioningClient* cert_provisioning_client,
      std::unique_ptr<CertProvisioningInvalidator> invalidator,
      base::RepeatingClosure state_change_callback,
      CertProvisioningWorkerCallback result_callback);

  // Doesn't take ownership.
  static void SetFactoryForTesting(CertProvisioningWorkerFactory* test_factory);

 private:
  static CertProvisioningWorkerFactory* test_factory_;
};

// This class is a part of certificate provisioning feature.  It takes a
// certificate profile, generates a key pair, communicates with DM Server to
// create a CSR for it and request a certificate, and imports it.
class CertProvisioningWorker {
 public:
  CertProvisioningWorker() = default;
  CertProvisioningWorker(const CertProvisioningWorker&) = delete;
  CertProvisioningWorker& operator=(const CertProvisioningWorker&) = delete;
  virtual ~CertProvisioningWorker() = default;

  // Continue provisioning a certificate.
  virtual void DoStep() = 0;
  // Sets worker's state to one of final ones. That triggers corresponding
  // clean ups (deletes serialized state, keys, and so on) and returns |state|
  // via callback.
  virtual void Stop(CertProvisioningWorkerState state) = 0;
  // Make worker pause all activity and wait for DoStep.
  virtual void Pause() = 0;
  // Mark worker that it is undergoing a reset process.
  virtual void MarkWorkerForReset() = 0;
  // Returns true, if the worker is waiting for some future event. |DoStep| can
  // be called to try continue right now.
  virtual bool IsWaiting() const = 0;
  // Returns true if the worker is to be recreated due to a user-initiated
  // "reset" action.
  virtual bool IsWorkerMarkedForReset() const = 0;
  // Returns CertProfile that this worker is working on.
  virtual const CertProfile& GetCertProfile() const = 0;
  // Returns public key or an empty string if the key is not created yet.
  virtual const std::vector<uint8_t>& GetPublicKey() const = 0;
  // Returns current state.
  virtual CertProvisioningWorkerState GetState() const = 0;
  // Returns state that was before the current one. Especially helpful on failed
  // workers.
  virtual CertProvisioningWorkerState GetPreviousState() const = 0;
  // Returns the time when this worker has been last updated.
  virtual base::Time GetLastUpdateTime() const = 0;
  // Return the info of when this worker has last faced an unsuccessful attempt.
  virtual const std::optional<BackendServerError>& GetLastBackendServerError()
      const = 0;
  // Return a message describing the reason for failure when the worker fails.
  // In case the worker did not fail, the message is empty.
  virtual std::string GetFailureMessage() const = 0;
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_H_
