// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_info.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_requests.h"
#include "chrome/browser/chromeos/certificate_provider/pin_dialog_manager.h"
#include "chrome/browser/chromeos/certificate_provider/sign_requests.h"
#include "chrome/browser/chromeos/certificate_provider/thread_safe_certificate_map.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_private_key.h"

namespace chromeos {

class CertificateProvider;

// A keyed service that manages registrations of extensions as certificate
// providers. It exposes all certificates that are provided by extensions
// through a |CertificateProvider| object that can be created using
// |CreateCertificateProvider()|. Private key handles are exposed through
// net::ClientKeyStore. Sign operations are routed to the extension that exposed
// the certificate.
//
// The typical order of execution is as follows:
//  1. HTTPS server requests client certs or
//     chrome.platformKeys.selectClientCertificates is called.
//  2. This starts the certificate request with ID x.
//  3. All extensions registered for the event onClientCertificatesRequested are
//     notified, the exposed callback is bound to request ID x.
//  4. Wait for all extensions to reply to request with ID x
//     or time out.
//  5. Filter all certificates from extensions that replied to request with ID x
//     and from the platform.
//  6. Show the selection dialog, user will select one.
//  7. Create private key handle. As this call is not associated with a specific
//     certificate request it looks at the certificate list obtained by the most
//     recent certificate request (execution of 3-5), which may or may not have
//     had ID x.
//  8. Sign() function of the key handle is called.
//  9. Forward the sign request to the extension that registered the
//     certificate. This request has a new sign request ID y.
// 10. Wait until the extension replies with the signature or fails the sign
//     request with ID y.
// 11. Forward the signature or failure as result of the key handle's Sign()
//     function.
class CertificateProviderService : public KeyedService {
 public:
  using CertificateInfo = certificate_provider::CertificateInfo;
  using CertificateInfoList = certificate_provider::CertificateInfoList;

  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Returns the ids of the extensions that want to provide certificates and
    // therefore want to be notified about certificate requests. This is called
    // once per client certificate request by the net layer.
    virtual std::vector<std::string> CertificateProviderExtensions() = 0;

    // Broadcasts a certificate request with |cert_request_id| to all
    // certificate provider extensions.
    virtual void BroadcastCertificateRequest(int cert_request_id) = 0;

    // Dispatches a sign request with the given arguments to the extension with
    // id |extension_id|. |algorithm| is a TLS 1.3 SignatureScheme value. See
    // net::SSLPrivateKey for details. Returns whether that extension is
    // actually a listener for that event.
    virtual bool DispatchSignRequestToExtension(
        const std::string& extension_id,
        int sign_request_id,
        uint16_t algorithm,
        const scoped_refptr<net::X509Certificate>& certificate,
        base::span<const uint8_t> digest) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when a sign request gets successfully completed.
    virtual void OnSignCompleted(
        const scoped_refptr<net::X509Certificate>& certificate) {}
  };

  // |SetDelegate| must be called exactly once directly after construction.
  CertificateProviderService();
  ~CertificateProviderService() override;

  // Must be called exactly once after construction and before other methods are
  // called. The delegate will be destroyed in the destructor of the service and
  // not before, which allows to unregister observers (e.g. for
  // OnExtensionUnloaded) in the delegate's destructor on behalf of the service.
  void SetDelegate(std::unique_ptr<Delegate> delegate);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Must be called with the reply of an extension to a previous certificate
  // request. For each request, it is expected that every registered extension
  // replies exactly once with the latest list of certificates.
  // |cert_request_id| must refer to a previously broadcast certificate request.
  // Returns false and ignores the call if the request id is unknown or it was
  // called before with the same combination of request id and extension id.
  // E.g. the request could have timed out before an extension replies.
  bool SetCertificatesProvidedByExtension(
      const std::string& extension_id,
      int cert_request_id,
      const CertificateInfoList& certificate_infos);

  // Must be called with the reply of an extension to a previous sign request.
  // |sign_request_id| is provided in the reply of the extension and must refer
  // to a previous sign request. The extension id must be provided, because
  // not the sign request id alone but only the pair (extension id, sign request
  // id) is unambiguous.
  // If the signature could be calculated by the extension, |signature| is
  // provided in the reply and should be the signature of the digest sent in the
  // sign request. Otherwise, in case of a failure, |signature| must be empty.
  // The call is ignored if |sign_request_id| is not referring to a pending
  // request.
  void ReplyToSignRequest(const std::string& extension_id,
                          int sign_request_id,
                          const std::vector<uint8_t>& signature);

  // Returns whether this certificate was provided by any extension during the
  // lifetime of this service. If this certificate is currently provided by an
  // extension, sets |is_currently_provided| to true and |extension_id| to that
  // extension's id. If this certificate was provided before but not anymore,
  // |is_currently_provided| will be set to false and |extension_id| will not be
  // modified.
  bool LookUpCertificate(const net::X509Certificate& cert,
                         bool* is_currently_provided,
                         std::string* extension_id);

  // Returns a CertificateProvider that always returns the latest list of
  // certificates that are provided by all registered extensions. Therefore, it
  // is sufficient to create the CertificateProvider once and then repeatedly
  // call its |GetCertificates()|. The returned provider is valid even after the
  // destruction of this service.
  std::unique_ptr<CertificateProvider> CreateCertificateProvider();

  // Must be called if extension with id |extension_id| is unloaded and cannot
  // serve certificates anymore. This should be called everytime the
  // corresponding notification of the ExtensionRegistry is triggered.
  void OnExtensionUnloaded(const std::string& extension_id);

  // Requests the extension which provided the certificate identified by
  // |subject_public_key_info| to sign |digest| with the corresponding private
  // key. |algorithm| is a TLS 1.3 SignatureScheme value. See net::SSLPrivateKey
  // for details. |callback| will be run with the reply of the extension or an
  // error.
  void RequestSignatureBySpki(
      const std::string& subject_public_key_info,
      uint16_t algorithm,
      base::span<const uint8_t> digest,
      const base::Optional<AccountId>& authenticating_user_account_id,
      net::SSLPrivateKey::SignCallback callback);

  // Looks up the certificate identified by |subject_public_key_info|. If any
  // extension is currently providing such a certificate, fills
  // *|supported_algorithms| with the algorithms supported for that certificate
  // and returns true. Values used for |supported_algorithms| are TLS 1.3
  // SignatureSchemes. See net::SSLPrivateKey for details. If no extension is
  // currently providing such a certificate, returns false.
  bool GetSupportedAlgorithmsBySpki(
      const std::string& subject_public_key_info,
      std::vector<uint16_t>* supported_algorithms);

  // Aborts all signature requests and related PIN dialogs that are associated
  // with the authentication of the given user.
  void AbortSignatureRequestsForAuthenticatingUser(
      const AccountId& authenticating_user_account_id);

  PinDialogManager* pin_dialog_manager() { return &pin_dialog_manager_; }

 private:
  class ClientCertIdentity;
  class CertificateProviderImpl;
  class SSLPrivateKey;

  // Requests the current list of certificates from every registered extension.
  // Once all extensions replied or a timeout was reached, the internal
  // |extension_to_certificates_| is updated and |callback| is run with the
  // retrieved list of certificates.
  void GetCertificatesFromExtensions(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback);

  // Copies the given certificates into the internal
  // |extension_to_certificates_|. Any previously stored certificates are
  // dropped. Afterwards, passes the list of given certificates to |callback|.
  void UpdateCertificatesAndRun(
      const std::map<std::string, CertificateInfoList>&
          extension_to_certificates,
      base::OnceCallback<void(net::ClientCertIdentityList)> callback);

  // Terminates the certificate request with id |cert_request_id| by ignoring
  // pending replies from extensions. Certificates that were already reported
  // are processed.
  void TerminateCertificateRequest(int cert_request_id);

  // Requests extension with |extension_id| to sign |digest| with the private
  // key certified by |certificate|. |algorithm| is a TLS 1.3 SignatureScheme
  // value. See net::SSLPrivateKey for details. |digest| was created by
  // |algorithm|'s prehash.  |callback| will be run with the reply of the
  // extension or an error.
  void RequestSignatureFromExtension(
      const std::string& extension_id,
      const scoped_refptr<net::X509Certificate>& certificate,
      uint16_t algorithm,
      base::span<const uint8_t> digest,
      const base::Optional<AccountId>& authenticating_user_account_id,
      net::SSLPrivateKey::SignCallback callback);

  std::unique_ptr<Delegate> delegate_;

  base::ObserverList<Observer> observers_;

  // The object to manage the dialog displayed when requestPin is called by the
  // extension.
  PinDialogManager pin_dialog_manager_;

  // State about all pending sign requests.
  certificate_provider::SignRequests sign_requests_;

  // Contains all pending certificate requests.
  certificate_provider::CertificateRequests certificate_requests_;

  // Contains all certificates that the extensions returned during the lifetime
  // of this service. Each certificate is associated with the extension that
  // reported the certificate in response to the most recent certificate
  // request. If a certificate was reported previously but in the most recent
  // responses, it is still cached but not loses it's association with any
  // extension. This ensures that a certificate can't magically appear as
  // platform certificate (e.g. in the client certificate selection dialog)
  // after an extension doesn't report it anymore.
  certificate_provider::ThreadSafeCertificateMap certificate_map_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CertificateProviderService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CertificateProviderService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_SERVICE_H_
