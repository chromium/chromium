// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker_static.h"

#include <stdint.h>

#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_metrics.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_serializer.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "content/public/browser/browser_context.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace em = enterprise_management;

namespace ash::cert_provisioning {

namespace {

constexpr unsigned int kNonVaKeyModulusLengthBits = 2048;

constexpr base::TimeDelta kMinumumTryAgainLaterDelay = base::Seconds(10);

// The delay after which a StartCsr request can be resent after a 412 Pending
// Approval has been returned by the DM server.
constexpr base::TimeDelta kRetryStartCsrRequestDelay = base::Hours(1);
// The delay after which a FinishCsr request can be resent after a 412 Pending
// Approval has been returned by the DM server.
constexpr base::TimeDelta kRetryFinishCsrRequestDelay = base::Hours(1);
// The delay after which a DownloadCsr request can be resent after a 412 Pending
// Approval has been returned by the DM server.
// Note: This request retry delay is more than other delays as a DownloadCsr
// request may not only fail because of a DM server or a CES server problem but
// also because of a problem with the Google Certificate Connecter which may
// take more time to solve.
constexpr base::TimeDelta kRetryDownloadCsrRequestDelay = base::Hours(8);

const net::BackoffEntry::Policy kBackoffPolicy{
    /*num_errors_to_ignore=*/0,
    /*initial_delay_ms=*/
    base::checked_cast<int>(base::Seconds(30).InMilliseconds()),
    /*multiply_factor=*/2.0,
    /*jitter_factor=*/0.15,
    /*maximum_backoff_ms=*/base::Hours(12).InMilliseconds(),
    /*entry_lifetime_ms=*/-1,
    /*always_use_initial_delay=*/false};

bool ConvertHashingAlgorithm(
    em::HashingAlgorithm input_algo,
    absl::optional<chromeos::platform_keys::HashAlgorithm>* output_algo) {
  switch (input_algo) {
    case em::HashingAlgorithm::SHA1:
      *output_algo =
          chromeos::platform_keys::HashAlgorithm::HASH_ALGORITHM_SHA1;
      return true;
    case em::HashingAlgorithm::SHA256:
      *output_algo =
          chromeos::platform_keys::HashAlgorithm::HASH_ALGORITHM_SHA256;
      return true;
    case em::HashingAlgorithm::NO_HASH:
      *output_algo =
          chromeos::platform_keys::HashAlgorithm::HASH_ALGORITHM_NONE;
      return true;
    case em::HashingAlgorithm::HASHING_ALGORITHM_UNSPECIFIED:
      return false;
  }
}

// States are used in serialization and cannot be reordered. Therefore, their
// order should not be defined by their underlying values.
int GetStateOrderedIndex(CertProvisioningWorkerState state) {
  int res = 0;
  switch (state) {
    case CertProvisioningWorkerState::kInitState:
      res -= 1;
      [[fallthrough]];
    case CertProvisioningWorkerState::kKeypairGenerated:
      res -= 1;
      [[fallthrough]];
    case CertProvisioningWorkerState::kStartCsrResponseReceived:
      res -= 1;
      [[fallthrough]];
    case CertProvisioningWorkerState::kVaChallengeFinished:
      res -= 1;
      [[fallthrough]];
    case CertProvisioningWorkerState::kKeyRegistered:
      res -= 1;
      [[fallthrough]];
    case CertProvisioningWorkerState::kKeypairMarked:
      res -= 1;
      [[fallthrough]];
    case CertProvisioningWorkerState::kSignCsrFinished:
      res -= 1;
      [[fallthrough]];
    case CertProvisioningWorkerState::kFinishCsrResponseReceived:
      res -= 1;
      [[fallthrough]];
    case CertProvisioningWorkerState::kSucceeded:
    case CertProvisioningWorkerState::kInconsistentDataError:
    case CertProvisioningWorkerState::kFailed:
    case CertProvisioningWorkerState::kCanceled:
      res -= 1;
      break;
    case CertProvisioningWorkerState::kReadyForNextOperation:
    case CertProvisioningWorkerState::kAuthorizeInstructionReceived:
    case CertProvisioningWorkerState::kProofOfPossessionInstructionReceived:
    case CertProvisioningWorkerState::kImportCertificateInstructionReceived:
      // These states are not used in the "static" flow.
      CHECK(false);
  }
  return res;
}

void OnAllowKeyForUsageDone(chromeos::platform_keys::Status status) {
  if (status != chromeos::platform_keys::Status::kSuccess) {
    LOG(ERROR) << "Cannot mark key corporate: "
               << chromeos::platform_keys::StatusToString(status);
  }
}
// Marks the key |public_key_spki_der| as corporate. |profile| can be nullptr if
// |scope| is CertScope::kDevice.
void MarkKeyAsCorporate(CertScope scope,
                        Profile* profile,
                        const std::vector<uint8_t>& public_key_spki_der) {
  CHECK(profile || scope == CertScope::kDevice);

  GetKeyPermissionsManager(scope, profile)
      ->AllowKeyForUsage(base::BindOnce(&OnAllowKeyForUsageDone),
                         platform_keys::KeyUsage::kCorporate,
                         public_key_spki_der);
}

base::TimeDelta GetTryLaterDelayForRequestType(
    DeviceManagementServerRequestType request_type) {
  switch (request_type) {
    case DeviceManagementServerRequestType::kStartCsr:
      return kRetryStartCsrRequestDelay;
    case DeviceManagementServerRequestType::kFinishCsr:
      return kRetryFinishCsrRequestDelay;
    case DeviceManagementServerRequestType::kDownloadCert:
      return kRetryDownloadCsrRequestDelay;
  }
}

// The original message of kUserNotManagedError is misleading in case the user
// is not affiliated. In this case, the error message associated to the error
// code kUserNotManagedError is replaced.
std::string ConstructFailureMessage(
    const attestation::TpmChallengeKeyResult& challenge_result) {
  std::string failure_message = "Failed to build challenge response: ";
  if (challenge_result.result_code ==
      attestation::TpmChallengeKeyResultCode::kUserNotManagedError) {
    return (failure_message +
            "User is not affiliated. Certificate profile is not applicable.");
  }
  return (failure_message + challenge_result.GetErrorMessage());
}

// TODO(b/192071491): Remove the use of this function by changing the
// dependencies.
std::vector<uint8_t> StrToBytes(base::StringPiece str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

// TODO(b/192071491): Remove the use of this function by changing the
// dependencies.
std::string BytesToStr(const std::vector<uint8_t>& blob) {
  return std::string(blob.begin(), blob.end());
}

}  // namespace

// ================== CertProvisioningWorkerStatic =============================

CertProvisioningWorkerStatic::CertProvisioningWorkerStatic(
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    const CertProfile& cert_profile,
    CertProvisioningClient* cert_provisioning_client,
    std::unique_ptr<CertProvisioningInvalidator> invalidator,
    base::RepeatingClosure state_change_callback,
    CertProvisioningWorkerCallback result_callback)
    : cert_scope_(cert_scope),
      profile_(profile),
      pref_service_(pref_service),
      cert_profile_(cert_profile),
      state_change_callback_(std::move(state_change_callback)),
      result_callback_(std::move(result_callback)),
      request_backoff_(&kBackoffPolicy),
      cert_provisioning_client_(cert_provisioning_client),
      invalidator_(std::move(invalidator)) {
  CHECK(profile || cert_scope == CertScope::kDevice);
  platform_keys_service_ = GetPlatformKeysService(cert_scope, profile);
  CHECK(platform_keys_service_);

  CHECK(pref_service);
  CHECK(cert_provisioning_client_);
  CHECK(invalidator_);
}

CertProvisioningWorkerStatic::~CertProvisioningWorkerStatic() = default;

bool CertProvisioningWorkerStatic::IsWaiting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return is_waiting_;
}

const CertProfile& CertProvisioningWorkerStatic::GetCertProfile() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return cert_profile_;
}

const std::vector<uint8_t>& CertProvisioningWorkerStatic::GetPublicKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return public_key_;
}

CertProvisioningWorkerState CertProvisioningWorkerStatic::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return state_;
}

CertProvisioningWorkerState CertProvisioningWorkerStatic::GetPreviousState()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return prev_state_;
}

base::Time CertProvisioningWorkerStatic::GetLastUpdateTime() const {
  return last_update_time_;
}

const absl::optional<BackendServerError>&
CertProvisioningWorkerStatic::GetLastBackendServerError() const {
  return last_backend_server_error_;
}

const std::string& CertProvisioningWorkerStatic::GetFailureMessage() const {
  return failure_message_;
}

void CertProvisioningWorkerStatic::Stop(CertProvisioningWorkerState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(IsFinalState(state));

  CancelScheduledTasks();
  UpdateState(FROM_HERE, state);
}

void CertProvisioningWorkerStatic::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CancelScheduledTasks();
  is_waiting_ = true;
}

void CertProvisioningWorkerStatic::DoStep() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CancelScheduledTasks();
  is_waiting_ = false;
  switch (state_) {
    case CertProvisioningWorkerState::kInitState:
      GenerateKey();
      return;
    case CertProvisioningWorkerState::kKeypairGenerated:
      StartCsr();
      return;
    case CertProvisioningWorkerState::kStartCsrResponseReceived:
      ProcessStartCsrResponse();
      return;
    case CertProvisioningWorkerState::kVaChallengeFinished:
      RegisterKey();
      return;
    case CertProvisioningWorkerState::kKeyRegistered:
      MarkKey();
      return;
    case CertProvisioningWorkerState::kKeypairMarked:
      SignCsr();
      return;
    case CertProvisioningWorkerState::kSignCsrFinished:
      FinishCsr();
      return;
    case CertProvisioningWorkerState::kFinishCsrResponseReceived:
      DownloadCert();
      return;
    case CertProvisioningWorkerState::kSucceeded:
    case CertProvisioningWorkerState::kInconsistentDataError:
    case CertProvisioningWorkerState::kFailed:
    case CertProvisioningWorkerState::kCanceled:
      DCHECK(false);
      return;
    case CertProvisioningWorkerState::kReadyForNextOperation:
    case CertProvisioningWorkerState::kAuthorizeInstructionReceived:
    case CertProvisioningWorkerState::kProofOfPossessionInstructionReceived:
    case CertProvisioningWorkerState::kImportCertificateInstructionReceived:
      // These states are not used in the "static" flow.
      CHECK(false);
      return;
  }
  NOTREACHED() << " " << static_cast<uint>(state_);
}

void CertProvisioningWorkerStatic::UpdateState(
    const base::Location& from_here,
    CertProvisioningWorkerState new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(kStaticWorkerStates.Has(new_state)) << static_cast<int>(new_state);
  DCHECK(GetStateOrderedIndex(state_) < GetStateOrderedIndex(new_state));

  prev_state_ = state_;
  state_ = new_state;
  last_update_time_ = base::Time::NowFromSystemTime();

  if (is_continued_without_invalidation_for_uma_) {
    RecordEvent(
        cert_profile_.protocol_version, cert_scope_,
        CertProvisioningEvent::kWorkerRetrySucceededWithoutInvalidation);
    is_continued_without_invalidation_for_uma_ = false;
  }

  HandleSerialization();

  if (state_ == CertProvisioningWorkerState::kFailed) {
    LOG(ERROR) << "Failure state from " << from_here.ToString()
               << ". Details: " << failure_message_;
  }

  state_change_callback_.Run();
  if (IsFinalState(state_)) {
    CleanUpAndRunCallback();
  }
}

void CertProvisioningWorkerStatic::GenerateKey() {
  if (cert_profile_.is_va_enabled) {
    GenerateKeyForVa();
  } else {
    GenerateRegularKey();
  }
}

void CertProvisioningWorkerStatic::GenerateRegularKey() {
  platform_keys_service_->GenerateRSAKey(
      GetPlatformKeysTokenId(cert_scope_), kNonVaKeyModulusLengthBits,
      /*sw_backed=*/false,
      base::BindOnce(&CertProvisioningWorkerStatic::OnGenerateRegularKeyDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerStatic::OnGenerateRegularKeyDone(
    std::vector<uint8_t> public_key_spki_der,
    chromeos::platform_keys::Status status) {
  if (status != chromeos::platform_keys::Status::kSuccess ||
      public_key_spki_der.empty()) {
    failure_message_ =
        base::StrCat({"Failed to prepare a non-VA key: ",
                      chromeos::platform_keys::StatusToString(status)});
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  public_key_ = std::move(public_key_spki_der);
  UpdateState(FROM_HERE, CertProvisioningWorkerState::kKeypairGenerated);
  DoStep();
}

void CertProvisioningWorkerStatic::GenerateKeyForVa() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tpm_challenge_key_subtle_impl_ =
      attestation::TpmChallengeKeySubtleFactory::Create();
  tpm_challenge_key_subtle_impl_->StartPrepareKeyStep(
      GetVaKeyType(cert_scope_),
      /*will_register_key=*/true, ::attestation::KEY_TYPE_RSA,
      GetKeyName(cert_profile_.profile_id), profile_,
      base::BindOnce(&CertProvisioningWorkerStatic::OnGenerateKeyForVaDone,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      /*signals=*/absl::nullopt);
}

void CertProvisioningWorkerStatic::OnGenerateKeyForVaDone(
    base::TimeTicks start_time,
    const attestation::TpmChallengeKeyResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordKeypairGenerationTime(cert_profile_.protocol_version, cert_scope_,
                              base::TimeTicks::Now() - start_time);

  if (result.result_code ==
      attestation::TpmChallengeKeyResultCode::kGetCertificateFailedError) {
    LOG(WARNING) << "Failed to get certificate for a key";
    request_backoff_.InformOfRequest(false);
    // Next DoStep will retry generating the key.
    ScheduleNextStep(request_backoff_.GetTimeUntilRelease());
    return;
  }

  if (!result.IsSuccess() || result.public_key.empty()) {
    failure_message_ =
        std::string("Failed to prepare a key: ") + result.GetErrorMessage();
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  public_key_ = StrToBytes(result.public_key);
  UpdateState(FROM_HERE, CertProvisioningWorkerState::kKeypairGenerated);
  DoStep();
}

void CertProvisioningWorkerStatic::StartCsr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cert_provisioning_client_->StartCsr(
      GetProvisioningProcessForClient(),
      base::BindOnce(&CertProvisioningWorkerStatic::OnStartCsrDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerStatic::OnStartCsrDone(
    policy::DeviceManagementStatus status,
    absl::optional<CertProvisioningResponseErrorType> error,
    absl::optional<int64_t> try_later,
    const std::string& invalidation_topic,
    const std::string& va_challenge,
    enterprise_management::HashingAlgorithm hashing_algorithm,
    std::vector<uint8_t> data_to_sign) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ProcessResponseErrors(DeviceManagementServerRequestType::kStartCsr,
                             status, error, try_later)) {
    return;
  }

  if (!ConvertHashingAlgorithm(hashing_algorithm, &hashing_algorithm_)) {
    failure_message_ = "Failed to parse hashing algorithm";
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  if (cert_profile_.is_va_enabled && va_challenge.empty()) {
    failure_message_ = "VA challenge is required, but not included";
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  csr_ = BytesToStr(data_to_sign);
  invalidation_topic_ = invalidation_topic;
  va_challenge_ = va_challenge;
  UpdateState(FROM_HERE,
              CertProvisioningWorkerState::kStartCsrResponseReceived);

  RegisterForInvalidationTopic();

  DoStep();
}

void CertProvisioningWorkerStatic::ProcessStartCsrResponse() {
  if (!cert_profile_.is_va_enabled) {
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kKeyRegistered);
    DoStep();
    return;
  }

  BuildVaChallengeResponse();
}

void CertProvisioningWorkerStatic::BuildVaChallengeResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tpm_challenge_key_subtle_impl_->StartSignChallengeStep(
      va_challenge_,
      base::BindOnce(
          &CertProvisioningWorkerStatic::OnBuildVaChallengeResponseDone,
          weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void CertProvisioningWorkerStatic::OnBuildVaChallengeResponseDone(
    base::TimeTicks start_time,
    const attestation::TpmChallengeKeyResult& challenge_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordVerifiedAccessTime(cert_profile_.protocol_version, cert_scope_,
                           base::TimeTicks::Now() - start_time);

  if (!challenge_result.IsSuccess()) {
    failure_message_ = ConstructFailureMessage(challenge_result);
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  if (challenge_result.challenge_response.empty()) {
    failure_message_ = "Challenge response is empty";
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  va_challenge_response_ = challenge_result.challenge_response;
  UpdateState(FROM_HERE, CertProvisioningWorkerState::kVaChallengeFinished);
  DoStep();
}

void CertProvisioningWorkerStatic::RegisterKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tpm_challenge_key_subtle_impl_->StartRegisterKeyStep(
      base::BindOnce(&CertProvisioningWorkerStatic::OnRegisterKeyDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerStatic::OnRegisterKeyDone(
    const attestation::TpmChallengeKeyResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tpm_challenge_key_subtle_impl_.reset();

  if (!result.IsSuccess()) {
    failure_message_ =
        base::StrCat({"Failed to register key: ", result.GetErrorMessage()});
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  UpdateState(FROM_HERE, CertProvisioningWorkerState::kKeyRegistered);
  DoStep();
}

void CertProvisioningWorkerStatic::MarkKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MarkKeyAsCorporate(cert_scope_, profile_, public_key_);

  platform_keys_service_->SetAttributeForKey(
      GetPlatformKeysTokenId(cert_scope_), BytesToStr(public_key_),
      chromeos::platform_keys::KeyAttributeType::kCertificateProvisioningId,
      StrToBytes(cert_profile_.profile_id),
      base::BindOnce(&CertProvisioningWorkerStatic::OnMarkKeyDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerStatic::OnMarkKeyDone(
    chromeos::platform_keys::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    failure_message_ =
        base::StrCat({"Failed to mark a key: ",
                      chromeos::platform_keys::StatusToString(status)});
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  UpdateState(FROM_HERE, CertProvisioningWorkerState::kKeypairMarked);
  DoStep();
}

void CertProvisioningWorkerStatic::SignCsr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!hashing_algorithm_.has_value()) {
    failure_message_ = "Hashing algorithm is empty";
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  if (hashing_algorithm_ ==
      chromeos::platform_keys::HashAlgorithm::HASH_ALGORITHM_NONE) {
    platform_keys_service_->SignRSAPKCS1Raw(
        GetPlatformKeysTokenId(cert_scope_), StrToBytes(csr_), public_key_,
        base::BindRepeating(&CertProvisioningWorkerStatic::OnSignCsrDone,
                            weak_factory_.GetWeakPtr(),
                            base::TimeTicks::Now()));
    return;
  }
  platform_keys_service_->SignRSAPKCS1Digest(
      GetPlatformKeysTokenId(cert_scope_), StrToBytes(csr_), public_key_,
      hashing_algorithm_.value(),
      base::BindRepeating(&CertProvisioningWorkerStatic::OnSignCsrDone,
                          weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void CertProvisioningWorkerStatic::OnSignCsrDone(
    base::TimeTicks start_time,
    std::vector<uint8_t> signature,
    chromeos::platform_keys::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordDataSignTime(cert_profile_.protocol_version, cert_scope_,
                     base::TimeTicks::Now() - start_time);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    failure_message_ =
        base::StrCat({"Failed to sign CSR: ",
                      chromeos::platform_keys::StatusToString(status)});
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  signature_ = BytesToStr(signature);
  UpdateState(FROM_HERE, CertProvisioningWorkerState::kSignCsrFinished);
  DoStep();
}

void CertProvisioningWorkerStatic::FinishCsr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cert_provisioning_client_->FinishCsr(
      GetProvisioningProcessForClient(), va_challenge_response_, signature_,
      base::BindOnce(&CertProvisioningWorkerStatic::OnFinishCsrDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerStatic::OnFinishCsrDone(
    policy::DeviceManagementStatus status,
    absl::optional<CertProvisioningResponseErrorType> error,
    absl::optional<int64_t> try_later) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ProcessResponseErrors(DeviceManagementServerRequestType::kFinishCsr,
                             status, error, try_later)) {
    return;
  }

  UpdateState(FROM_HERE,
              CertProvisioningWorkerState::kFinishCsrResponseReceived);
  DoStep();
}

void CertProvisioningWorkerStatic::DownloadCert() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cert_provisioning_client_->DownloadCert(
      GetProvisioningProcessForClient(),
      base::BindOnce(&CertProvisioningWorkerStatic::OnDownloadCertDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerStatic::OnDownloadCertDone(
    policy::DeviceManagementStatus status,
    absl::optional<CertProvisioningResponseErrorType> error,
    absl::optional<int64_t> try_later,
    const std::string& pem_encoded_certificate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ProcessResponseErrors(DeviceManagementServerRequestType::kDownloadCert,
                             status, error, try_later)) {
    return;
  }

  ImportCert(pem_encoded_certificate);
}

void CertProvisioningWorkerStatic::ImportCert(
    const std::string& pem_encoded_certificate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<net::X509Certificate> cert = CreateSingleCertificateFromBytes(
      pem_encoded_certificate.data(), pem_encoded_certificate.size());
  if (!cert) {
    failure_message_ = "Failed to parse a certificate";
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  std::vector<uint8_t> public_key_from_cert =
      chromeos::platform_keys::GetSubjectPublicKeyInfoBlob(cert);
  if (public_key_from_cert != public_key_) {
    failure_message_ =
        "Downloaded certificate does not match the expected key pair";
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  platform_keys_service_->ImportCertificate(
      GetPlatformKeysTokenId(cert_scope_), cert,
      base::BindRepeating(&CertProvisioningWorkerStatic::OnImportCertDone,
                          weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerStatic::OnImportCertDone(
    chromeos::platform_keys::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    failure_message_ =
        base::StrCat({"Failed to import certificate: ",
                      chromeos::platform_keys::StatusToString(status)});
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return;
  }

  UpdateState(FROM_HERE, CertProvisioningWorkerState::kSucceeded);
}

bool CertProvisioningWorkerStatic::ProcessResponseErrors(
    DeviceManagementServerRequestType request_type,
    policy::DeviceManagementStatus status,
    absl::optional<CertProvisioningResponseErrorType> error,
    absl::optional<int64_t> try_later) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if ((status ==
       policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE) ||
      (status == policy::DeviceManagementStatus::DM_STATUS_REQUEST_FAILED) ||
      (status == policy::DeviceManagementStatus::DM_STATUS_HTTP_STATUS_ERROR)) {
    LOG(WARNING) << "Connection to DM Server failed, error: " << status
                 << " for profile ID: " << cert_profile_.profile_id
                 << " in state: "
                 << CertificateProvisioningWorkerStateToString(state_);
    last_backend_server_error_ =
        BackendServerError(status, base::Time::NowFromSystemTime());
    request_backoff_.InformOfRequest(false);
    ScheduleNextStep(request_backoff_.GetTimeUntilRelease());
    return false;
  }

  // From this point, connection to the DM Server was successful.
  last_backend_server_error_ = absl::nullopt;
  if (status ==
      policy::DeviceManagementStatus::DM_STATUS_SERVICE_ACTIVATION_PENDING) {
    const base::TimeDelta try_later_delay =
        GetTryLaterDelayForRequestType(request_type);
    LOG(ERROR) << "A device management server request of type: "
               << static_cast<int>(request_type)
               << " will be retried after: " << try_later_delay;

    ScheduleNextStep(std::move(try_later_delay));
    return false;
  }

  if (status != policy::DeviceManagementStatus::DM_STATUS_SUCCESS) {
    failure_message_ = base::StrCat(
        {"DM Server returned error: ", base::NumberToString(status),
         " for profile ID: ", cert_profile_.profile_id,
         " in state: ", CertificateProvisioningWorkerStateToString(state_)});
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return false;
  }

  request_backoff_.InformOfRequest(true);

  if (error.has_value() &&
      (error.value() == CertProvisioningResponseError::INCONSISTENT_DATA)) {
    LOG(ERROR) << "Server response contains error: " << error.value()
               << " for profile ID: " << cert_profile_.profile_id
               << " in state: "
               << CertificateProvisioningWorkerStateToString(state_);
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kInconsistentDataError);
    return false;
  }

  if (error.has_value()) {
    failure_message_ = base::StrCat(
        {"Server response contains error: ",
         base::NumberToString(error.value()),
         " for profile ID: ", cert_profile_.profile_id,
         " in state: ", CertificateProvisioningWorkerStateToString(state_)});
    UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed);
    return false;
  }

  if (try_later.has_value()) {
    ScheduleNextStep(base::Milliseconds(try_later.value()));
    return false;
  }

  return true;
}

void CertProvisioningWorkerStatic::ScheduleNextStep(base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  delay = std::max(delay, kMinumumTryAgainLaterDelay);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CertProvisioningWorkerStatic::OnShouldContinue,
                     weak_factory_.GetWeakPtr(), ContinueReason::kTimeout),
      delay);

  is_waiting_ = true;
  VLOG(0) << "Next step scheduled in " << delay;

  last_update_time_ = base::Time::NowFromSystemTime();
  state_change_callback_.Run();
}

void CertProvisioningWorkerStatic::OnShouldContinue(ContinueReason reason) {
  switch (reason) {
    case ContinueReason::kInvalidation:
      RecordEvent(cert_profile_.protocol_version, cert_scope_,
                  CertProvisioningEvent::kInvalidationReceived);
      break;
    case ContinueReason::kTimeout:
      RecordEvent(cert_profile_.protocol_version, cert_scope_,
                  CertProvisioningEvent::kWorkerRetryWithoutInvalidation);
      break;
  }

  // Worker is already doing something.
  if (!IsWaiting()) {
    return;
  }

  is_continued_without_invalidation_for_uma_ =
      (reason == ContinueReason::kTimeout);

  DoStep();
}

void CertProvisioningWorkerStatic::CancelScheduledTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_factory_.InvalidateWeakPtrs();
}

void CertProvisioningWorkerStatic::CleanUpAndRunCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UnregisterFromInvalidationTopic();

  // Keep conditions mutually exclusive.
  if (state_ == CertProvisioningWorkerState::kSucceeded) {
    // No extra clean up is necessary.
    OnCleanUpDone();
    return;
  }

  const int prev_state_idx = GetStateOrderedIndex(prev_state_);
  const int key_generated_idx =
      GetStateOrderedIndex(CertProvisioningWorkerState::kKeypairGenerated);
  const int key_registered_idx =
      GetStateOrderedIndex(CertProvisioningWorkerState::kKeyRegistered);

  // Keep conditions mutually exclusive.
  if ((prev_state_idx >= key_generated_idx) &&
      (prev_state_idx < key_registered_idx)) {
    DeleteVaKey(cert_scope_, profile_, GetKeyName(cert_profile_.profile_id),
                base::BindOnce(&CertProvisioningWorkerStatic::OnDeleteVaKeyDone,
                               weak_factory_.GetWeakPtr()));
    return;
  }

  // Keep conditions mutually exclusive.
  if (!public_key_.empty() && (prev_state_idx >= key_registered_idx)) {
    platform_keys_service_->RemoveKey(
        GetPlatformKeysTokenId(cert_scope_), public_key_,
        base::BindOnce(&CertProvisioningWorkerStatic::OnRemoveKeyDone,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // No extra clean up is necessary.
  OnCleanUpDone();
}

void CertProvisioningWorkerStatic::OnDeleteVaKeyDone(bool delete_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!delete_result) {
    LOG(ERROR) << "Failed to delete a va key";
  }
  OnCleanUpDone();
}

void CertProvisioningWorkerStatic::OnRemoveKeyDone(
    chromeos::platform_keys::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    LOG(ERROR) << "Failed to delete a key: "
               << chromeos::platform_keys::StatusToString(status);
  }

  OnCleanUpDone();
}

void CertProvisioningWorkerStatic::OnCleanUpDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordResult(cert_profile_.protocol_version, cert_scope_, state_,
               prev_state_);
  std::move(result_callback_).Run(cert_profile_, state_);
}

CertProvisioningClient::ProvisioningProcess
CertProvisioningWorkerStatic::GetProvisioningProcessForClient() {
  return CertProvisioningClient::ProvisioningProcess(
      /*cert_scope=*/cert_scope_,
      /*cert_profile_id=*/cert_profile_.profile_id,
      /*policy_version=*/cert_profile_.policy_version,
      /*public_key=*/public_key_);
}

void CertProvisioningWorkerStatic::HandleSerialization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_) {
    case CertProvisioningWorkerState::kInitState:
      break;
    case CertProvisioningWorkerState::kKeypairGenerated:
      CertProvisioningSerializer::SerializeWorkerToPrefs(pref_service_, *this);
      break;
    case CertProvisioningWorkerState::kStartCsrResponseReceived:
      // StartCSR response contains VA challenge and data to sign. It is allowed
      // to build only one VA challenge response and sign only one data with the
      // same key. To make sure that the key is not used again after
      // deserialization, the serialized state should be deleted here. Also
      // lifetime of the VA challenge is very short and most likely it would not
      // survive long enough anyway.
      CertProvisioningSerializer::DeleteWorkerFromPrefs(pref_service_, *this);
      break;
    case CertProvisioningWorkerState::kVaChallengeFinished:
    case CertProvisioningWorkerState::kKeyRegistered:
    case CertProvisioningWorkerState::kKeypairMarked:
    case CertProvisioningWorkerState::kSignCsrFinished:
      break;
    case CertProvisioningWorkerState::kFinishCsrResponseReceived:
      CertProvisioningSerializer::SerializeWorkerToPrefs(pref_service_, *this);
      break;
    case CertProvisioningWorkerState::kSucceeded:
    case CertProvisioningWorkerState::kInconsistentDataError:
    case CertProvisioningWorkerState::kFailed:
    case CertProvisioningWorkerState::kCanceled:
      CertProvisioningSerializer::DeleteWorkerFromPrefs(pref_service_, *this);
      break;
    case CertProvisioningWorkerState::kReadyForNextOperation:
    case CertProvisioningWorkerState::kAuthorizeInstructionReceived:
    case CertProvisioningWorkerState::kProofOfPossessionInstructionReceived:
    case CertProvisioningWorkerState::kImportCertificateInstructionReceived:
      // These states are not used in the "static" flow.
      CHECK(false);
      break;
  }
}

void CertProvisioningWorkerStatic::InitAfterDeserialization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RegisterForInvalidationTopic();

  tpm_challenge_key_subtle_impl_ =
      attestation::TpmChallengeKeySubtleFactory::CreateForPreparedKey(
          GetVaKeyType(cert_scope_),
          /*will_register_key=*/true, ::attestation::KEY_TYPE_RSA,
          GetKeyName(cert_profile_.profile_id), BytesToStr(public_key_),
          profile_);
}

void CertProvisioningWorkerStatic::RegisterForInvalidationTopic() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(invalidator_);

  // Can be empty after deserialization if no topic was received yet. Also
  // protects from errors on the server side.
  if (invalidation_topic_.empty()) {
    return;
  }

  // Registering the callback with base::Unretained is OK because this class
  // owns |invalidator_|, and the callback will never be called after
  // |invalidator_| is destroyed.
  invalidator_->Register(
      invalidation_topic_,
      base::BindRepeating(&CertProvisioningWorkerStatic::OnShouldContinue,
                          base::Unretained(this),
                          ContinueReason::kInvalidation));

  RecordEvent(cert_profile_.protocol_version, cert_scope_,
              CertProvisioningEvent::kRegisteredToInvalidationTopic);
}

void CertProvisioningWorkerStatic::UnregisterFromInvalidationTopic() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(invalidator_);

  invalidator_->Unregister();
}

}  // namespace ash::cert_provisioning
