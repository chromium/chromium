// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_provider/certificate_provider_service.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "extensions/common/extension_id.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // IS_CHROMEOS_LACROS

namespace chromeos {

namespace {

void PostSignResult(net::SSLPrivateKey::SignCallback callback,
                    net::Error error,
                    const std::vector<uint8_t>& signature) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error, signature));
}

void PostIdentities(
    base::OnceCallback<void(net::ClientCertIdentityList)> callback,
    net::ClientCertIdentityList certs) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

  CertificateProviderImpl(const CertificateProviderImpl&) = delete;
  CertificateProviderImpl& operator=(const CertificateProviderImpl&) = delete;

  ~CertificateProviderImpl() override;

  void GetCertificates(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback) override;

 private:
  const base::WeakPtr<CertificateProviderService> service_;
  SEQUENCE_CHECKER(sequence_checker_);
};

// Implements an SSLPrivateKey backed by the signing function exposed by an
// extension through the certificateProvider API.
// Objects of this class must be used the CertificateProviderService's sequence.
class CertificateProviderService::SSLPrivateKey : public net::SSLPrivateKey {
 public:
  SSLPrivateKey(const std::string& extension_id,
                const CertificateInfo& cert_info,
                const base::WeakPtr<CertificateProviderService>& service);

  SSLPrivateKey(const SSLPrivateKey&) = delete;
  SSLPrivateKey& operator=(const SSLPrivateKey&) = delete;

  // net::SSLPrivateKey:
  std::string GetProviderName() override;
  std::vector<uint16_t> GetAlgorithmPreferences() override;
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override;

 private:
  ~SSLPrivateKey() override;

  const extensions::ExtensionId extension_id_;
  const CertificateInfo cert_info_;
  const base::WeakPtr<CertificateProviderService> service_;
  SEQUENCE_CHECKER(sequence_checker_);
};

class CertificateProviderService::ClientCertIdentity
    : public net::ClientCertIdentity {
 public:
  ClientCertIdentity(scoped_refptr<net::X509Certificate> cert,
                     base::WeakPtr<CertificateProviderService> service)
      : net::ClientCertIdentity(std::move(cert)), service_(service) {}

  ClientCertIdentity(const ClientCertIdentity&) = delete;
  ClientCertIdentity& operator=(const ClientCertIdentity&) = delete;

  ~ClientCertIdentity() override;

  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  const base::WeakPtr<CertificateProviderService> service_;
};

CertificateProviderService::ClientCertIdentity::~ClientCertIdentity() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CertificateProviderService::ClientCertIdentity::AcquirePrivateKey(
    base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
        private_key_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
    ~CertificateProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

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

  if (!service_) {
    std::move(callback).Run(net::ERR_FAILED, /*signature=*/{});
    return;
  }

  service_->RequestSignatureFromExtension(
      extension_id_, cert_info_.certificate, algorithm, input,
      /*authenticating_user_account_id=*/{}, std::move(callback));
}

CertificateProviderService::SSLPrivateKey::~SSLPrivateKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void CertificateProviderService::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void CertificateProviderService::SetCertificatesProvidedByExtension(
    const std::string& extension_id,
    const CertificateInfoList& certificate_infos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Synchronize with Ash-Chrome
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service && service->IsAvailable<crosapi::mojom::CertDatabase>() &&
      service->GetInterfaceVersion<crosapi::mojom::CertDatabase>() >=
          static_cast<int>(crosapi::mojom::CertDatabase::MethodMinVersions::
                               kSetCertsProvidedByExtensionMinVersion)) {
    service->GetRemote<crosapi::mojom::CertDatabase>()
        ->SetCertsProvidedByExtension(extension_id, certificate_infos);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  certificate_map_.UpdateCertificatesForExtension(extension_id,
                                                  certificate_infos);
  for (auto& observer : observers_)
    observer.OnCertificatesUpdated(extension_id, certificate_infos);
}

bool CertificateProviderService::SetExtensionCertificateReplyReceived(
    const std::string& extension_id,
    int cert_request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool completed = false;
  if (!certificate_requests_.SetExtensionReplyReceived(
          extension_id, cert_request_id, &completed)) {
    DLOG(WARNING) << "Unexpected reply of extension " << extension_id
                  << " to request " << cert_request_id;
    return false;
  }
  if (completed) {
    base::OnceCallback<void(net::ClientCertIdentityList)> callback;
    certificate_requests_.RemoveRequest(cert_request_id, &callback);
    CollectCertificatesAndRun(std::move(callback));
  }
  return true;
}

bool CertificateProviderService::ReplyToSignRequest(
    const std::string& extension_id,
    int sign_request_id,
    const std::vector<uint8_t>& signature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
  LOG(WARNING) << "Extension " << extension_id
               << " replied to signature request " << sign_request_id
               << ", size " << signature.size();

  scoped_refptr<net::X509Certificate> certificate;
  net::SSLPrivateKey::SignCallback callback;
  if (!sign_requests_.RemoveRequest(extension_id, sign_request_id, &certificate,
                                    &callback)) {
    return false;
  }
  pin_dialog_manager_.RemoveSignRequest(extension_id, sign_request_id);

  const net::Error error_code = signature.empty() ? net::ERR_FAILED : net::OK;
  std::move(callback).Run(error_code, signature);

  if (!signature.empty()) {
    for (auto& observer : observers_)
      observer.OnSignCompleted(certificate, extension_id);
  }
  return true;
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

void CertificateProviderService::OnExtensionUnregistered(
    const std::string& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  certificate_map_.RemoveExtension(extension_id);
}

void CertificateProviderService::OnExtensionUnloaded(
    const std::string& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  certificate_map_.RemoveExtension(extension_id);

  for (const int completed_cert_request_id :
       certificate_requests_.DropExtension(extension_id)) {
    base::OnceCallback<void(net::ClientCertIdentityList)> callback;
    certificate_requests_.RemoveRequest(completed_cert_request_id, &callback);
    CollectCertificatesAndRun(std::move(callback));
  }

  for (auto& callback : sign_requests_.RemoveAllRequests(extension_id))
    std::move(callback).Run(net::ERR_FAILED, std::vector<uint8_t>());

  pin_dialog_manager_.ExtensionUnloaded(extension_id);
}

void CertificateProviderService::RequestSignatureBySpki(
    const std::string& subject_public_key_info,
    uint16_t algorithm,
    base::span<const uint8_t> input,
    const std::optional<AccountId>& authenticating_user_account_id,
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
                                input, authenticating_user_account_id,
                                std::move(callback));
}

bool CertificateProviderService::LookUpSpki(
    const std::string& subject_public_key_info,
    std::vector<uint16_t>* supported_algorithms,
    std::string* extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_currently_provided = false;
  CertificateInfo info;
  certificate_map_.LookUpCertificateBySpki(
      subject_public_key_info, &is_currently_provided, &info, extension_id);
  if (!is_currently_provided) {
    LOG(ERROR) << "no certificate with the specified spki was found";
    return false;
  }
  *supported_algorithms = info.supported_algorithms;
  return true;
}

void CertificateProviderService::AbortSignatureRequestsForAuthenticatingUser(
    const AccountId& authenticating_user_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  using ExtensionNameRequestIdPair =
      certificate_provider::SignRequests::ExtensionNameRequestIdPair;

  const std::vector<ExtensionNameRequestIdPair> sign_requests_to_abort =
      sign_requests_.FindRequestsForAuthenticatingUser(
          authenticating_user_account_id);

  for (const ExtensionNameRequestIdPair& sign_request :
       sign_requests_to_abort) {
    const std::string& extension_id = sign_request.first;
    const int sign_request_id = sign_request.second;

    // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
    LOG(WARNING) << "Aborting user login signature request from extension "
                 << extension_id << " id " << sign_request_id;

    pin_dialog_manager_.RemoveSignRequest(extension_id, sign_request_id);

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
    DVLOG(2) << "No provider extensions left.";
    // Note that there could still be unfinished requests to extensions that
    // were previously registered.
    CollectCertificatesAndRun(std::move(callback));
    return;
  }

  const int cert_request_id = certificate_requests_.AddRequest(
      provider_extensions, std::move(callback),
      base::BindOnce(&CertificateProviderService::TerminateCertificateRequest,
                     base::Unretained(this)));

  DVLOG(2) << "Start certificate request " << cert_request_id;
  delegate_->BroadcastCertificateRequest(cert_request_id);
}

void CertificateProviderService::CollectCertificatesAndRun(
    base::OnceCallback<void(net::ClientCertIdentityList)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net::ClientCertIdentityList client_cert_identity_list;
  std::vector<scoped_refptr<net::X509Certificate>> certificates =
      certificate_map_.GetCertificates();
  for (const scoped_refptr<net::X509Certificate>& certificate : certificates) {
    client_cert_identity_list.push_back(std::make_unique<ClientCertIdentity>(
        certificate, weak_factory_.GetWeakPtr()));
  }

  std::move(callback).Run(std::move(client_cert_identity_list));
}

void CertificateProviderService::TerminateCertificateRequest(
    int cert_request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::OnceCallback<void(net::ClientCertIdentityList)> callback;
  if (!certificate_requests_.RemoveRequest(cert_request_id, &callback)) {
    DLOG(WARNING) << "Request id " << cert_request_id << " unknown.";
    return;
  }

  DVLOG(1) << "Time out certificate request " << cert_request_id;
  CollectCertificatesAndRun(std::move(callback));
}

void CertificateProviderService::RequestSignatureFromExtension(
    const std::string& extension_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    uint16_t algorithm,
    base::span<const uint8_t> input,
    const std::optional<AccountId>& authenticating_user_account_id,
    net::SSLPrivateKey::SignCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int sign_request_id = sign_requests_.AddRequest(
      extension_id, certificate, authenticating_user_account_id,
      std::move(callback));

  // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
  LOG(WARNING) << "Starting signature request to extension " << extension_id
               << " id " << sign_request_id;

  pin_dialog_manager_.AddSignRequestId(extension_id, sign_request_id,
                                       authenticating_user_account_id);
  if (!delegate_->DispatchSignRequestToExtension(
          extension_id, sign_request_id, algorithm, certificate, input)) {
    // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
    LOG(WARNING) << "Failed to dispatch signature request to extension "
                 << extension_id << " id " << sign_request_id;
    scoped_refptr<net::X509Certificate> local_certificate;
    sign_requests_.RemoveRequest(extension_id, sign_request_id,
                                 &local_certificate, &callback);
    pin_dialog_manager_.RemoveSignRequest(extension_id, sign_request_id);
    std::move(callback).Run(net::ERR_FAILED, std::vector<uint8_t>());
  }
}

}  // namespace chromeos
