// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class StateStore;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}

namespace policy {
class PolicyService;
}

namespace chromeos {

class PlatformKeysService : public KeyedService {
 public:
  // The SelectDelegate is used to select a single certificate from all
  // certificates matching a request (see SelectClientCertificates). E.g. this
  // can happen by exposing UI to let the user select.
  class SelectDelegate {
   public:
    using CertificateSelectedCallback = base::Callback<void(
        const scoped_refptr<net::X509Certificate>& selection)>;

    SelectDelegate();
    virtual ~SelectDelegate();

    // Called on an interactive SelectClientCertificates call with the list of
    // matching certificates, |certs|.
    // The certificate passed to |callback| will be forwarded to the
    // calling extension and the extension will get unlimited sign permission
    // for this cert. By passing null to |callback|, no cert will be selected.
    // Must eventually call |callback| or be destructed. |callback| must not be
    // called after this delegate is destructed.
    // |web_contents| and |context| provide the context in which the
    // certificates were requested and are not null.
    virtual void Select(const std::string& extension_id,
                        const net::CertificateList& certs,
                        const CertificateSelectedCallback& callback,
                        content::WebContents* web_contents,
                        content::BrowserContext* context) = 0;

   private:
    DISALLOW_ASSIGN(SelectDelegate);
  };

  // Stores registration information in |state_store|, i.e. for each extension
  // the list of public keys that are valid to be used for signing. See
  // |KeyPermissions| for details.
  // |browser_context| and |state_store| must not be null and outlive this
  // object.
  explicit PlatformKeysService(bool profile_is_managed,
                               PrefService* profile_prefs,
                               policy::PolicyService* profile_policies,
                               content::BrowserContext* browser_context,
                               extensions::StateStore* state_store);

  ~PlatformKeysService() override;

  // Sets the delegate which will be used for interactive
  // SelectClientCertificates calls.
  void SetSelectDelegate(std::unique_ptr<SelectDelegate> delegate);

  // If the generation was successful, |public_key_spki_der| will contain the
  // DER encoding of the SubjectPublicKeyInfo of the generated key and
  // |error_message| will be empty. If it failed, |public_key_spki_der| will be
  // empty and |error_message| contain an error message.
  using GenerateKeyCallback =
      base::Callback<void(const std::string& public_key_spki_der,
                          const std::string& error_message)>;

  // Generates an RSA key pair with |modulus_length_bits| and registers the key
  // to allow a single sign operation by the given extension. |token_id|
  // specifies the token to store the keypair on. |callback| will be invoked
  // with the resulting public key or an error. Will only call back during the
  // lifetime of this object.
  void GenerateRSAKey(const std::string& token_id,
                      unsigned int modulus_length_bits,
                      const std::string& extension_id,
                      const GenerateKeyCallback& callback);

  // If signing was successful, |signature| will be contain the signature and
  // |error_message| will be empty. If it failed, |signature| will be empty and
  // |error_message| contain an error message.
  using SignCallback = base::Callback<void(const std::string& signature,
                                           const std::string& error_message)>;

  // Digests |data|, applies PKCS1 padding and afterwards signs the data with
  // the private key matching |public_key_spki_der|. If a non empty token id is
  // provided and the key is not found in that token, the operation aborts. If
  // the extension does not have permissions for signing with this key, the
  // operation aborts. In case of a one time permission (granted after
  // generating the key), this function also removes the permission to prevent
  // future signing attempts.
  // |callback| will be invoked with the signature or an error message.
  // Will only call back during the lifetime of this object.
  void SignRSAPKCS1Digest(const std::string& token_id,
                          const std::string& data,
                          const std::string& public_key_spki_der,
                          platform_keys::HashAlgorithm hash_algorithm,
                          const std::string& extension_id,
                          const SignCallback& callback);

  // Applies PKCS1 padding and afterwards signs the data with the private key
  // matching |public_key_spki_der|. |data| is not digested. If a non empty
  // token id is provided and the key is not found in that token, the operation
  // aborts.
  // The size of |data| (number of octets) must be smaller than k - 11, where k
  // is the key size in octets.
  // If the extension does not have permissions for signing with this key, the
  // operation aborts. In case of a one time permission (granted after
  // generating the key), this function also removes the permission to prevent
  // future signing attempts.
  // |callback| will be invoked with the signature or an error message.
  // Will only call back during the lifetime of this object.
  void SignRSAPKCS1Raw(const std::string& token_id,
                       const std::string& data,
                       const std::string& public_key_spki_der,
                       const std::string& extension_id,
                       const SignCallback& callback);

  // If the certificate request could be processed successfully, |matches| will
  // contain the list of matching certificates (maybe empty) and |error_message|
  // will be empty. If an error occurred, |matches| will be null and
  // |error_message| contain an error message.
  using SelectCertificatesCallback =
      base::Callback<void(std::unique_ptr<net::CertificateList> matches,
                          const std::string& error_message)>;

  // Returns a list of certificates matching |request|.
  // 1) all certificates that match the request (like being rooted in one of the
  // give CAs) are determined.
  // 2) if |client_certificates| is not null, drops all certificates that are
  // not elements of |client_certificates|,
  // 3) if |interactive| is true, the currently set SelectDelegate is used to
  // select a single certificate from these matches
  // which will the extension will also be granted access to.
  // 4) only certificates, that the extension has unlimited sign permission for,
  // will be returned.
  // |callback| will be invoked with these certificates or an error message.
  // Will only call back during the lifetime of this object. |web_contents| must
  // not be null.
  void SelectClientCertificates(
      const platform_keys::ClientCertificateRequest& request,
      std::unique_ptr<net::CertificateList> client_certificates,
      bool interactive,
      const std::string& extension_id,
      const SelectCertificatesCallback& callback,
      content::WebContents* web_contents);

 private:
  class GenerateRSAKeyTask;
  class SelectTask;
  class SignTask;
  class Task;

  // Starts |task| eventually. To ensure that at most one |Task| is running at a
  // time, it queues |task| for later execution if necessary.
  void StartOrQueueTask(std::unique_ptr<Task> task);

  // Must be called after |task| is done. |task| will be invalid after this
  // call. This must not be called for any but the task that ran last. If any
  // other tasks are queued (see StartOrQueueTask()), it will start the next
  // one.
  void TaskFinished(Task* task);

  // Callback used by |GenerateRSAKey|.
  // If the key generation was successful, registers the generated public key
  // for the given extension. If any error occurs during key generation or
  // registration, calls |callback| with an error. Otherwise, on success, calls
  // |callback| with the public key.
  void GeneratedKey(const std::string& extension_id,
                    const GenerateKeyCallback& callback,
                    const std::string& public_key_spki_der,
                    const std::string& error_message);

  content::BrowserContext* browser_context_;
  KeyPermissions key_permissions_;
  std::unique_ptr<SelectDelegate> select_delegate_;
  base::queue<std::unique_ptr<Task>> tasks_;
  base::WeakPtrFactory<PlatformKeysService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PlatformKeysService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_
