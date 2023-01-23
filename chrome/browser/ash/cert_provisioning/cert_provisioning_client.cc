// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::cert_provisioning {

namespace em = enterprise_management;

// The type for variables containing an error from DM Server response.
using CertProvisioningResponseErrorType =
    enterprise_management::ClientCertificateProvisioningResponse::Error;

namespace {

// Returns the device management protocol string representation of a CertScope.
std::string CertScopeToString(CertScope cert_scope) {
  switch (cert_scope) {
    case CertScope::kUser:
      return "google/chromeos/user";
    case CertScope::kDevice:
      return "google/chromeos/device";
  }
  NOTREACHED();
}

// Checks all error-like fields of a client cert provisioning response.
// Extracts error and try_again_later fields from the |response| into
// |response_error| and |try_later|. Returns true if all error-like fields are
// empty or "ok" and the parsing of the |response| can be continued.
bool CheckCommonClientCertProvisioningResponse(
    const em::ClientCertificateProvisioningResponse& response,
    policy::DeviceManagementStatus status,
    absl::optional<CertProvisioningResponseErrorType>& out_response_error,
    absl::optional<int64_t>& out_try_later) {
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

}  // namespace

CertProvisioningClient::ProvisioningProcess::ProvisioningProcess(
    CertScope cert_scope,
    std::string cert_profile_id,
    std::string policy_version,
    std::vector<uint8_t> public_key)
    : cert_scope(cert_scope),
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
  static_assert(kFieldCount == 4, "Check/update operator==.");
  return cert_scope == other.cert_scope &&
         cert_profile_id == other.cert_profile_id &&
         policy_version == other.policy_version &&
         public_key == other.public_key;
}

CertProvisioningClientImpl::CertProvisioningClientImpl(
    policy::CloudPolicyClient& cloud_policy_client)
    : cloud_policy_client_(cloud_policy_client) {}

CertProvisioningClientImpl::~CertProvisioningClientImpl() = default;

void CertProvisioningClientImpl::StartOrContinue(
    ProvisioningProcess provisioning_process,
    NextActionCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  // Sets the request type, no actual data is required.
  request.mutable_start_or_continue_request();

  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(&CertProvisioningClientImpl::OnNextActionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CertProvisioningClientImpl::Authorize(
    ProvisioningProcess provisioning_process,
    std::string va_challenge_response,
    NextActionCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  request.mutable_authorize_request()->set_va_challenge_response(
      std::move(va_challenge_response));

  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(&CertProvisioningClientImpl::OnNextActionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CertProvisioningClientImpl::UploadProofOfPossession(
    ProvisioningProcess provisioning_process,
    std::string signature,
    NextActionCallback callback) {
  em::ClientCertificateProvisioningRequest request;
  FillCommonRequestData(std::move(provisioning_process), request);

  request.mutable_upload_proof_of_possession_request()->set_signature(
      std::move(signature));

  cloud_policy_client_->ClientCertProvisioningRequest(
      std::move(request),
      base::BindOnce(&CertProvisioningClientImpl::OnNextActionResponse,
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
    enterprise_management::ClientCertificateProvisioningRequest& out_request) {
  out_request.set_certificate_scope(
      CertScopeToString(provisioning_process.cert_scope));
  out_request.set_cert_profile_id(
      std::move(provisioning_process.cert_profile_id));
  out_request.set_policy_version(
      std::move(provisioning_process.policy_version));
  out_request.set_public_key(provisioning_process.public_key.data(),
                             provisioning_process.public_key.size());
}

void CertProvisioningClientImpl::OnNextActionResponse(
    NextActionCallback callback,
    policy::DeviceManagementStatus status,
    const em::ClientCertificateProvisioningResponse& response) {
  absl::optional<CertProvisioningResponseErrorType> response_error;

  // Single step loop for convenience.
  do {
    if (status != policy::DM_STATUS_SUCCESS) {
      break;
    }

    if (response.has_error()) {
      response_error = response.error();
      break;
    }

    if (!response.has_next_action_response()) {
      status = policy::DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    // One of the oneof fields must be set.
    if (response.next_action_response().instruction_case() ==
        em::CertProvNextActionResponse::INSTRUCTION_NOT_SET) {
      status = policy::DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    // Everything is ok, run |callback| with data.
    std::move(callback).Run(status, response_error,
                            response.next_action_response());
    return;
  } while (false);

  // Something went wrong. Return error via |status|, |response_error|.
  std::move(callback).Run(status, response_error, CertProvNextActionResponse());
}

void CertProvisioningClientImpl::OnStartCsrResponse(
    StartCsrCallback callback,
    policy::DeviceManagementStatus status,
    const enterprise_management::ClientCertificateProvisioningResponse&
        response) {
  absl::optional<CertProvisioningResponseErrorType> response_error;
  absl::optional<int64_t> try_later;

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
    return std::move(callback).Run(status, response_error, try_later,
                                   invalidation_topic, va_challenge,
                                   start_csr_response.hashing_algorithm(),
                                   start_csr_response.data_to_sign());
  } while (false);

  // Something went wrong. Return error via |status|, |response_error|,
  // |try_later|.
  const std::string empty_str;
  em::HashingAlgorithm hash_algo = {};
  return std::move(callback).Run(status, response_error, try_later, empty_str,
                                 empty_str, hash_algo, empty_str);
}

void CertProvisioningClientImpl::OnFinishCsrResponse(
    FinishCsrCallback callback,
    policy::DeviceManagementStatus status,
    const enterprise_management::ClientCertificateProvisioningResponse&
        response) {
  absl::optional<CertProvisioningResponseErrorType> response_error;
  absl::optional<int64_t> try_later;

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
    const enterprise_management::ClientCertificateProvisioningResponse&
        response) {
  absl::optional<CertProvisioningResponseErrorType> response_error;
  absl::optional<int64_t> try_later;

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
