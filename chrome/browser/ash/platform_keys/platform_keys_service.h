// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cert/x509_certificate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class NSSCertDatabase;
class ClientCertStore;
}  // namespace net

namespace ash {
namespace platform_keys {

using GenerateKeyCallback =
    base::OnceCallback<void(const std::string& public_key_spki_der,
                            chromeos::platform_keys::Status status)>;

using SignCallback =
    base::OnceCallback<void(const std::string& signature,
                            chromeos::platform_keys::Status status)>;

// If the certificate request could be processed successfully, |matches| will
// contain the list of matching certificates (which may be empty). If an error
// occurred, |matches| will be null.
using SelectCertificatesCallback =
    base::OnceCallback<void(std::unique_ptr<net::CertificateList> matches,
                            chromeos::platform_keys::Status status)>;

// If the list of certificates could be successfully retrieved, |certs| will
// contain the list of available certificates (maybe empty). If an error
// occurred, |certs| will be empty.
using GetCertificatesCallback =
    base::OnceCallback<void(std::unique_ptr<net::CertificateList> certs,
                            chromeos::platform_keys::Status status)>;

// If the list of key pairs could be successfully retrieved,
// |public_key_spki_der_list| will contain the list of available key pairs (may
// be empty if no key pairs exist). Each available key pair is represented as a
// DER-encoded SubjectPublicKeyInfo. If an error occurred,
// |public_key_spki_der_list| will be empty.
using GetAllKeysCallback =
    base::OnceCallback<void(std::vector<std::string> public_key_spki_der_list,
                            chromeos::platform_keys::Status status)>;

using ImportCertificateCallback =
    base::OnceCallback<void(chromeos::platform_keys::Status status)>;

using RemoveCertificateCallback =
    base::OnceCallback<void(chromeos::platform_keys::Status status)>;

using RemoveKeyCallback =
    base::OnceCallback<void(chromeos::platform_keys::Status status)>;

// If the list of available tokens could be successfully retrieved, |token_ids|
// will contain the token ids. If an error occurs, |token_ids| will be nullptr.
using GetTokensCallback = base::OnceCallback<void(
    std::unique_ptr<std::vector<chromeos::platform_keys::TokenId>> token_ids,
    chromeos::platform_keys::Status status)>;

// If token ids have been successfully retrieved, two cases are possible then:
// If |token_ids| is not empty, |token_ids| has been filled with the identifiers
// of the tokens the private key was found on and the user has access to.
// If |token_ids| is empty, the private key has not been found on any token the
// user has access to. Note that this is also the case if the key exists on the
// system token, but the current user does not have access to the system token.
// If an error occurred during processing, |token_ids| will be empty.
using GetKeyLocationsCallback = base::OnceCallback<void(
    const std::vector<chromeos::platform_keys::TokenId>& token_ids,
    chromeos::platform_keys::Status status)>;

using SetAttributeForKeyCallback =
    base::OnceCallback<void(chromeos::platform_keys::Status status)>;

// If the attribute value has been successfully retrieved, |attribute_value|
// will contain the result. If an error occurs, |attribute_value| will be empty.
using GetAttributeForKeyCallback =
    base::OnceCallback<void(const absl::optional<std::string>& attribute_value,
                            chromeos::platform_keys::Status status)>;

// If the availability of the key on the provided token has been successfully
// determined, |on_token| will contain the result. If an error occurs,
// |on_token| will be empty and an error |status| will be returned.
using IsKeyOnTokenCallback =
    base::OnceCallback<void(absl::optional<bool> on_token,
                            chromeos::platform_keys::Status status)>;

// An observer that gets notified when the PlatformKeysService is being shut
// down.
class PlatformKeysServiceObserver : public base::CheckedObserver {
 public:
  // Called when the PlatformKeysService is being shut down.
  // It may not be used after this call - any usage except for removing the
  // observer will DCHECK.
  virtual void OnPlatformKeysServiceShutDown() = 0;
};

// Functions of this class shouldn't be called directly from the context of
// an extension. Instead use ExtensionPlatformKeysService which enforces
// restrictions upon extensions.
// All public methods of this class should be called on the UI thread and all of
// the callbacks will be called with the result on the UI thread as well.
// When the underlying key store is not available anymore, a PlatformKeysService
// is shut down. Any function called after that will fail with an error.
// For a Profile-specific PlatformKeysService, this will be when the Profile is
// being destroyed.
// For a device-wide PlatformKeysService, this will be at some point during
// chrome shut down.
// Use AddObserver to get a notification when the service shuts down.
class PlatformKeysService : public KeyedService {
 public:
  PlatformKeysService() = default;
  PlatformKeysService(const PlatformKeysService&) = delete;
  PlatformKeysService& operator=(const PlatformKeysService&) = delete;
  ~PlatformKeysService() override = default;

  // Adds |observer| which will be notified when this service is being shut
  // down.
  virtual void AddObserver(PlatformKeysServiceObserver* observer) = 0;
  // Removes a previously added |observer|.
  virtual void RemoveObserver(PlatformKeysServiceObserver* observer) = 0;

  // Generates a RSA key pair with |modulus_length_bits|. |token_id| specifies
  // the token to store the key pair on. |callback| will be invoked with the
  // resulting public key or an error status.
  virtual void GenerateRSAKey(chromeos::platform_keys::TokenId token_id,
                              unsigned int modulus_length_bits,
                              bool sw_backed,
                              GenerateKeyCallback callback) = 0;

  // Generates a EC key pair with |named_curve|. |token_id| specifies the token
  // to store the key pair on. |callback| will be invoked with the resulting
  // public key or an error status.
  virtual void GenerateECKey(chromeos::platform_keys::TokenId token_id,
                             const std::string& named_curve,
                             GenerateKeyCallback callback) = 0;

  // Digests |data|, applies PKCS1 padding and afterwards signs the data with
  // the private key matching |public_key_spki_der|. If the key is not found in
  // that |token_id| (or in none of the available tokens if |token_id| is not
  // specified), the operation aborts. |callback| will be invoked with the
  // signature or an error status.
  virtual void SignRSAPKCS1Digest(
      absl::optional<chromeos::platform_keys::TokenId> token_id,
      const std::string& data,
      const std::string& public_key_spki_der,
      chromeos::platform_keys::HashAlgorithm hash_algorithm,
      SignCallback callback) = 0;

  // Applies PKCS1 padding and afterwards signs the data with the private key
  // matching |public_key_spki_der|. |data| is not digested, PKCS1 DigestInfo is
  // not prepended. If the key is not found in that |token_id| (or in none of
  // the available tokens if |token_id| is not specified), the operation aborts.
  // The size of |data| (number of octets) must be smaller than k - 11, where k
  // is the key size in octets. |callback| will be invoked with the signature or
  // an error status.
  virtual void SignRSAPKCS1Raw(
      absl::optional<chromeos::platform_keys::TokenId> token_id,
      const std::string& data,
      const std::string& public_key_spki_der,
      SignCallback callback) = 0;

  // Digests |data| and afterwards signs the data with the private key matching
  // |public_key_spki_der|. If the key is not found in that |token_id| (or in
  // none of the available tokens if |token_id| is not specified), the operation
  // aborts. |callback| will be invoked with the ECDSA signature or an error
  // status.
  virtual void SignECDSADigest(
      absl::optional<chromeos::platform_keys::TokenId> token_id,
      const std::string& data,
      const std::string& public_key_spki_der,
      chromeos::platform_keys::HashAlgorithm hash_algorithm,
      SignCallback callback) = 0;

  // Returns the list of all certificates that were issued by one of the
  // |certificate_authorities|. If |certificate_authorities| is empty, all
  // certificates will be returned. |callback| will be invoked with the matches
  // or an error status.
  virtual void SelectClientCertificates(
      std::vector<std::string> certificate_authorities,
      const SelectCertificatesCallback callback) = 0;

  // Returns the list of all certificates with stored private key available from
  // the given token. Only certificates from the specified |token_id| are
  // listed. |callback| will be invoked with the list of available certificates
  // or an error status.
  virtual void GetCertificates(chromeos::platform_keys::TokenId token_id,
                               const GetCertificatesCallback callback) = 0;

  // Returns the list of all public keys available from the given |token_id|
  // that have corresponding private keys on the same token as a list of
  // DER-encoded SubjectPublicKeyInfo strings. |callback| will be invoked on the
  // UI thread with the list of available public keys, possibly with an error
  // status.
  virtual void GetAllKeys(chromeos::platform_keys::TokenId token_id,
                          GetAllKeysCallback callback) = 0;

  // Imports |certificate| to the given token if the certified key is already
  // stored in this token. Any intermediate of |certificate| will be ignored.
  // |token_id| specifies the token to store the certificate on. The private key
  // must be stored on the same token. |callback| will be invoked when the
  // import is finished, possibly with an error status.
  virtual void ImportCertificate(
      chromeos::platform_keys::TokenId token_id,
      const scoped_refptr<net::X509Certificate>& certificate,
      ImportCertificateCallback callback) = 0;

  // Removes |certificate| from the given token. Any intermediate of
  // |certificate| will be ignored. |token_id| specifies the token to remove the
  // certificate from. |callback| will be invoked when the removal is finished,
  // possibly with an error status.
  virtual void RemoveCertificate(
      chromeos::platform_keys::TokenId token_id,
      const scoped_refptr<net::X509Certificate>& certificate,
      RemoveCertificateCallback callback) = 0;

  // Removes the key pair if no matching certificates exist. Only keys in the
  // given |token_id| are considered. |callback| will be invoked on the UI
  // thread when the removal is finished, possibly with an error status.
  virtual void RemoveKey(chromeos::platform_keys::TokenId token_id,
                         const std::string& public_key_spki_der,
                         RemoveKeyCallback callback) = 0;

  // Gets the list of available tokens. |callback| will be invoked when the list
  // of available tokens is determined, possibly with an error status.
  // Calls |callback| on the UI thread.
  virtual void GetTokens(GetTokensCallback callback) = 0;

  // Determines the token(s) on which the private key corresponding to
  // |public_key_spki_der| is stored. |callback| will be invoked when the token
  // ids are determined, possibly with an error status. Calls |callback| on the
  // UI thread.
  virtual void GetKeyLocations(const std::string& public_key_spki_der,
                               GetKeyLocationsCallback callback) = 0;

  // Sets |attribute_type| for the private key corresponding to
  // |public_key_spki_der| to |attribute_value| only if the key is in
  // |token_id|. |callback| will be invoked on the UI thread when setting the
  // attribute is done, possibly with an error status.
  virtual void SetAttributeForKey(
      chromeos::platform_keys::TokenId token_id,
      const std::string& public_key_spki_der,
      chromeos::platform_keys::KeyAttributeType attribute_type,
      const std::string& attribute_value,
      SetAttributeForKeyCallback callback) = 0;

  // Gets |attribute_type| for the private key corresponding to
  // |public_key_spki_der| only if the key is in |token_id|. |callback| will be
  // invoked on the UI thread when getting the attribute is done, possibly with
  // an error message. In case no value was set for |attribute_type|, an error
  // |status| and absl::nullopt |attribute_value| will be returned.
  virtual void GetAttributeForKey(
      chromeos::platform_keys::TokenId token_id,
      const std::string& public_key_spki_der,
      chromeos::platform_keys::KeyAttributeType attribute_type,
      GetAttributeForKeyCallback callback) = 0;

  // Determines if |public_key_spki_der| resides on |token_id|. |callback| will
  // be invoked on the UI thread with the result. If an error occurred, an error
  // |status| will be returned and absl::nullopt |on_token| will be returned.
  virtual void IsKeyOnToken(chromeos::platform_keys::TokenId token_id,
                            const std::string& public_key_spki_der,
                            IsKeyOnTokenCallback callback) = 0;

  // Softoken NSS PKCS11 module (used for testing) allows only predefined key
  // attributes to be set and retrieved. Chaps supports setting and retrieving
  // custom attributes.
  // If |map_to_softoken_attrs_for_testing| is true, the service will use
  // fake KeyAttribute mappings predefined in softoken module for testing.
  // Otherwise, the real mappings to constants in
  // third_party/cros_system_api/constants/pkcs11_custom_attributes.h will be
  // used.
  virtual void SetMapToSoftokenAttrsForTesting(
      const bool map_to_softoken_attrs_for_testing) = 0;
};

class PlatformKeysServiceImplDelegate {
 public:
  PlatformKeysServiceImplDelegate();
  virtual ~PlatformKeysServiceImplDelegate();
  PlatformKeysServiceImplDelegate(
      const PlatformKeysServiceImplDelegate& other) = delete;
  PlatformKeysServiceImplDelegate& operator=(
      const PlatformKeysServiceImplDelegate& other) = delete;

  // |on_shutdown_callback| will be called when the underlying key/certificate
  // store is shut down. It is an error to call this twice, or after the
  // delegate has been shut down.
  void SetOnShutdownCallback(base::OnceClosure on_shutdown_callback);

  // This callback is invoked by GetNSSCertDatabase.
  using OnGotNSSCertDatabase = base::OnceCallback<void(net::NSSCertDatabase*)>;

  // Retrieves the NSSCertDatabase that should be used for certificate
  // operations. |callback| will be called on the thread that GetNSSCertDatabase
  // has been called on.
  virtual void GetNSSCertDatabase(OnGotNSSCertDatabase callback) = 0;

  // Creates a ClientCertStore that should be used to list / operate on client
  // certificates.
  virtual std::unique_ptr<net::ClientCertStore> CreateClientCertStore() = 0;

  bool IsShutDown() const;

 protected:
  void ShutDown();

 private:
  // A callback that should be called when the underlying key/certificate store
  // is shut down.
  base::OnceClosure on_shutdown_callback_;

  // True if the underlying key/certificate store has already been shut down.
  bool shut_down_ = false;
};

class PlatformKeysServiceImpl final : public PlatformKeysService {
 public:
  explicit PlatformKeysServiceImpl(
      std::unique_ptr<PlatformKeysServiceImplDelegate> delegate);
  ~PlatformKeysServiceImpl() override;

  // PlatformKeysService
  void AddObserver(PlatformKeysServiceObserver* observer) override;
  void RemoveObserver(PlatformKeysServiceObserver* observer) override;
  void GenerateRSAKey(chromeos::platform_keys::TokenId token_id,
                      unsigned int modulus_length_bits,
                      bool sw_backed,
                      GenerateKeyCallback callback) override;
  void GenerateECKey(chromeos::platform_keys::TokenId token_id,
                     const std::string& named_curve,
                     GenerateKeyCallback callback) override;
  void SignRSAPKCS1Digest(
      absl::optional<chromeos::platform_keys::TokenId> token_id,
      const std::string& data,
      const std::string& public_key_spki_der,
      chromeos::platform_keys::HashAlgorithm hash_algorithm,
      SignCallback callback) override;
  void SignRSAPKCS1Raw(
      absl::optional<chromeos::platform_keys::TokenId> token_id,
      const std::string& data,
      const std::string& public_key_spki_der,
      SignCallback callback) override;
  void SignECDSADigest(
      absl::optional<chromeos::platform_keys::TokenId> token_id,
      const std::string& data,
      const std::string& public_key_spki_der,
      chromeos::platform_keys::HashAlgorithm hash_algorithm,
      SignCallback callback) override;
  void SelectClientCertificates(
      std::vector<std::string> certificate_authorities,
      SelectCertificatesCallback callback) override;
  void GetCertificates(chromeos::platform_keys::TokenId token_id,
                       GetCertificatesCallback callback) override;
  void GetAllKeys(chromeos::platform_keys::TokenId token_id,
                  GetAllKeysCallback callback) override;
  void ImportCertificate(chromeos::platform_keys::TokenId token_id,
                         const scoped_refptr<net::X509Certificate>& certificate,
                         ImportCertificateCallback callback) override;
  void RemoveCertificate(chromeos::platform_keys::TokenId token_id,
                         const scoped_refptr<net::X509Certificate>& certificate,
                         RemoveCertificateCallback callback) override;
  void RemoveKey(chromeos::platform_keys::TokenId token_id,
                 const std::string& public_key_spki_der,
                 RemoveKeyCallback callback) override;
  void GetTokens(GetTokensCallback callback) override;
  void GetKeyLocations(const std::string& public_key_spki_der,
                       const GetKeyLocationsCallback callback) override;
  void SetAttributeForKey(
      chromeos::platform_keys::TokenId token_id,
      const std::string& public_key_spki_der,
      chromeos::platform_keys::KeyAttributeType attribute_type,
      const std::string& attribute_value,
      SetAttributeForKeyCallback callback) override;
  void GetAttributeForKey(
      chromeos::platform_keys::TokenId token_id,
      const std::string& public_key_spki_der,
      chromeos::platform_keys::KeyAttributeType attribute_type,
      GetAttributeForKeyCallback callback) override;
  void IsKeyOnToken(chromeos::platform_keys::TokenId token_id,
                    const std::string& public_key_spki_der,
                    IsKeyOnTokenCallback callback) override;
  void SetMapToSoftokenAttrsForTesting(
      bool map_to_softoken_attrs_for_testing) override;
  bool IsSetMapToSoftokenAttrsForTesting();

 private:
  void OnDelegateShutDown();

  std::unique_ptr<PlatformKeysServiceImplDelegate> const delegate_;
  // List of observers that will be notified when the service is shut down.
  base::ObserverList<PlatformKeysServiceObserver> observers_;
  bool map_to_softoken_attrs_for_testing_ = false;
  base::WeakPtrFactory<PlatformKeysServiceImpl> weak_factory_{this};
};

}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_
