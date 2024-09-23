// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace ash::cert_provisioning {

namespace em = enterprise_management;

// The type for variables containing an error from DM Server response.
using CertProvisioningResponseErrorType =
    em::ClientCertificateProvisioningResponse::Error;

using ResponseCase = em::ClientCertificateProvisioningResponse::ResponseCase;

namespace {

// Returns the device management protocol string representation of a CertScope.
std::string CertScopeToString(CertScope cert_scope) {
  switch (cert_scope) {
    case CertScope::kUser:
      return "google/chromeos/user";
    case CertScope::kDevice:
      return "google/chromeos/device";
  }
  NOTREACHED_IN_MIGRATION();
}

// "Static" flow:
// Checks all error-like fields of a client cert provisioning response.
// Extracts error and try_again_later fields from the |response| into
// |response_error| and |try_later|. Returns true if all error-like fields are
// empty or "ok" and the parsing of the |response| can be continued.
bool CheckCommonClientCertProvisioningResponse(
    const em::ClientCertificateProvisioningResponse& response,
    policy::DeviceManagementStatus status,
    std::optional<CertProvisioningResponseErrorType>& out_response_error,
    std::optional<int64_t>& out_try_later) {
  if (status != policy::DM_STATUS_SUCCESS) {
    return false;
  }

  if (response.has_error()) {
    out_response_error = response.error();
    return false;
  }

  if (response.has_try_again_later()) {
    out_try_later = response.try_again_later();
    return false;
  }

  return true;
}

// "Dynamic flow":
// Detects error-like cases that are common to all requests.
// Returns an `Error` struct if any error-like case has been detected,
// or `nullopt` otherwise.
std::optional<CertProvisioningClient::Error> HandleCommonErrorCases(
    policy::DeviceManagementStatus status,
    const em::ClientCertificateProvisioningResponse& response,
    ResponseCase expected_response_case) {
  if (status != policy::DM_STATUS_SUCCESS) {
    return CertProvisioningClient::Error{status, em::CertProvBackendError()};
  }

  if (response.has_backend_error()) {
    return CertProvisioningClient::Error{status, response.backend_error()};
  }

  if (response.response_case() != expected_response_case) {
    // Either no field or an unexpected field was set in the "response" oneof
    // field.
    return CertProvisioningClient::Error{
        policy::DM_STATUS_RESPONSE_DECODING_ERROR, em::CertProvBackendError()};
  }

  return std::nullopt;
}

std::vector<uint8_t> StrToBytes(const std::string& val) {
  return std::vector<uint8_t>(val.begin(), val.end());
}

}  // namespace

CertProvisioningClient::ProvisioningProcess::ProvisioningProcess(
    std::string process_id,
    CertScope cert_scope,
    std::string cert_profile_id,
    std::string policy_version,
    std::vector<uint8_t> public_key)
    : process_id(process_id),
      cert_scope(cert_scope),
      cert_profile_id(std::move(cert_profile_id)),
      policy_version(std::move(policy_version)),
      public_key(std::move(public_key)) {}
CertProvisioningClient::ProvisioningProcess::~ProvisioningProcess() = default;

CertProvisioningClient::ProvisioningProcess::ProvisioningProcess(
    ProvisioningProcess&& other) = default;

CertProvisioningClient::ProvisioningProcess&
CertProvisioningClient::ProvisioningProcess::operator=(
    ProvisioningProcess&& other) = default;

bool CertProvisioningClient::ProvisioningProcess::operator==(
    const ProvisioningProcess& other) const {
  static_assert(kFieldCount == 5, "Check/update operator==.");
  return process_id == other.process_id && cert_scope == other.cert_scope &&
         cert_profile_id == other.cert_profile_id &&
         policy_version == other.policy_version &&
         public_key == other.public_key;
}

CertProvisioningClientImpl::CertProvisioningClientImpl(
    policy::CloudPolicyClient& cloud_policy_client)
    : cloud_policy_client_(cloud_policy_client) {}

CertProvisioningClientImpl::~CertProvisioningClientImpl() = default;

void CertProvisioningClientImpl::Start(ProvisioningProcess provisioning_process,
                                       StartCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  // Sets the request type, no actual data is required.
  request.mutable_start_request();

  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(&CertProvisioningClientImpl::OnStartResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CertProvisioningClientImpl::GetNextInstruction(
    ProvisioningProcess provisioning_process,
    NextInstructionCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  // Sets the request type, no actual data is required.
  request.mutable_get_next_instruction_request();

  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(&CertProvisioningClientImpl::OnGetNextInstructionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CertProvisioningClientImpl::Authorize(
    ProvisioningProcess provisioning_process,
    std::string va_challenge_response,
    AuthorizeCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  request.mutable_authorize_request()->set_va_challenge_response(
      std::move(va_challenge_response));

  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(&CertProvisioningClientImpl::OnAuthorizeResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CertProvisioningClientImpl::UploadProofOfPossession(
    ProvisioningProcess provisioning_process,
    std::string signature,
    UploadProofOfPossessionCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  request.mutable_upload_proof_of_possession_request()->set_signature(
      std::move(signature));

  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(
          &CertProvisioningClientImpl::OnUploadProofOfPossessionResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CertProvisioningClientImpl::StartCsr(
    ProvisioningProcess provisioning_process,
    StartCsrCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  // Sets the request type, no actual data is required.
  request.mutable_start_csr_request();

  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(&CertProvisioningClientImpl::OnStartCsrResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CertProvisioningClientImpl::FinishCsr(
    ProvisioningProcess provisioning_process,
    std::string va_challenge_response,
    std::string signature,
    FinishCsrCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  em::FinishCsrRequest* finish_csr_request =
      request.mutable_finish_csr_request();
  if (!va_challenge_response.empty()) {
    finish_csr_request->set_va_challenge_response(
        std::move(va_challenge_response));
  }
  finish_csr_request->set_signature(std::move(signature));
  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(&CertProvisioningClientImpl::OnFinishCsrResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CertProvisioningClientImpl::DownloadCert(
    ProvisioningProcess provisioning_process,
    DownloadCertCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  // Sets the request type, no actual data is required.
  request.mutable_download_cert_request();

  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(&CertProvisioningClientImpl::OnDownloadCertResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CertProvisioningClientImpl::FillCommonRequestData(
    ProvisioningProcess provisioning_process,
    em::ClientCertificateProvisioningRequest& out_request) {
  static_assert(ProvisioningProcess::kFieldCount == 5,
                "Check/update this method.");
  out_request.set_certificate_provisioning_process_id(
      std::move(provisioning_process.process_id));
  out_request.set_certificate_scope(
      CertScopeToString(provisioning_process.cert_scope));
  out_request.set_cert_profile_id(
      std::move(provisioning_process.cert_profile_id));
  out_request.set_policy_version(
      std::move(provisioning_process.policy_version));
  out_request.set_public_key(provisioning_process.public_key.data(),
                             provisioning_process.public_key.size());
}

void CertProvisioningClientImpl::OnAuthorizeResponse(
    AuthorizeCallback callback,
    policy::DeviceManagementStatus status,
    const em::ClientCertificateProvisioningResponse& response) {
  if (std::optional<Error> error = HandleCommonErrorCases(
          status, response,
          /*expected_response_case=*/ResponseCase::kAuthorizeResponse)) {
    return std::move(callback).Run(base::unexpected(std::move(error).value()));
  }

  // Everything is ok, run |callback| with no error.
  return std::move(callback).Run({});
}

void CertProvisioningClientImpl::OnUploadProofOfPossessionResponse(
    UploadProofOfPossessionCallback callback,
    policy::DeviceManagementStatus status,
    const em::ClientCertificateProvisioningResponse& response) {
  if (std::optional<Error> error = HandleCommonErrorCases(
          status, response, /*expected_response_case=*/
          ResponseCase::kUploadProofOfPossessionResponse)) {
    return std::move(callback).Run(base::unexpected(std::move(error).value()));
  }

  // Everything is ok, run |callback| with no error.
  return std::move(callback).Run({});
}

void CertProvisioningClientImpl::OnStartResponse(
    StartCallback callback,
    policy::DeviceManagementStatus status,
    const em::ClientCertificateProvisioningResponse& response) {
  if (std::optional<Error> error = HandleCommonErrorCases(
          status, response,
          /*expected_response_case=*/ResponseCase::kStartResponse)) {
    return std::move(callback).Run(base::unexpected(std::move(error).value()));
  }

  // Everything is ok, run |callback| with data.
  return std::move(callback).Run(response.start_response());
}

void CertProvisioningClientImpl::OnGetNextInstructionResponse(
    NextInstructionCallback callback,
    policy::DeviceManagementStatus status,
    const em::ClientCertificateProvisioningResponse& response) {
  if (std::optional<Error> error =
          HandleCommonErrorCases(status, response, /*expected_response_case=*/
                                 ResponseCase::kGetNextInstructionResponse)) {
    return std::move(callback).Run(base::unexpected(std::move(error).value()));
  }

  // One of the oneof fields must be set.
  if (response.get_next_instruction_response().instruction_case() ==
      em::CertProvGetNextInstructionResponse::INSTRUCTION_NOT_SET) {
    return std::move(callback).Run(
        base::unexpected(Error{policy::DM_STATUS_RESPONSE_DECODING_ERROR,
                               em::CertProvBackendError()}));
  }

  // Everything is ok, run |callback| with data.
  return std::move(callback).Run(response.get_next_instruction_response());
}

void CertProvisioningClientImpl::OnStartCsrResponse(
    StartCsrCallback callback,
    policy::DeviceManagementStatus status,
    const em::ClientCertificateProvisioningResponse& response) {
  std::optional<CertProvisioningResponseErrorType> response_error;
  std::optional<int64_t> try_later;

  // Single step loop for convenience.
  do {
    if (!CheckCommonClientCertProvisioningResponse(response, status,
                                                   response_error, try_later)) {
      break;
    }

    if (!response.has_start_csr_response()) {
      status = policy::DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    const em::StartCsrResponse& start_csr_response =
        response.start_csr_response();

    if (!start_csr_response.has_hashing_algorithm() ||
        !start_csr_response.has_signing_algorithm() ||
        !start_csr_response.has_data_to_sign()) {
      status = policy::DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    if (start_csr_response.signing_algorithm() !=
        em::SigningAlgorithm::RSA_PKCS1_V1_5) {
      status = policy::DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    const std::string empty_str;

    const std::string& invalidation_topic =
        start_csr_response.has_invalidation_topic()
            ? start_csr_response.invalidation_topic()
            : empty_str;

    const std::string& va_challenge = start_csr_response.has_va_challenge()
                                          ? start_csr_response.va_challenge()
                                          : empty_str;

    // Everything is ok, run |callback| with data.
    return std::move(callback).Run(
        status, response_error, try_later, invalidation_topic, va_challenge,
        start_csr_response.hashing_algorithm(),
        StrToBytes(start_csr_response.data_to_sign()));
  } while (false);

  // Something went wrong. Return error via |status|, |response_error|,
  // |try_later|.
  const std::string empty_str;
  em::HashingAlgorithm hash_algo = {};
  return std::move(callback).Run(status, response_error, try_later, empty_str,
                                 empty_str, hash_algo, std::vector<uint8_t>());
}

void CertProvisioningClientImpl::OnFinishCsrResponse(
    FinishCsrCallback callback,
    policy::DeviceManagementStatus status,
    const em::ClientCertificateProvisioningResponse& response) {
  std::optional<CertProvisioningResponseErrorType> response_error;
  std::optional<int64_t> try_later;

  // Single step loop for convenience.
  do {
    if (!CheckCommonClientCertProvisioningResponse(response, status,
                                                   response_error, try_later)) {
      break;
    }

    if (!response.has_finish_csr_response()) {
      status = policy::DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }
  } while (false);

  std::move(callback).Run(status, response_error, try_later);
}

void CertProvisioningClientImpl::OnDownloadCertResponse(
    DownloadCertCallback callback,
    policy::DeviceManagementStatus status,
    const em::ClientCertificateProvisioningResponse& response) {
  std::optional<CertProvisioningResponseErrorType> response_error;
  std::optional<int64_t> try_later;

  // Single step loop for convenience.
  do {
    if (!CheckCommonClientCertProvisioningResponse(response, status,
                                                   response_error, try_later)) {
      break;
    }

    if (!response.has_download_cert_response()) {
      status = policy::DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    const em::DownloadCertResponse& download_cert_response =
        response.download_cert_response();

    if (!download_cert_response.has_pem_encoded_certificate()) {
      status = policy::DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    // Everything is ok, run |callback| with data.
    return std::move(callback).Run(
        status, response_error, try_later,
        download_cert_response.pem_encoded_certificate());
  } while (false);

  // Something went wrong. Return error via |status|, |response_error|,
  // |try_later|.
  return std::move(callback).Run(status, response_error, try_later,
                                 std::string());
}

}  // namespace ash::cert_provisioning
