// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "net/base/net_errors.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace chromeos {

namespace {

void PostSignResult(net::SSLPrivateKey::SignCallback callback,
                    net::Error error,
                    const std::vector<uint8_t>& signature) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error, signature));
}

void PostIdentities(
    base::OnceCallback<void(net::ClientCertIdentityList)> callback,
    net::ClientCertIdentityList certs) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(certs)));
}

}  // namespace

class CertificateProviderService::CertificateProviderImpl
    : public CertificateProvider {
 public:
  // This provider must be used on the same thread as the
  // CertificateProviderService.
  explicit CertificateProviderImpl(
      const base::WeakPtr<CertificateProviderService>& service);
  ~CertificateProviderImpl() override;

  void GetCertificates(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback) override;

 private:

  const base::WeakPtr<CertificateProviderService> service_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CertificateProviderImpl);
};

// Implements an SSLPrivateKey backed by the signing function exposed by an
// extension through the certificateProvider API.
// Objects of this class must be used the CertificateProviderService's sequence.
class CertificateProviderService::SSLPrivateKey : public net::SSLPrivateKey {
 public:
  SSLPrivateKey(const std::string& extension_id,
                const CertificateInfo& cert_info,
                const base::WeakPtr<CertificateProviderService>& service);

  // net::SSLPrivateKey:
  std::string GetProviderName() override;
  std::vector<uint16_t> GetAlgorithmPreferences() override;
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override;

 private:
  ~SSLPrivateKey() override = default;

  const std::string extension_id_;
  const CertificateInfo cert_info_;
  const base::WeakPtr<CertificateProviderService> service_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SSLPrivateKey);
};

class CertificateProviderService::ClientCertIdentity
    : public net::ClientCertIdentity {
 public:
  ClientCertIdentity(scoped_refptr<net::X509Certificate> cert,
                     base::WeakPtr<CertificateProviderService> service)
      : net::ClientCertIdentity(std::move(cert)), service_(service) {}

  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  const base::WeakPtr<CertificateProviderService> service_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertIdentity);
};

void CertificateProviderService::ClientCertIdentity::AcquirePrivateKey(
    base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
        private_key_callback) {
  if (!service_) {
    std::move(private_key_callback).Run(nullptr);
    return;
  }

  bool is_currently_provided = false;
  CertificateInfo info;
  std::string extension_id;
  // TODO(mattm): can the ClientCertIdentity store a handle directly to the
  // extension instead of having to go through service_->certificate_map_ ?
  service_->certificate_map_.LookUpCertificate(
      *certificate(), &is_currently_provided, &info, &extension_id);
  if (!is_currently_provided) {
    std::move(private_key_callback).Run(nullptr);
    return;
  }

  std::move(private_key_callback)
      .Run(base::MakeRefCounted<SSLPrivateKey>(extension_id, info, service_));
}

CertificateProviderService::CertificateProviderImpl::CertificateProviderImpl(
    const base::WeakPtr<CertificateProviderService>& service)
    : service_(service) {}

CertificateProviderService::CertificateProviderImpl::
    ~CertificateProviderImpl() {}

void CertificateProviderService::CertificateProviderImpl::GetCertificates(
    base::OnceCallback<void(net::ClientCertIdentityList)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Avoid running the callback reentrantly.
  callback = base::BindOnce(&PostIdentities, std::move(callback));

  if (!service_) {
    std::move(callback).Run(net::ClientCertIdentityList());
    return;
  }
  service_->GetCertificatesFromExtensions(std::move(callback));
}

CertificateProviderService::SSLPrivateKey::SSLPrivateKey(
    const std::string& extension_id,
    const CertificateInfo& cert_info,
    const base::WeakPtr<CertificateProviderService>& service)
    : extension_id_(extension_id), cert_info_(cert_info), service_(service) {}

std::string CertificateProviderService::SSLPrivateKey::GetProviderName() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "extension \"" + extension_id_ + "\"";
}

std::vector<uint16_t>
CertificateProviderService::SSLPrivateKey::GetAlgorithmPreferences() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cert_info_.supported_algorithms;
}

void CertificateProviderService::SSLPrivateKey::Sign(
    uint16_t algorithm,
    base::span<const uint8_t> input,
    SignCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The Sign() method should not call its callback reentrantly, so wrap it in
  // PostSignResult().
  callback = base::BindOnce(&PostSignResult, std::move(callback));

  // The extension expects the input to be hashed ahead of time.
  const EVP_MD* md = SSL_get_signature_algorithm_digest(algorithm);
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (!md || !EVP_Digest(input.data(), input.size(), digest, &digest_len, md,
                         nullptr)) {
    std::move(callback).Run(net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
                            /*signature=*/{});
    return;
  }

  if (!service_) {
    std::move(callback).Run(net::ERR_FAILED, /*signature=*/{});
    return;
  }

  service_->RequestSignatureFromExtension(
      extension_id_, cert_info_.certificate, algorithm,
      base::make_span(digest, digest_len),
      /*authenticating_user_account_id=*/{}, std::move(callback));
}

CertificateProviderService::CertificateProviderService() {}

CertificateProviderService::~CertificateProviderService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CertificateProviderService::SetDelegate(
    std::unique_ptr<Delegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delegate_);
  DCHECK(delegate);

  delegate_ = std::move(delegate);
}

void CertificateProviderService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CertificateProviderService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool CertificateProviderService::SetCertificatesProvidedByExtension(
    const std::string& extension_id,
    int cert_request_id,
    const CertificateInfoList& certificate_infos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool completed = false;
  if (!certificate_requests_.SetCertificates(extension_id, cert_request_id,
                                             certificate_infos, &completed)) {
    DLOG(WARNING) << "Unexpected reply of extension " << extension_id
                  << " to request " << cert_request_id;
    return false;
  }
  if (completed) {
    std::map<std::string, CertificateInfoList> certificates;
    base::OnceCallback<void(net::ClientCertIdentityList)> callback;
    certificate_requests_.RemoveRequest(cert_request_id, &certificates,
                                        &callback);
    UpdateCertificatesAndRun(certificates, std::move(callback));
  }
  return true;
}

void CertificateProviderService::ReplyToSignRequest(
    const std::string& extension_id,
    int sign_request_id,
    const std::vector<uint8_t>& signature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<net::X509Certificate> certificate;
  net::SSLPrivateKey::SignCallback callback;
  if (!sign_requests_.RemoveRequest(extension_id, sign_request_id, &certificate,
                                    &callback)) {
    LOG(ERROR) << "request id unknown.";
    // The request was aborted before, or the extension replied multiple times
    // to the same request.
    return;
  }

  const net::Error error_code = signature.empty() ? net::ERR_FAILED : net::OK;
  std::move(callback).Run(error_code, signature);

  if (!signature.empty()) {
    for (auto& observer : observers_)
      observer.OnSignCompleted(certificate);
  }
}

bool CertificateProviderService::LookUpCertificate(
    const net::X509Certificate& cert,
    bool* has_extension,
    std::string* extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CertificateInfo unused_info;
  return certificate_map_.LookUpCertificate(cert, has_extension, &unused_info,
                                            extension_id);
}

std::unique_ptr<CertificateProvider>
CertificateProviderService::CreateCertificateProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<CertificateProviderImpl>(weak_factory_.GetWeakPtr());
}

void CertificateProviderService::OnExtensionUnloaded(
    const std::string& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const int cert_request_id :
       certificate_requests_.DropExtension(extension_id)) {
    std::map<std::string, CertificateInfoList> certificates;
    base::OnceCallback<void(net::ClientCertIdentityList)> callback;
    certificate_requests_.RemoveRequest(cert_request_id, &certificates,
                                        &callback);
    UpdateCertificatesAndRun(certificates, std::move(callback));
  }

  certificate_map_.RemoveExtension(extension_id);

  for (auto& callback : sign_requests_.RemoveAllRequests(extension_id))
    std::move(callback).Run(net::ERR_FAILED, std::vector<uint8_t>());

  pin_dialog_manager_.ExtensionUnloaded(extension_id);
}

void CertificateProviderService::RequestSignatureBySpki(
    const std::string& subject_public_key_info,
    uint16_t algorithm,
    base::span<const uint8_t> digest,
    const base::Optional<AccountId>& authenticating_user_account_id,
    net::SSLPrivateKey::SignCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_currently_provided = false;
  CertificateInfo info;
  std::string extension_id;
  certificate_map_.LookUpCertificateBySpki(
      subject_public_key_info, &is_currently_provided, &info, &extension_id);
  if (!is_currently_provided) {
    LOG(ERROR) << "no certificate with the specified spki was found";
    std::move(callback).Run(net::ERR_FAILED, std::vector<uint8_t>());
    return;
  }

  RequestSignatureFromExtension(extension_id, info.certificate, algorithm,
                                digest, authenticating_user_account_id,
                                std::move(callback));
}

bool CertificateProviderService::GetSupportedAlgorithmsBySpki(
    const std::string& subject_public_key_info,
    std::vector<uint16_t>* supported_algorithms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_currently_provided = false;
  CertificateInfo info;
  std::string extension_id;
  certificate_map_.LookUpCertificateBySpki(
      subject_public_key_info, &is_currently_provided, &info, &extension_id);
  if (!is_currently_provided) {
    LOG(ERROR) << "no certificate with the specified spki was found";
    return false;
  }
  *supported_algorithms = info.supported_algorithms;
  return true;
}

void CertificateProviderService::AbortSignatureRequestsForAuthenticatingUser(
    const AccountId& authenticating_user_account_id) {
  using ExtensionNameRequestIdPair =
      certificate_provider::SignRequests::ExtensionNameRequestIdPair;

  const std::vector<ExtensionNameRequestIdPair> sign_requests_to_abort =
      sign_requests_.FindRequestsForAuthenticatingUser(
          authenticating_user_account_id);

  for (const ExtensionNameRequestIdPair& sign_request :
       sign_requests_to_abort) {
    const std::string& extension_id = sign_request.first;
    const int sign_request_id = sign_request.second;
    pin_dialog_manager_.AbortSignRequest(extension_id, sign_request_id);

    scoped_refptr<net::X509Certificate> certificate;
    net::SSLPrivateKey::SignCallback sign_callback;
    if (sign_requests_.RemoveRequest(extension_id, sign_request_id,
                                     &certificate, &sign_callback)) {
      std::move(sign_callback).Run(net::ERR_FAILED, std::vector<uint8_t>());
    }
  }
}

void CertificateProviderService::GetCertificatesFromExtensions(
    base::OnceCallback<void(net::ClientCertIdentityList)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::vector<std::string> provider_extensions(
      delegate_->CertificateProviderExtensions());

  if (provider_extensions.empty()) {
    DVLOG(2) << "No provider extensions left, clear all certificates.";
    UpdateCertificatesAndRun(std::map<std::string, CertificateInfoList>(),
                             std::move(callback));
    return;
  }

  const int cert_request_id = certificate_requests_.AddRequest(
      provider_extensions, std::move(callback),
      base::BindOnce(&CertificateProviderService::TerminateCertificateRequest,
                     base::Unretained(this)));

  DVLOG(2) << "Start certificate request " << cert_request_id;
  delegate_->BroadcastCertificateRequest(cert_request_id);
}

void CertificateProviderService::UpdateCertificatesAndRun(
    const std::map<std::string, CertificateInfoList>& extension_to_certificates,
    base::OnceCallback<void(net::ClientCertIdentityList)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Extensions are removed from the service's state when they're unloaded.
  // Any remaining extension is assumed to be enabled.
  certificate_map_.Update(extension_to_certificates);

  net::ClientCertIdentityList all_certs;
  for (const auto& entry : extension_to_certificates) {
    for (const CertificateInfo& cert_info : entry.second)
      all_certs.push_back(std::make_unique<ClientCertIdentity>(
          cert_info.certificate, weak_factory_.GetWeakPtr()));
  }

  std::move(callback).Run(std::move(all_certs));
}

void CertificateProviderService::TerminateCertificateRequest(
    int cert_request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::map<std::string, CertificateInfoList> certificates;
  base::OnceCallback<void(net::ClientCertIdentityList)> callback;
  if (!certificate_requests_.RemoveRequest(cert_request_id, &certificates,
                                           &callback)) {
    DLOG(WARNING) << "Request id " << cert_request_id << " unknown.";
    return;
  }

  DVLOG(1) << "Time out certificate request " << cert_request_id;
  UpdateCertificatesAndRun(certificates, std::move(callback));
}

void CertificateProviderService::RequestSignatureFromExtension(
    const std::string& extension_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    uint16_t algorithm,
    base::span<const uint8_t> digest,
    const base::Optional<AccountId>& authenticating_user_account_id,
    net::SSLPrivateKey::SignCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int sign_request_id = sign_requests_.AddRequest(
      extension_id, certificate, authenticating_user_account_id,
      std::move(callback));
  pin_dialog_manager_.AddSignRequestId(extension_id, sign_request_id,
                                       authenticating_user_account_id);
  if (!delegate_->DispatchSignRequestToExtension(
          extension_id, sign_request_id, algorithm, certificate, digest)) {
    scoped_refptr<net::X509Certificate> local_certificate;
    sign_requests_.RemoveRequest(extension_id, sign_request_id,
                                 &local_certificate, &callback);
    std::move(callback).Run(net::ERR_FAILED, std::vector<uint8_t>());
  }
}

}  // namespace chromeos
