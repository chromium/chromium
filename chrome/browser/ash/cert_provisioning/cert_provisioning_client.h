// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_CLIENT_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace ash::cert_provisioning {

// Client for Certificate Provisioning requests interfacing with the device
// management server.
class CertProvisioningClient {
 public:
  virtual ~CertProvisioningClient() = default;

  // Information identifying a Certificate Provisioning Process.
  struct ProvisioningProcess {
    ProvisioningProcess(CertScope cert_scope,
                        std::string cert_profile_id,
                        std::string policy_version,
                        std::vector<uint8_t> public_key);
    ~ProvisioningProcess();

    ProvisioningProcess& operator=(const ProvisioningProcess& other) = delete;
    ProvisioningProcess(const ProvisioningProcess& other) = delete;

    ProvisioningProcess(ProvisioningProcess&& other);
    ProvisioningProcess& operator=(ProvisioningProcess&& other);

    bool operator==(const ProvisioningProcess& other) const;

    // If you add/remove fields, don't forget to adapt kFieldCount!
    CertScope cert_scope;
    std::string cert_profile_id;
    std::string policy_version;
    std::vector<uint8_t> public_key;

    // This must match the number of fields this struct has.
    // Used as a change detector to remind you to take a look e.g. at
    // operator==.
    static constexpr int kFieldCount = 4;
  };

  using CertProvNextActionResponse =
      enterprise_management::CertProvNextActionResponse;

  using StartCsrCallback = base::OnceCallback<void(
      policy::DeviceManagementStatus status,
      absl::optional<enterprise_management::
                         ClientCertificateProvisioningResponse::Error> error,
      absl::optional<int64_t> try_later,
      const std::string& invalidation_topic,
      const std::string& va_challenge,
      enterprise_management::HashingAlgorithm hash_algorithm,
      const std::string& data_to_sign)>;

  using FinishCsrCallback = base::OnceCallback<void(
      policy::DeviceManagementStatus status,
      absl::optional<enterprise_management::
                         ClientCertificateProvisioningResponse::Error> error,
      absl::optional<int64_t> try_later)>;

  using DownloadCertCallback = base::OnceCallback<void(
      policy::DeviceManagementStatus status,
      absl::optional<enterprise_management::
                         ClientCertificateProvisioningResponse::Error> error,
      absl::optional<int64_t> try_later,
      const std::string& pem_encoded_certificate)>;

  using NextActionCallback = base::OnceCallback<void(
      policy::DeviceManagementStatus status,
      absl::optional<enterprise_management::
                         ClientCertificateProvisioningResponse::Error> error,
      const CertProvNextActionResponse& next_action_response)>;

  virtual void StartOrContinue(ProvisioningProcess provisioning_process,
                               NextActionCallback callback) = 0;

  virtual void Authorize(ProvisioningProcess provisioning_process,
                         std::string va_challenge_response,
                         NextActionCallback callback) = 0;

  virtual void UploadProofOfPossession(ProvisioningProcess provisioning_process,
                                       std::string signature,
                                       NextActionCallback callback) = 0;

  // Sends certificate provisioning start csr request. It is Step 1 in the
  // certificate provisioning flow. |cert_scope| defines if it is a user- or
  // device-level request, |cert_profile_id| defines for which profile from
  // policies the request applies, |public_key| is used to build the CSR.
  // |callback| will be called when the operation completes. It is expected to
  // receive the CSR and VA challenge.
  virtual void StartCsr(ProvisioningProcess provisioning_process,
                        StartCsrCallback callback) = 0;

  // Sends certificate provisioning finish csr request. It is Step 2 in the
  // certificate provisioning flow. |cert_scope| defines if it is a user- or
  // device-level request, |cert_profile_id| and |public_key| define the
  // provisioning flow that should be continued. |va_challenge_response| is a
  // challenge response to the challenge from the previous step. |signature| is
  // cryptographic signature of the CSR from the previous step, the algorithm
  // for it is defined in a corresponding certificate profile. |callback| will
  // be called when the operation completes. It is expected to receive a
  // confirmation that the request is accepted.
  virtual void FinishCsr(ProvisioningProcess provisioning_process,
                         std::string va_challenge_response,
                         std::string signature,
                         FinishCsrCallback callback) = 0;

  // Sends certificate provisioning download certificate request. It is Step 3
  // (final) in the certificate provisioning flow. |cert_scope|,
  // |cert_profile_id|, |public_key| are the same as for finish csr request.
  // |callback| will be called when the operation completes. It is expected to
  // receive a certificate that was issued according to the CSR that was
  // generated during previous steps.
  virtual void DownloadCert(ProvisioningProcess provisioning_process,
                            DownloadCertCallback callback) = 0;
};

class CertProvisioningClientImpl : public CertProvisioningClient {
 public:
  // The caller must ensure that `cloud_policy_client` remains valid as long as
  // this instance exists.
  explicit CertProvisioningClientImpl(
      policy::CloudPolicyClient& cloud_policy_client);
  CertProvisioningClientImpl(const CertProvisioningClientImpl&) = delete;
  CertProvisioningClientImpl& operator=(const CertProvisioningClientImpl&) =
      delete;
  ~CertProvisioningClientImpl() override;

  void StartOrContinue(ProvisioningProcess provisioning_process,
                       NextActionCallback callback) override;

  void Authorize(ProvisioningProcess provisioning_process,
                 std::string va_challenge_response,
                 NextActionCallback callback) override;

  void UploadProofOfPossession(ProvisioningProcess provisioning_process,
                               std::string signature,
                               NextActionCallback callback) override;

  void StartCsr(ProvisioningProcess provisioning_process,
                StartCsrCallback callback) override;

  void FinishCsr(ProvisioningProcess provisioning_process,
                 std::string va_challenge_response,
                 std::string signature,
                 FinishCsrCallback callback) override;

  void DownloadCert(ProvisioningProcess provisioning_process,
                    DownloadCertCallback callback) override;

 private:
  void FillCommonRequestData(
      ProvisioningProcess provisioning_process,
      enterprise_management::ClientCertificateProvisioningRequest& out_request);

  void OnNextActionResponse(
      NextActionCallback callback,
      policy::DeviceManagementStatus status,
      const enterprise_management::ClientCertificateProvisioningResponse&
          response);

  void OnStartCsrResponse(
      StartCsrCallback callback,
      policy::DeviceManagementStatus status,
      const enterprise_management::ClientCertificateProvisioningResponse&
          response);
  void OnFinishCsrResponse(
      FinishCsrCallback callback,
      policy::DeviceManagementStatus status,
      const enterprise_management::ClientCertificateProvisioningResponse&
          response);
  void OnDownloadCertResponse(
      DownloadCertCallback callback,
      policy::DeviceManagementStatus status,
      const enterprise_management::ClientCertificateProvisioningResponse&
          response);

  // Unowned.
  const raw_ref<policy::CloudPolicyClient> cloud_policy_client_;

  base::WeakPtrFactory<CertProvisioningClientImpl> weak_ptr_factory_{this};
};

}  // namespace ash::cert_provisioning

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_CLIENT_H_
