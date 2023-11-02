// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "net/base/backoff_entry.h"

namespace policy {
class CloudPolicyClient;
}  // namespace policy
class Profile;
class PrefService;

namespace ash {
namespace cert_provisioning {

class CertProvisioningInvalidator;

// A OnceCallback that is invoked when the CertProvisioningWorker is done and
// has a result (which could be success or failure).
using CertProvisioningWorkerCallback =
    base::OnceCallback<void(const CertProfile& profile,
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
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      const CertProfile& cert_profile,
      policy::CloudPolicyClient* cloud_policy_client,
      std::unique_ptr<CertProvisioningInvalidator> invalidator,
      base::RepeatingClosure state_change_callback,
      CertProvisioningWorkerCallback result_callback);

  virtual std::unique_ptr<CertProvisioningWorker> Deserialize(
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      const base::Value& saved_worker,
      policy::CloudPolicyClient* cloud_policy_client,
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
  // Returns true, if the worker is waiting for some future event. |DoStep| can
  // be called to try continue right now.
  virtual bool IsWaiting() const = 0;
  // Returns CertProfile that this worker is working on.
  virtual const CertProfile& GetCertProfile() const = 0;
  // Returns public key or an empty string if the key is not created yet.
  virtual const std::string& GetPublicKey() const = 0;
  // Returns current state.
  virtual CertProvisioningWorkerState GetState() const = 0;
  // Returns state that was before the current one. Especially helpful on failed
  // workers.
  virtual CertProvisioningWorkerState GetPreviousState() const = 0;
  // Returns the time when this worker has been last updated.
  virtual base::Time GetLastUpdateTime() const = 0;
  // Return the info of when this worker has last faced an unsuccessful attempt.
  virtual const absl::optional<BackendServerError>& GetLastBackendServerError()
      const = 0;
  // Return a message describing the reason for failure when the worker fails.
  // In case the worker did not fail, the message is empty.
  virtual const std::string& GetFailureMessage() const = 0;
};

class CertProvisioningWorkerImpl : public CertProvisioningWorker {
 public:
  CertProvisioningWorkerImpl(
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      const CertProfile& cert_profile,
      policy::CloudPolicyClient* cloud_policy_client,
      std::unique_ptr<CertProvisioningInvalidator> invalidator,
      base::RepeatingClosure state_change_callback,
      CertProvisioningWorkerCallback result_callback);
  ~CertProvisioningWorkerImpl() override;

  // CertProvisioningWorker
  void DoStep() override;
  void Stop(CertProvisioningWorkerState state) override;
  void Pause() override;
  bool IsWaiting() const override;
  const CertProfile& GetCertProfile() const override;
  const std::string& GetPublicKey() const override;
  CertProvisioningWorkerState GetState() const override;
  CertProvisioningWorkerState GetPreviousState() const override;
  base::Time GetLastUpdateTime() const override;
  const absl::optional<BackendServerError>& GetLastBackendServerError()
      const override;
  const std::string& GetFailureMessage() const override;

 private:
  friend class CertProvisioningSerializer;

  void GenerateKey();

  void GenerateRegularKey();
  void OnGenerateRegularKeyDone(const std::string& public_key_spki_der,
                                chromeos::platform_keys::Status status);

  void GenerateKeyForVa();
  void OnGenerateKeyForVaDone(base::TimeTicks start_time,
                              const attestation::TpmChallengeKeyResult& result);

  void StartCsr();
  void OnStartCsrDone(policy::DeviceManagementStatus status,
                      absl::optional<CertProvisioningResponseErrorType> error,
                      absl::optional<int64_t> try_later,
                      const std::string& invalidation_topic,
                      const std::string& va_challenge,
                      enterprise_management::HashingAlgorithm hashing_algorithm,
                      const std::string& data_to_sign);

  void ProcessStartCsrResponse();

  void BuildVaChallengeResponse();
  void OnBuildVaChallengeResponseDone(
      base::TimeTicks start_time,
      const attestation::TpmChallengeKeyResult& result);

  void RegisterKey();
  void OnRegisterKeyDone(const attestation::TpmChallengeKeyResult& result);

  void MarkKey();
  void OnMarkKeyDone(chromeos::platform_keys::Status status);

  void SignCsr();
  void OnSignCsrDone(base::TimeTicks start_time,
                     const std::string& signature,
                     chromeos::platform_keys::Status status);

  void FinishCsr();
  void OnFinishCsrDone(policy::DeviceManagementStatus status,
                       absl::optional<CertProvisioningResponseErrorType> error,
                       absl::optional<int64_t> try_later);

  void DownloadCert();
  void OnDownloadCertDone(
      policy::DeviceManagementStatus status,
      absl::optional<CertProvisioningResponseErrorType> error,
      absl::optional<int64_t> try_later,
      const std::string& pem_encoded_certificate);

  void ImportCert(const std::string& pem_encoded_certificate);
  void OnImportCertDone(chromeos::platform_keys::Status status);

  void ScheduleNextStep(base::TimeDelta delay);
  void CancelScheduledTasks();

  enum class ContinueReason { kTimeout, kInvalidation };
  void OnShouldContinue(ContinueReason reason);

  // Registers for |invalidation_topic_| that allows to receive notification
  // when server side is ready to continue provisioning process.
  void RegisterForInvalidationTopic();
  // Should be called only when provisioning process is finished (successfully
  // or not). Should not be called when the worker is destroyed, but will be
  // deserialized back later.
  void UnregisterFromInvalidationTopic();

  // If it is called with kSucceed or kFailed, it will call the |callback_|. The
  // worker can be destroyed in callback and should not use any member fields
  // after that.
  void UpdateState(const base::Location& from_here,
                   CertProvisioningWorkerState state);

  // Serializes the worker or deletes serialized state accroding to the current
  // state. Some states are considered unrecoverable, some can be reached again
  // from previous ones.
  void HandleSerialization();
  // Handles recreation of some internal objects after deserialization. Intended
  // to be called from CertProvisioningDeserializer.
  void InitAfterDeserialization();

  void CleanUpAndRunCallback();
  void OnDeleteVaKeyDone(bool delete_result);
  void OnRemoveKeyDone(chromeos::platform_keys::Status status);
  void OnCleanUpDone();

  // Returns true if there are no errors and the flow can be continued.
  // |request_type| is the type of the request to which the DM server has
  // responded with the given |status|.
  bool ProcessResponseErrors(
      DeviceManagementServerRequestType request_type,
      policy::DeviceManagementStatus status,
      absl::optional<CertProvisioningResponseErrorType> error,
      absl::optional<int64_t> try_later);

  CertScope cert_scope_ = CertScope::kUser;
  Profile* profile_ = nullptr;
  PrefService* pref_service_ = nullptr;
  CertProfile cert_profile_;
  base::RepeatingClosure state_change_callback_;
  CertProvisioningWorkerCallback result_callback_;

  // This field should be updated only via |UpdateState| function. It will
  // trigger update of the serialized data.
  CertProvisioningWorkerState state_ = CertProvisioningWorkerState::kInitState;
  // State that was before the current one. Useful for debugging and cleaning
  // on failure.
  CertProvisioningWorkerState prev_state_ = state_;
  // Time when this worker has been last updated. An update is when the worker
  // advances to the next state or for states that wait for a backend-side
  // condition (e.g CertProvisioningWorkerState:kFinishCsrResponseReceived):
  // when it successfully checked with the backend that the condition is not
  // fulfilled yet.
  base::Time last_update_time_;
  // Consequently, it is not updated if waiting for a backend-side condition,
  // but communication with the backend is not possible (e.g. due to server
  // errors or network connectivity issues).
  // The last error received in communicating to the backend server.
  absl::optional<BackendServerError> last_backend_server_error_;
  bool is_waiting_ = false;
  // Used for an UMA metric to track situation when the worker did not receive
  // an invalidation for a completed server side task.
  bool is_continued_without_invalidation_for_uma_ = false;
  // Calculates retry timeout for network related failures.
  net::BackoffEntry request_backoff_;

  // Public key - represented as DER-encoded X.509 SubjectPublicKeyInfo
  // (binary).
  std::string public_key_;
  std::string invalidation_topic_;

  // These variables may not contain valid values after
  // kFinishCsrResponseReceived state because of deserialization (and they don't
  // need to).
  std::string csr_;
  std::string va_challenge_;
  std::string va_challenge_response_;
  absl::optional<chromeos::platform_keys::HashAlgorithm> hashing_algorithm_;
  std::string signature_;

  // Holds a message describing the reason for failure when the worker fails.
  // If the worker did not fail, this message is empty.
  std::string failure_message_;

  // IMPORTANT:
  // Increment this when you add/change any member in CertProvisioningWorkerImpl
  // that affects serialization (and update all functions that fail to compile
  // because of it).
  static constexpr int kVersion = 1;

  // Unowned PlatformKeysService. Note that the CertProvisioningWorker does not
  // observe the PlatformKeysService for shutdown events. Instead, it relies on
  // the CertProvisioningScheduler to destroy all CertProvisioningWorker
  // instances when the corresponding PlatformKeysService is shutting down.
  platform_keys::PlatformKeysService* platform_keys_service_ = nullptr;
  std::unique_ptr<attestation::TpmChallengeKeySubtle>
      tpm_challenge_key_subtle_impl_;
  policy::CloudPolicyClient* cloud_policy_client_ = nullptr;

  std::unique_ptr<CertProvisioningInvalidator> invalidator_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CertProvisioningWorkerImpl> weak_factory_{this};
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_H_
