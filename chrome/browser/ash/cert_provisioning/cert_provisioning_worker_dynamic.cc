// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker_dynamic.h"

#include <stdint.h>

#include <optional>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
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
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_context.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"

// This will execute the `UpdateStateStatement` and return from the current
// function if the worker has reached a final state.
// It can only be used in void functions.
#define RETURN_ON_FINAL_STATE(UpdateStateStatement)               \
  if ((UpdateStateStatement) == UpdateStateResult::kFinalState) { \
    return;                                                       \
  }

// This will execute the `UpdateStateStatement` and expect that the worker is in
// a final state afterwards.
#define FINAL_STATE_EXPECTED(UpdateStateStatement)                \
  if ((UpdateStateStatement) != UpdateStateResult::kFinalState) { \
    NOTREACHED_IN_MIGRATION();                                    \
  }

namespace em = enterprise_management;

namespace ash::cert_provisioning {

namespace {

constexpr unsigned int kNonVaKeyModulusLengthBits = 2048;
constexpr char kEcNamedCurve[] = "P-256";

constexpr base::TimeDelta kMinumumTryAgainLaterDelay = base::Seconds(10);

const net::BackoffEntry::Policy kBackoffPolicy{
    /*num_errors_to_ignore=*/0,
    /*initial_delay_ms=*/
    base::checked_cast<int>(base::Seconds(30).InMilliseconds()),
    /*multiply_factor=*/2.0,
    /*jitter_factor=*/0.15,
    /*maximum_backoff_ms=*/base::Hours(12).InMilliseconds(),
    /*entry_lifetime_ms=*/-1,
    /*always_use_initial_delay=*/false};

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
  return base::StrCat({failure_message, challenge_result.GetErrorMessage()});
}

// TODO(b/192071491): Remove the use of this function by changing the
// dependencies.
std::vector<uint8_t> StrToBytes(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

// TODO(b/192071491): Remove the use of this function by changing the
// dependencies.
std::string BytesToStr(const std::vector<uint8_t>& blob) {
  return std::string(blob.begin(), blob.end());
}

bool IsInstructionReceivedState(CertProvisioningWorkerState state) {
  switch (state) {
    case CertProvisioningWorkerState::kAuthorizeInstructionReceived:
    case CertProvisioningWorkerState::kProofOfPossessionInstructionReceived:
    case CertProvisioningWorkerState::kImportCertificateInstructionReceived:
      return true;
    default:
      return false;
  }
}

bool IsStateTransitionAllowed(CertProvisioningWorkerState prev_state,
                              CertProvisioningWorkerState new_state) {
  if (prev_state == new_state) {
    return true;
  }
  if (IsFinalState(prev_state)) {
    // No transition out of final states.
    return false;
  }
  if (IsFinalState(new_state)) {
    // It's always possible to go to final states.
    return true;
  }

  switch (prev_state) {
    case CertProvisioningWorkerState::kInitState:
      return new_state == CertProvisioningWorkerState::kKeypairGenerated;
    case CertProvisioningWorkerState::kKeypairGenerated:
      return new_state == CertProvisioningWorkerState::kReadyForNextOperation;
    case CertProvisioningWorkerState::kReadyForNextOperation:
      return IsInstructionReceivedState(new_state);
    case CertProvisioningWorkerState::kAuthorizeInstructionReceived:
      return new_state == CertProvisioningWorkerState::kVaChallengeFinished;
    case CertProvisioningWorkerState::kProofOfPossessionInstructionReceived:
      return new_state == CertProvisioningWorkerState::kSignCsrFinished;
    case CertProvisioningWorkerState::kImportCertificateInstructionReceived:
      // After "Import Cert", only final states are expected.
      return false;
    case CertProvisioningWorkerState::kVaChallengeFinished:
      return new_state == CertProvisioningWorkerState::kKeyRegistered;
    case CertProvisioningWorkerState::kKeyRegistered:
      return new_state == CertProvisioningWorkerState::kKeypairMarked;
    case CertProvisioningWorkerState::kKeypairMarked:
      return new_state == CertProvisioningWorkerState::kReadyForNextOperation;
    case CertProvisioningWorkerState::kSignCsrFinished:
      return new_state == CertProvisioningWorkerState::kReadyForNextOperation;
    case CertProvisioningWorkerState::kSucceeded:
    case CertProvisioningWorkerState::kInconsistentDataError:
    case CertProvisioningWorkerState::kFailed:
    case CertProvisioningWorkerState::kCanceled:
      // These are final state, so they should already be handled above.
      CHECK(false);
      return false;
    case CertProvisioningWorkerState::kStartCsrResponseReceived:
    case CertProvisioningWorkerState::kFinishCsrResponseReceived:
      // Not used in "dynamic" flow.
      CHECK(false);
      return false;
  }
}

}  // namespace

// ===================== CertProvisioningWorkerDynamic =========================

CertProvisioningWorkerDynamic::CertProvisioningWorkerDynamic(
    std::string cert_provisioning_process_id,
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    const CertProfile& cert_profile,
    CertProvisioningClient* cert_provisioning_client,
    std::unique_ptr<CertProvisioningInvalidator> invalidator,
    base::RepeatingClosure state_change_callback,
    CertProvisioningWorkerCallback result_callback)
    : process_id_(cert_provisioning_process_id),
      cert_scope_(cert_scope),
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

CertProvisioningWorkerDynamic::~CertProvisioningWorkerDynamic() = default;

bool CertProvisioningWorkerDynamic::IsWaiting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return is_waiting_;
}

bool CertProvisioningWorkerDynamic::IsWorkerMarkedForReset() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return is_schedueled_for_reset_;
}

const CertProfile& CertProvisioningWorkerDynamic::GetCertProfile() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return cert_profile_;
}

const std::vector<uint8_t>& CertProvisioningWorkerDynamic::GetPublicKey()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return public_key_;
}

CertProvisioningWorkerState CertProvisioningWorkerDynamic::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return state_;
}

CertProvisioningWorkerState CertProvisioningWorkerDynamic::GetPreviousState()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return prev_state_;
}

base::Time CertProvisioningWorkerDynamic::GetLastUpdateTime() const {
  return last_update_time_;
}

const std::optional<BackendServerError>&
CertProvisioningWorkerDynamic::GetLastBackendServerError() const {
  return last_backend_server_error_;
}

std::string CertProvisioningWorkerDynamic::GetFailureMessage() const {
  return failure_message_ui_.value_or(failure_message_);
}

void CertProvisioningWorkerDynamic::Stop(CertProvisioningWorkerState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(IsFinalState(state));

  CancelScheduledTasks();
  FINAL_STATE_EXPECTED(UpdateState(FROM_HERE, state));
}

void CertProvisioningWorkerDynamic::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CancelScheduledTasks();
  is_waiting_ = true;
}

void CertProvisioningWorkerDynamic::DoStep() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CancelScheduledTasks();
  is_waiting_ = false;
  switch (state_) {
    case CertProvisioningWorkerState::kInitState:
      GenerateKey();
      return;
    case CertProvisioningWorkerState::kKeypairGenerated:
      Start();
      return;
    case CertProvisioningWorkerState::kReadyForNextOperation:
      GetNextInstruction();
      return;
    case CertProvisioningWorkerState::kAuthorizeInstructionReceived:
      BuildVaChallengeResponse();
      return;
    case CertProvisioningWorkerState::kProofOfPossessionInstructionReceived:
      BuildProofOfPossession();
      return;
    case CertProvisioningWorkerState::kImportCertificateInstructionReceived:
      ImportCert();
      return;
    case CertProvisioningWorkerState::kVaChallengeFinished:
      RegisterKey();
      return;
    case CertProvisioningWorkerState::kKeyRegistered:
      MarkVaGeneratedKey();
      return;
    case CertProvisioningWorkerState::kKeypairMarked:
      UploadAuthorization();
      return;
    case CertProvisioningWorkerState::kSignCsrFinished:
      UploadProofOfPossession();
      return;
    case CertProvisioningWorkerState::kSucceeded:
    case CertProvisioningWorkerState::kInconsistentDataError:
    case CertProvisioningWorkerState::kFailed:
    case CertProvisioningWorkerState::kCanceled:
      DCHECK(false);
      return;
    case CertProvisioningWorkerState::kStartCsrResponseReceived:
    case CertProvisioningWorkerState::kFinishCsrResponseReceived:
      // Not used in "dynamic" flow.
      CHECK(false);
      return;
  }
  NOTREACHED_IN_MIGRATION() << " " << static_cast<uint>(state_);
}

void CertProvisioningWorkerDynamic::MarkWorkerForReset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_schedueled_for_reset_ = true;
}

CertProvisioningWorkerDynamic::UpdateStateResult
CertProvisioningWorkerDynamic::UpdateState(
    const base::Location& from_here,
    CertProvisioningWorkerState new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(kDynamicWorkerStates.Has(new_state)) << static_cast<int>(new_state);

  if (!IsStateTransitionAllowed(state_, new_state)) {
    failure_message_ = base::StrCat(
        {"Invalid state transition from ", from_here.ToString(),
         " state=", CertificateProvisioningWorkerStateToString(state_),
         " new_state=", CertificateProvisioningWorkerStateToString(new_state),
         GetLogInfoBlock()});
    new_state = CertProvisioningWorkerState::kFailed;
  }

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
    return UpdateStateResult::kFinalState;
  }
  return UpdateStateResult::kNonFinalState;
}

void CertProvisioningWorkerDynamic::GenerateKey() {
  if (cert_profile_.is_va_enabled) {
    GenerateKeyForVa();
  } else {
    GenerateRegularKey();
  }
}

void CertProvisioningWorkerDynamic::GenerateRegularKey() {
  switch (cert_profile_.key_type) {
    case KeyType::kRsa:
      platform_keys_service_->GenerateRSAKey(
          GetPlatformKeysTokenId(cert_scope_), kNonVaKeyModulusLengthBits,
          /*sw_backed=*/false,
          base::BindOnce(
              &CertProvisioningWorkerDynamic::OnGenerateRegularKeyDone,
              weak_factory_.GetWeakPtr()));
      break;
    case KeyType::kEc:
      platform_keys_service_->GenerateECKey(
          GetPlatformKeysTokenId(cert_scope_), kEcNamedCurve,
          base::BindOnce(
              &CertProvisioningWorkerDynamic::OnGenerateRegularKeyDone,
              weak_factory_.GetWeakPtr()));
  }
}

void CertProvisioningWorkerDynamic::OnGenerateRegularKeyDone(
    std::vector<uint8_t> public_key_spki_der,
    chromeos::platform_keys::Status status) {
  if (status != chromeos::platform_keys::Status::kSuccess ||
      public_key_spki_der.empty()) {
    failure_message_ = base::StrCat(
        {"Failed to prepare a non-VA key: ",
         chromeos::platform_keys::StatusToString(status), GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  key_location_ = KeyLocation::kPkcs11Token;
  public_key_ = std::move(public_key_spki_der);
  MarkRegularKey();
}

void CertProvisioningWorkerDynamic::GenerateKeyForVa() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tpm_challenge_key_subtle_impl_ =
      attestation::TpmChallengeKeySubtleFactory::Create();
  tpm_challenge_key_subtle_impl_->StartPrepareKeyStep(
      GetVaFlowType(cert_scope_),
      /*will_register_key=*/true, ::attestation::KEY_TYPE_RSA,
      GetKeyName(cert_profile_.profile_id), profile_,
      base::BindOnce(&CertProvisioningWorkerDynamic::OnGenerateKeyForVaDone,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      /*signals=*/std::nullopt);
}

void CertProvisioningWorkerDynamic::OnGenerateKeyForVaDone(
    base::TimeTicks start_time,
    const attestation::TpmChallengeKeyResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordKeypairGenerationTime(cert_profile_.protocol_version, cert_scope_,
                              base::TimeTicks::Now() - start_time);

  if (result.result_code ==
      attestation::TpmChallengeKeyResultCode::kGetCertificateFailedError) {
    LOG(WARNING) << base::StrCat(
        {"Failed to get certificate for a key", GetLogInfoBlock()});
    request_backoff_.InformOfRequest(false);
    // Next DoStep will retry generating the key.
    ScheduleNextStep(request_backoff_.GetTimeUntilRelease(),
                     /*try_provisioning_on_timeout=*/true);
    return;
  }

  if (!result.IsSuccess() || result.public_key.empty()) {
    failure_message_ =
        base::StrCat({"Failed to prepare a key: ", result.GetErrorMessage(),
                      GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  key_location_ = KeyLocation::kVaDatabase;
  public_key_ = StrToBytes(result.public_key);
  RETURN_ON_FINAL_STATE(
      UpdateState(FROM_HERE, CertProvisioningWorkerState::kKeypairGenerated));
  DoStep();
}

void CertProvisioningWorkerDynamic::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // From this point, connection to the DM Server was successful.
  cert_provisioning_client_->Start(
      GetProvisioningProcessForClient(),
      base::BindOnce(&CertProvisioningWorkerDynamic::OnStartResponse,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerDynamic::OnStartResponse(
    base::expected<enterprise_management::CertProvStartResponse,
                   CertProvisioningClient::Error> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ProcessResponseErrors(response)) {
    return;
  }

  invalidation_topic_ = response.value().invalidation_topic();
  RegisterForInvalidationTopic();

  RETURN_ON_FINAL_STATE(UpdateState(
      FROM_HERE, CertProvisioningWorkerState::kReadyForNextOperation));
  DoStep();
}

void CertProvisioningWorkerDynamic::GetNextInstruction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cert_provisioning_client_->GetNextInstruction(
      GetProvisioningProcessForClient(),
      base::BindOnce(
          &CertProvisioningWorkerDynamic::OnGetNextInstructionResponse,
          weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerDynamic::OnGetNextInstructionResponse(
    base::expected<enterprise_management::CertProvGetNextInstructionResponse,
                   CertProvisioningClient::Error> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ProcessResponseErrors(response)) {
    return;
  }

  const em::CertProvGetNextInstructionResponse& next_instruction_response =
      response.value();
  if (next_instruction_response.has_authorize_instruction()) {
    OnAuthorizeInstructionReceived(
        next_instruction_response.authorize_instruction());
    return;
  }
  if (next_instruction_response.has_proof_of_possession_instruction()) {
    OnProofOfPossessionInstructionReceived(
        next_instruction_response.proof_of_possession_instruction());
    return;
  }
  if (next_instruction_response.has_import_certificate_instruction()) {
    OnImportCertificateInstructionReceived(
        next_instruction_response.import_certificate_instruction());
    return;
  }
  // CertProvisioningClient ensures that at least one of the instructions was
  // filled.
  NOTREACHED_IN_MIGRATION();
}

void CertProvisioningWorkerDynamic::OnAuthorizeInstructionReceived(
    const em::CertProvAuthorizeInstruction& authorize_instruction) {
  if (!cert_profile_.is_va_enabled) {
    failure_message_ = base::StrCat({"VA not enabled", GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }
  if (attempted_va_challenge_) {
    failure_message_ =
        base::StrCat({"VA only possible once", GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }
  attempted_va_challenge_ = true;

  va_challenge_ = authorize_instruction.va_challenge();
  RETURN_ON_FINAL_STATE(UpdateState(
      FROM_HERE, CertProvisioningWorkerState::kAuthorizeInstructionReceived));
  DoStep();
}

void CertProvisioningWorkerDynamic::OnProofOfPossessionInstructionReceived(
    const em::CertProvProofOfPossessionInstruction&
        proof_of_possession_instruction) {
  if (cert_profile_.is_va_enabled && !attempted_va_challenge_) {
    failure_message_ =
        base::StrCat({"Expected VA challenge", GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  if (proof_of_possession_instruction.has_signature_algorithm()) {
    signature_algorithm_ =
        proof_of_possession_instruction.signature_algorithm();
  } else {
    failure_message_ =
        base::StrCat({"No signature algorithm provided", GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  data_to_sign_ = StrToBytes(proof_of_possession_instruction.data_to_sign());
  RETURN_ON_FINAL_STATE(UpdateState(
      FROM_HERE,
      CertProvisioningWorkerState::kProofOfPossessionInstructionReceived));
  DoStep();
}

void CertProvisioningWorkerDynamic::OnImportCertificateInstructionReceived(
    const em::CertProvImportCertificateInstruction&
        import_certificate_instruction) {
  if (cert_profile_.is_va_enabled && !attempted_va_challenge_) {
    failure_message_ =
        base::StrCat({"Expected VA challenge", GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  pem_encoded_certificate_ =
      import_certificate_instruction.pem_encoded_certificate();
  RETURN_ON_FINAL_STATE(UpdateState(
      FROM_HERE,
      CertProvisioningWorkerState::kImportCertificateInstructionReceived));
  DoStep();
}

void CertProvisioningWorkerDynamic::BuildVaChallengeResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tpm_challenge_key_subtle_impl_->StartSignChallengeStep(
      std::move(va_challenge_),
      base::BindOnce(
          &CertProvisioningWorkerDynamic::OnBuildVaChallengeResponseDone,
          weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
  va_challenge_.clear();
}

void CertProvisioningWorkerDynamic::OnBuildVaChallengeResponseDone(
    base::TimeTicks start_time,
    const attestation::TpmChallengeKeyResult& challenge_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordVerifiedAccessTime(cert_profile_.protocol_version, cert_scope_,
                           base::TimeTicks::Now() - start_time);

  if (!challenge_result.IsSuccess()) {
    failure_message_ = ConstructFailureMessage(challenge_result);
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  if (challenge_result.challenge_response.empty()) {
    failure_message_ =
        base::StrCat({"Challenge response is empty", GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  va_challenge_response_ = challenge_result.challenge_response;
  RETURN_ON_FINAL_STATE(UpdateState(
      FROM_HERE, CertProvisioningWorkerState::kVaChallengeFinished));
  DoStep();
}

void CertProvisioningWorkerDynamic::RegisterKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tpm_challenge_key_subtle_impl_->StartRegisterKeyStep(
      base::BindOnce(&CertProvisioningWorkerDynamic::OnRegisterKeyDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerDynamic::OnRegisterKeyDone(
    const attestation::TpmChallengeKeyResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  key_location_ = KeyLocation::kPkcs11Token;
  tpm_challenge_key_subtle_impl_.reset();

  if (!result.IsSuccess()) {
    failure_message_ =
        base::StrCat({"Failed to register key: ", result.GetErrorMessage(),
                      GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  RETURN_ON_FINAL_STATE(
      UpdateState(FROM_HERE, CertProvisioningWorkerState::kKeyRegistered));
  DoStep();
}

void CertProvisioningWorkerDynamic::MarkRegularKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MarkKey(CertProvisioningWorkerState::kKeypairGenerated);
}

void CertProvisioningWorkerDynamic::MarkVaGeneratedKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MarkKey(CertProvisioningWorkerState::kKeypairMarked);
}

void CertProvisioningWorkerDynamic::MarkKey(
    CertProvisioningWorkerState target_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MarkKeyAsCorporate();

  platform_keys_service_->SetAttributeForKey(
      GetPlatformKeysTokenId(cert_scope_), public_key_,
      chromeos::platform_keys::KeyAttributeType::kCertificateProvisioningId,
      StrToBytes(cert_profile_.profile_id),
      base::BindOnce(&CertProvisioningWorkerDynamic::OnMarkKeyDone,
                     weak_factory_.GetWeakPtr(), target_state));
}

// Marks the key |public_key_spki_der| as corporate. |profile| can be nullptr if
// |scope| is CertScope::kDevice. The method is asynchronous, but the worker
// doesn't have to wait for it to finish.
void CertProvisioningWorkerDynamic::MarkKeyAsCorporate() {
  CHECK(profile_ || cert_scope_ == CertScope::kDevice);

  GetKeyPermissionsManager(cert_scope_, profile_)
      ->AllowKeyForUsage(
          base::BindOnce(&CertProvisioningWorkerDynamic::OnAllowKeyForUsageDone,
                         weak_factory_.GetWeakPtr()),
          platform_keys::KeyUsage::kCorporate, public_key_);
}

void CertProvisioningWorkerDynamic::OnAllowKeyForUsageDone(
    chromeos::platform_keys::Status status) {
  if (status != chromeos::platform_keys::Status::kSuccess) {
    LOG(ERROR) << "Cannot mark key corporate: "
               << chromeos::platform_keys::StatusToString(status)
               << GetLogInfoBlock();
  }
}

void CertProvisioningWorkerDynamic::OnMarkKeyDone(
    CertProvisioningWorkerState target_state,
    chromeos::platform_keys::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    failure_message_ = base::StrCat(
        {"Failed to mark a key: ",
         chromeos::platform_keys::StatusToString(status), GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  RETURN_ON_FINAL_STATE(UpdateState(FROM_HERE, target_state));
  DoStep();
}

void CertProvisioningWorkerDynamic::UploadAuthorization() {
  cert_provisioning_client_->Authorize(
      GetProvisioningProcessForClient(), va_challenge_response_,
      base::BindOnce(
          &CertProvisioningWorkerDynamic::OnUploadAuthorizationResponse,
          weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerDynamic::OnUploadAuthorizationResponse(
    base::expected<void, CertProvisioningClient::Error> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ProcessResponseErrors(response)) {
    return;
  }
  va_challenge_response_.clear();

  RETURN_ON_FINAL_STATE(UpdateState(
      FROM_HERE, CertProvisioningWorkerState::kReadyForNextOperation));
  DoStep();
}

void CertProvisioningWorkerDynamic::BuildProofOfPossession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (attempted_proof_of_possession_) {
    failure_message_ = base::StrCat(
        {"Proof of possession requested >1 times", GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }
  attempted_proof_of_possession_ = true;

  switch (signature_algorithm_) {
    case em::CertProvSignatureAlgorithm::
        SIGNATURE_ALGORITHM_RSA_PKCS1_V1_5_NO_HASH:
      platform_keys_service_->SignRSAPKCS1Raw(
          GetPlatformKeysTokenId(cert_scope_), std::move(data_to_sign_),
          public_key_,
          base::BindRepeating(
              &CertProvisioningWorkerDynamic::OnBuildProofOfPossessionDone,
              weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
      break;
    case em::CertProvSignatureAlgorithm::SIGNATURE_ALGORITHM_ECDSA_SHA256:
      platform_keys_service_->SignEcdsa(
          GetPlatformKeysTokenId(cert_scope_), std::move(data_to_sign_),
          public_key_, chromeos::platform_keys::HASH_ALGORITHM_SHA256,
          base::BindRepeating(
              &CertProvisioningWorkerDynamic::OnBuildProofOfPossessionDone,
              weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
      break;
    case em::CertProvSignatureAlgorithm::SIGNATURE_ALGORITHM_UNSPECIFIED:
      failure_message_ =
          base::StrCat({"Unknown signature algorithm", GetLogInfoBlock()});
      FINAL_STATE_EXPECTED(
          UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
      return;
  }

  data_to_sign_.clear();
}

void CertProvisioningWorkerDynamic::OnBuildProofOfPossessionDone(
    base::TimeTicks start_time,
    std::vector<uint8_t> signature,
    chromeos::platform_keys::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordDataSignTime(cert_profile_.protocol_version, cert_scope_,
                     base::TimeTicks::Now() - start_time);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    failure_message_ = base::StrCat(
        {"Failed to sign data: ",
         chromeos::platform_keys::StatusToString(status), GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  signature_ = std::move(signature);
  RETURN_ON_FINAL_STATE(
      UpdateState(FROM_HERE, CertProvisioningWorkerState::kSignCsrFinished));
  DoStep();
}

void CertProvisioningWorkerDynamic::UploadProofOfPossession() {
  cert_provisioning_client_->UploadProofOfPossession(
      GetProvisioningProcessForClient(), BytesToStr(signature_),
      base::BindOnce(
          &CertProvisioningWorkerDynamic::OnUploadProofOfPossessionResponse,
          weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerDynamic::OnUploadProofOfPossessionResponse(
    base::expected<void, CertProvisioningClient::Error> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ProcessResponseErrors(response)) {
    return;
  }

  signature_.clear();

  RETURN_ON_FINAL_STATE(UpdateState(
      FROM_HERE, CertProvisioningWorkerState::kReadyForNextOperation));
  DoStep();
}

void CertProvisioningWorkerDynamic::ImportCert() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<net::X509Certificate> cert = CreateSingleCertificateFromBytes(
      pem_encoded_certificate_.data(), pem_encoded_certificate_.size());
  if (!cert) {
    failure_message_ =
        base::StrCat({"Failed to parse a certificate", GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  std::vector<uint8_t> public_key_from_cert =
      chromeos::platform_keys::GetSubjectPublicKeyInfoBlob(cert);
  if (public_key_from_cert != public_key_) {
    failure_message_ = base::StrCat(
        {"Downloaded certificate does not match the expected key pair.",
         GetLogInfoBlock()});
    failure_message_ui_ = base::StrCat(
        {"Downloaded certificate does not match the expected key pair. ",
         "Expected: ", base::Base64Encode(public_key_), " ",
         "Public key from cert: ", base::Base64Encode(public_key_from_cert),
         "\n", "Cert: ", pem_encoded_certificate_, GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  platform_keys_service_->ImportCertificate(
      GetPlatformKeysTokenId(cert_scope_), cert,
      base::BindRepeating(&CertProvisioningWorkerDynamic::OnImportCertDone,
                          weak_factory_.GetWeakPtr()));
}

void CertProvisioningWorkerDynamic::OnImportCertDone(
    chromeos::platform_keys::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    failure_message_ = base::StrCat(
        {"Failed to import certificate: ",
         chromeos::platform_keys::StatusToString(status), GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  FINAL_STATE_EXPECTED(
      UpdateState(FROM_HERE, CertProvisioningWorkerState::kSucceeded));
}

template <typename ResultType>
bool CertProvisioningWorkerDynamic::ProcessResponseErrors(
    const base::expected<ResultType, CertProvisioningClient::Error>& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (response.has_value()) {
    last_backend_server_error_ = std::nullopt;
    return true;
  }

  ProcessResponseErrors(response.error());
  return false;
}

void CertProvisioningWorkerDynamic::ProcessResponseErrors(
    const CertProvisioningClient::Error& error) {
  const policy::DeviceManagementStatus status = error.device_management_status;
  if ((status ==
       policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE) ||
      (status == policy::DeviceManagementStatus::DM_STATUS_REQUEST_FAILED) ||
      (status == policy::DeviceManagementStatus::DM_STATUS_HTTP_STATUS_ERROR)) {
    LOG(WARNING) << "Connection to DM Server failed, error: " << status
                 << " for profile ID: " << cert_profile_.profile_id
                 << " in state: "
                 << CertificateProvisioningWorkerStateToString(state_)
                 << GetLogInfoBlock();
    last_backend_server_error_ =
        BackendServerError(status, base::Time::NowFromSystemTime());
    request_backoff_.InformOfRequest(false);
    ScheduleNextStep(request_backoff_.GetTimeUntilRelease(),
                     /*try_provisioning_on_timeout=*/true);
    return;
  }

  // From this point, connection to the DM Server was successful.
  last_backend_server_error_ = std::nullopt;
  if (status != policy::DeviceManagementStatus::DM_STATUS_SUCCESS) {
    failure_message_ = base::StrCat(
        {"DM Server returned error: ", base::NumberToString(status),
         " for profile ID: ", cert_profile_.profile_id,
         " in state: ", CertificateProvisioningWorkerStateToString(state_),
         GetLogInfoBlock()});
    FINAL_STATE_EXPECTED(
        UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
    return;
  }

  request_backoff_.InformOfRequest(true);

  const em::CertProvBackendError& backend_error = error.backend_error;
  if (backend_error.error() ==
      em::CertProvBackendError::INSTRUCTION_NOT_YET_AVAILABLE) {
    LOG(WARNING) << "No instruction available yet "
                 << " for profile ID: " << cert_profile_.profile_id
                 << GetLogInfoBlock();
    // Don't change state, just retry the operation in this state in a delay (or
    // when an invalidation is triggered).
    // TODO(b/289983352): Use a backoff strategy for the delay.
    ScheduleNextStep(
        base::Seconds(30),
        /*try_provisioning_on_timeout=*/!ShouldOnlyUseInvalidations());
    return;
  }

  if (backend_error.error() == em::CertProvBackendError::INCONSISTENT_DATA ||
      backend_error.error() == em::CertProvBackendError::PROFILE_NOT_FOUND) {
    // Report both INCONSISTENT_DATA and PROFILE_NOT_FOUND as
    // kInconsistentDataError because both mean that the locally-cached policy
    // does not match the server's database.
    LOG(ERROR) << "Server response contains error: " << backend_error.error()
               << " for profile ID: " << cert_profile_.profile_id
               << " in state: "
               << CertificateProvisioningWorkerStateToString(state_)
               << GetLogInfoBlock();
    FINAL_STATE_EXPECTED(UpdateState(
        FROM_HERE, CertProvisioningWorkerState::kInconsistentDataError));
    return;
  }

  failure_message_ = base::StrCat(
      {"Server response contains error: ",
       base::NumberToString(backend_error.error()),
       " for profile ID: ", cert_profile_.profile_id,
       " in state: ", CertificateProvisioningWorkerStateToString(state_),
       ". Debug message: ", backend_error.debug_message(), GetLogInfoBlock()});
  FINAL_STATE_EXPECTED(
      UpdateState(FROM_HERE, CertProvisioningWorkerState::kFailed));
  return;
}

void CertProvisioningWorkerDynamic::ScheduleNextStep(
    base::TimeDelta delay,
    bool try_provisioning_on_timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (try_provisioning_on_timeout) {
    delay = std::max(delay, kMinumumTryAgainLaterDelay);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CertProvisioningWorkerDynamic::OnShouldContinue,
                       weak_factory_.GetWeakPtr(), ContinueReason::kTimeout),
        delay);
    VLOG(0) << "Next step scheduled in " << delay << GetLogInfoBlock();
  }

  is_waiting_ = true;
  last_update_time_ = base::Time::NowFromSystemTime();
  state_change_callback_.Run();
}

void CertProvisioningWorkerDynamic::OnShouldContinue(ContinueReason reason) {
  switch (reason) {
    case ContinueReason::kSubscribedToInvalidation:
      RecordEvent(
          cert_profile_.protocol_version, cert_scope_,
          CertProvisioningEvent::kSuccessfullySubscribedToInvalidationTopic);
      break;
    case ContinueReason::kInvalidationReceived:
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

void CertProvisioningWorkerDynamic::CancelScheduledTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_factory_.InvalidateWeakPtrs();
}

// This method handles clean up.
// One of the things to be cleaned up are generated keys. It is possible that a
// worker is asked to cleanup and shutdown while a key is being generated for
// it. In that case this cleanup will miss that key and it's important to make
// sure that there is another mechanism that will eventually clean up the key.
// VA and PKS keys both are covered and the mechanism is described in seperate
// comments.
void CertProvisioningWorkerDynamic::CleanUpAndRunCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UnregisterFromInvalidationTopic();

  if (state_ == CertProvisioningWorkerState::kSucceeded) {
    // No extra clean up is necessary.
    OnCleanUpDone();
    return;
  }

  if (key_location_ == KeyLocation::kVaDatabase) {
    // if the worker is still waiting for the key right now, then it will be
    // eventually cleaned by the scheduler once it goes idle.
    DeleteVaKey(
        cert_scope_, profile_, GetKeyName(cert_profile_.profile_id),
        base::BindOnce(&CertProvisioningWorkerDynamic::OnDeleteVaKeyDone,
                       weak_factory_.GetWeakPtr()));
    return;
  } else if (key_location_ == KeyLocation::kPkcs11Token) {
    platform_keys_service_->RemoveKey(
        GetPlatformKeysTokenId(cert_scope_), public_key_,
        base::BindOnce(&CertProvisioningWorkerDynamic::OnRemoveKeyDone,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // If the worker is still waiting for a key from PlatformKeysService right
  // now, PlatformKeysService will clean up the key when the key is generated
  // and the worker is gone. No extra clean up is necessary.
  OnCleanUpDone();
}

void CertProvisioningWorkerDynamic::OnDeleteVaKeyDone(bool delete_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!delete_result) {
    LOG(ERROR) << "Failed to delete a va key" << GetLogInfoBlock();
  }
  OnCleanUpDone();
}

void CertProvisioningWorkerDynamic::OnRemoveKeyDone(
    chromeos::platform_keys::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    LOG(ERROR) << "Failed to delete a key: "
               << chromeos::platform_keys::StatusToString(status)
               << GetLogInfoBlock();
  }

  OnCleanUpDone();
}

void CertProvisioningWorkerDynamic::OnCleanUpDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordResult(cert_profile_.protocol_version, cert_scope_, state_,
               prev_state_);

  // The worked is likely to be deleted in `result_callback_`. Run it
  // asynchronously in case something is still interacting with it in the
  // current call stack.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback_), cert_profile_,
                                process_id_, state_));
}

CertProvisioningClient::ProvisioningProcess
CertProvisioningWorkerDynamic::GetProvisioningProcessForClient() {
  return CertProvisioningClient::ProvisioningProcess(
      process_id_, cert_scope_, cert_profile_.profile_id,
      cert_profile_.policy_version, public_key_);
}

void CertProvisioningWorkerDynamic::HandleSerialization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_) {
    case CertProvisioningWorkerState::kInitState:
      break;
    case CertProvisioningWorkerState::kKeypairGenerated:
      // Serialize as we're going to perform a server-side request which could
      // be repeated if we had e.g. no connectivity.
    case CertProvisioningWorkerState::kReadyForNextOperation:
      // Serialize as we're going to wait for a server-side instruction.
    case CertProvisioningWorkerState::kSignCsrFinished:
      // Serialize as we're going to upload a signature, which (contrary to the
      // VA challenge response) does not have a limited validity time.
      CertProvisioningSerializer::SerializeWorkerToPrefs(pref_service_, *this);
      break;
    case CertProvisioningWorkerState::kAuthorizeInstructionReceived:
    case CertProvisioningWorkerState::kProofOfPossessionInstructionReceived:
    case CertProvisioningWorkerState::kImportCertificateInstructionReceived:
      // Some operations can only be performed once, and most are expected to be
      // fast. If chrome restarts in the middle of an operation it is simpler to
      // start from scratch.
      CertProvisioningSerializer::DeleteWorkerFromPrefs(pref_service_, *this);
      break;
    case CertProvisioningWorkerState::kVaChallengeFinished:
    case CertProvisioningWorkerState::kKeyRegistered:
    case CertProvisioningWorkerState::kKeypairMarked:
      break;
    case CertProvisioningWorkerState::kSucceeded:
    case CertProvisioningWorkerState::kInconsistentDataError:
    case CertProvisioningWorkerState::kFailed:
    case CertProvisioningWorkerState::kCanceled:
      CertProvisioningSerializer::DeleteWorkerFromPrefs(pref_service_, *this);
      break;
    case CertProvisioningWorkerState::kStartCsrResponseReceived:
    case CertProvisioningWorkerState::kFinishCsrResponseReceived:
      // Not used in "dynamic" flow.
      CHECK(false);
  }
}

void CertProvisioningWorkerDynamic::InitAfterDeserialization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RegisterForInvalidationTopic();

  // Only initialize TpmChallengeKeySubtle if any Verified Access operations can
  // still happen, i.e. if VA is enabled and the key has not been moved into the
  // PKCS#11 token ("registered") yet.
  // TODO(b/364893005): Update to support ECC keys as well, once VA support is
  // ready.
  if (cert_profile_.is_va_enabled &&
      key_location_ != KeyLocation::kPkcs11Token) {
    tpm_challenge_key_subtle_impl_ =
        attestation::TpmChallengeKeySubtleFactory::CreateForPreparedKey(
            GetVaFlowType(cert_scope_),
            /*will_register_key=*/true, ::attestation::KEY_TYPE_RSA,
            GetKeyName(cert_profile_.profile_id), BytesToStr(public_key_),
            profile_);
  }
}

void CertProvisioningWorkerDynamic::RegisterForInvalidationTopic() {
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
      invalidation_topic_, MakeInvalidationListenerType(process_id_),
      base::BindRepeating(&CertProvisioningWorkerDynamic::OnInvalidationEvent,
                          base::Unretained(this)));

  RecordEvent(cert_profile_.protocol_version, cert_scope_,
              CertProvisioningEvent::kRegisteredToInvalidationTopic);
}

void CertProvisioningWorkerDynamic::UnregisterFromInvalidationTopic() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(invalidator_);

  invalidator_->Unregister();
}

void CertProvisioningWorkerDynamic::OnInvalidationEvent(
    InvalidationEvent invalidation_event) {
  // This function logs as WARNING so the messages are visible in feedback logs
  // to monitor for b/307340577 .
  switch (invalidation_event) {
    case InvalidationEvent::kSuccessfullySubscribed:
      LOG(WARNING) << "Successfully subscribed to invalidations"
                   << GetLogInfoBlock();
      OnShouldContinue(ContinueReason::kSubscribedToInvalidation);
      break;
    case InvalidationEvent::kInvalidationReceived:
      LOG(WARNING) << "Invalidation received" << GetLogInfoBlock();
      OnShouldContinue(ContinueReason::kInvalidationReceived);
      break;
  }
}

std::string CertProvisioningWorkerDynamic::GetLogInfoBlock() {
  return base::StringPrintf(" [cppId: %s]", process_id_.c_str());
}

}  // namespace ash::cert_provisioning
