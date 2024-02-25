// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}  // namespace net

namespace chromeos {

class ExtensionPlatformKeysService : public KeyedService {
 public:
  // The SelectDelegate is used to select a single certificate from all
  // certificates matching a request (see SelectClientCertificates). E.g. this
  // can happen by exposing UI to let the user select.
  class SelectDelegate {
   public:
    using CertificateSelectedCallback =
        base::OnceCallback<void(scoped_refptr<net::X509Certificate> selection)>;

    SelectDelegate();
    SelectDelegate(const SelectDelegate&) = delete;
    auto operator=(const SelectDelegate&) = delete;
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
                        CertificateSelectedCallback callback,
                        content::WebContents* web_contents,
                        content::BrowserContext* context) = 0;
  };

  // |browser_context| must not be null and must outlive this object.
  explicit ExtensionPlatformKeysService(
      content::BrowserContext* browser_context);

  ExtensionPlatformKeysService(const ExtensionPlatformKeysService&) = delete;
  auto operator=(const ExtensionPlatformKeysService&) = delete;

  ~ExtensionPlatformKeysService() override;

  // Sets the delegate which will be used for interactive
  // SelectClientCertificates calls.
  void SetSelectDelegate(std::unique_ptr<SelectDelegate> delegate);

  // If the generation was successful, |public_key_spki_der| will contain the
  // DER encoding of the SubjectPublicKeyInfo of the generated key. If it
  // failed, |public_key_spki_der| will be empty.
  using GenerateKeyCallback = base::OnceCallback<void(
      std::vector<uint8_t> public_key_spki_der,
      std::optional<crosapi::mojom::KeystoreError> error)>;

  // Generates an RSA key pair with |modulus_length_bits| and registers the key
  // to allow a single sign operation by the given extension. |token_id|
  // specifies the token to store the key pair on. If |sw_backed| is true, the
  // generated RSA key pair will be software-backed. If the generation was
  // successful, |callback| will be invoked with the resulting public key. If it
  // failed, the resulting public key will be empty. Will only call back during
  // the lifetime of this object.
  void GenerateRSAKey(platform_keys::TokenId token_id,
                      unsigned int modulus_length_bits,
                      bool sw_backed,
                      std::string extension_id,
                      GenerateKeyCallback callback);

  // Generates an EC key pair with |named_curve| and registers the key to allow
  // a single sign operation by the given extension. |token_id| specifies the
  // token to store the key pair on. If the generation was successful,
  // |callback| will be invoked with the resulting public key. If it failed, the
  // resulting public key will be empty. Will only call back during the lifetime
  // of this object.
  void GenerateECKey(platform_keys::TokenId token_id,
                     std::string named_curve,
                     std::string extension_id,
                     GenerateKeyCallback callback);

  // Gets the current profile using the BrowserContext object and returns
  // whether the current profile is a sign in profile with
  // ProfileHelper::IsSigninProfile.
  bool IsUsingSigninProfile();

  // If signing was successful, |signature| will contain the signature. If it
  // failed, |signature| will be empty.
  using SignCallback = base::OnceCallback<void(
      std::vector<uint8_t> signature,
      std::optional<crosapi::mojom::KeystoreError> error)>;

  // Digests |data|, applies PKCS1 padding if specified by |hash_algorithm| and
  // chooses the signature algorithm according to |key_type| and signs the data
  // with the private key matching |public_key_spki_der|. If a |token_id|
  // is provided and the key is not found in that token, the operation aborts.
  // If |token_id| is not provided (nullopt), all tokens available to the caller
  // will be considered while searching for the key.
  // If the extension does not have permissions for signing with this key, the
  // operation aborts. In case of a one time permission (granted after
  // generating the key), this function also removes the permission to prevent
  // future signing attempts. If signing was successful, |callback| will be
  // invoked with the signature. If it failed, the resulting signature will be
  // empty. Will only call back during the lifetime of this object.
  void SignDigest(std::optional<platform_keys::TokenId> token_id,
                  std::vector<uint8_t> data,
                  std::vector<uint8_t> public_key_spki_der,
                  platform_keys::KeyType key_type,
                  platform_keys::HashAlgorithm hash_algorithm,
                  std::string extension_id,
                  SignCallback callback);

  // Applies PKCS1 padding and afterwards signs the data with the private key
  // matching |public_key_spki_der|. |data| is not digested. If a |token_id|
  // is provided and the key is not found in that token, the operation aborts.
  // If |token_id| is not provided (nullopt), all available tokens to the caller
  // will be considered while searching for the key. The size of |data| (number
  // of octets) must be smaller than k - 11, where k is the key size in octets.
  // If the extension does not have permissions for signing with this key, the
  // operation aborts. In case of a one time permission (granted after
  // generating the key), this function also removes the permission to prevent
  // future signing attempts. If signing was successful, |callback| will be
  // invoked with the signature. If it failed, the resulting signature will be
  // empty. Will only call back during the lifetime of this object.
  void SignRSAPKCS1Raw(std::optional<platform_keys::TokenId> token_id,
                       std::vector<uint8_t> data,
                       std::vector<uint8_t> public_key_spki_der,
                       std::string extension_id,
                       SignCallback callback);

  // If the certificate request could be processed successfully, |matches| will
  // contain the list of matching certificates (maybe empty). If an error
  // occurred, |matches| will be null.
  using SelectCertificatesCallback = base::OnceCallback<void(
      std::unique_ptr<net::CertificateList> matches,
      std::optional<crosapi::mojom::KeystoreError> error)>;

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
  // If selection was successful, |callback| will be invoked with these
  // certificates. If it failed, the resulting certificate list will be empty
  // and an error status will be returned. Will only call back during the
  // lifetime of this object. |web_contents| must not be null.
  void SelectClientCertificates(
      const platform_keys::ClientCertificateRequest& request,
      std::unique_ptr<net::CertificateList> client_certificates,
      bool interactive,
      std::string extension_id,
      SelectCertificatesCallback callback,
      content::WebContents* web_contents);

 private:
  class GenerateRSAKeyTask;
  class GenerateECKeyTask;
  class GenerateKeyTask;
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

  const raw_ptr<content::BrowserContext> browser_context_ = nullptr;
  const raw_ptr<crosapi::mojom::KeystoreService> keystore_service_ = nullptr;
  std::unique_ptr<SelectDelegate> select_delegate_;
  base::queue<std::unique_ptr<Task>> tasks_;
  base::WeakPtrFactory<ExtensionPlatformKeysService> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_H_
