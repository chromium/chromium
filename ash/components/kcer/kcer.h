// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_KCER_H_
#define ASH_COMPONENTS_KCER_KCER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "ash/components/kcer/key_permissions.pb.h"
#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_runner.h"
#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace kcer {

// Strong alias to Kcer related types to prevent incorrect cross-assignment.
using Pkcs11Id = base::StrongAlias<class TypeTagPkcs11Id, std::vector<uint8_t>>;
using Signature =
    base::StrongAlias<class TypeTagSignature, std::vector<uint8_t>>;
using PublicKeySpki =
    base::StrongAlias<class TypeTagPublicKeySpki, std::vector<uint8_t>>;
using CertDer = base::StrongAlias<class TypeTagCertDer, std::vector<uint8_t>>;
using Pkcs8PrivateKeyInfoDer =
    base::StrongAlias<class TypeTagPkcs8PrivateKeyInfoDer,
                      std::vector<uint8_t>>;
using Pkcs12Blob =
    base::StrongAlias<class TypeTagPkcs12Blob, std::vector<uint8_t>>;
using DataToSign =
    base::StrongAlias<class TypeTagDataToSign, std::vector<uint8_t>>;
// Digest of the DataToSign. If the signing algorithm expects a prefix (such as
// DigestInfo for RSA), it is already prepended for this type.
using DigestWithPrefix =
    base::StrongAlias<class TypeTagDigestWithPrefix, std::vector<uint8_t>>;

// Values should not be reused or renumbered.
enum class COMPONENT_EXPORT(KCER) Error {
  kUnknownError = 0,
  kNotImplemented = 1,
  kNotSupported = 2,
  kTokenIsNotAvailable = 3,
  kTokenInitializationFailed = 4,
  kFailedToGenerateKey = 5,
  kFailedToExportPublicKey = 6,
  kFailedToEncodePublicKey = 7,
  kFailedToImportKey = 8,
  kInvalidCertificate = 9,
  kFailedToImportCertificate = 10,
  kFailedToRemoveCertificate = 11,
  kKeyNotFound = 12,
  kUnknownKeyType = 13,
  kFailedToGetKeyId = 14,
  kFailedToReadAttribute = 15,
  kFailedToWriteAttribute = 16,
  kFailedToParseKeyPermissions = 17,
  kUnexpectedSigningScheme = 18,
  kKeyDoesNotSupportSigningScheme = 19,
  kFailedToSignFailedToDigest = 20,
  kFailedToSignFailedToAddPrefix = 21,
  kFailedToSignFailedToGetSignatureLength = 22,
  kFailedToSign = 23,
  kFailedToSignBadSignatureLength = 24,
  kFailedToDerEncode = 25,
  kInputTooLong = 26,
  kFailedToListKeys = 27,
  kFailedToRemovePrivateKey = 28,
  kFailedToRemovePublicKey = 29,
  kFailedToRemoveObjects = 30,
  kFailedToCreateSpki = 31,
  kFailedToGetPkcs11Id = 32,
  kFailedToSearchForObjects = 33,
  kPkcs11SessionFailure = 34,
  kBadKeyParams = 35,
  kUnexpectedFindResult = 36,
  kFailedToDecodeKeyAttributes = 37,
  kFailedToRetrieveMechanismList = 38,
  kFailedToParseKey = 39,
  kFailedToGetIssuerName = 40,
  kFailedToGetSubjectName = 41,
  kFailedToGetSerialNumber = 42,
  kFailedToParsePkcs12 = 43,
  kInvalidPkcs12 = 44,
  kPkcs12WrongPassword = 45,
  kPkcs12InvalidMac = 46,
  kFailedToMakeCertNickname = 47,
  kAlreadyExists = 48,
  kMaxValue = kAlreadyExists,
};

// Handles for tokens on ChromeOS.
enum class COMPONENT_EXPORT(KCER) Token {
  // Keys and certificates storage that belongs to a specific user.
  kUser,
  // Keys and certificates storage that belongs to the entire
  // device (some users might still not have access to it).
  kDevice,
  kMaxValue = kDevice,
};

// Additional info related to a token.
struct COMPONENT_EXPORT(KCER) TokenInfo {
  // PKCS#11 id assigned to the token by Chaps.
  // Unstable across restarts.
  uint32_t pkcs11_id = 0;
  // Human readable name of the token.
  std::string token_name;
  // Human readable name of the module (i.e. Chaps).
  std::string module_name;
};

enum class COMPONENT_EXPORT(KCER) KeyType {
  kRsa,
  kEcc,
};

// Supported sizes for RSA keys. It's allowed to static_cast the values to
// uint32_t.
enum class COMPONENT_EXPORT(KCER) RsaModulusLength {
  k1024 = 1024,
  k2048 = 2048,
  k4096 = 4096
};

enum class COMPONENT_EXPORT(KCER) EllipticCurve {
  kP256,
};

// Possible sign schemes (aka algorithms) for Kcer::Sign() method. Maps 1-to-1
// to OpenSSL SSL_* constants. It is allowed to cast SigningScheme to SSL_*.
enum class COMPONENT_EXPORT(KCER) SigningScheme {
  kRsaPkcs1Sha1 = SSL_SIGN_RSA_PKCS1_SHA1,
  kRsaPkcs1Sha256 = SSL_SIGN_RSA_PKCS1_SHA256,
  kRsaPkcs1Sha384 = SSL_SIGN_RSA_PKCS1_SHA384,
  kRsaPkcs1Sha512 = SSL_SIGN_RSA_PKCS1_SHA512,
  kEcdsaSecp256r1Sha256 = SSL_SIGN_ECDSA_SECP256R1_SHA256,
  kEcdsaSecp384r1Sha384 = SSL_SIGN_ECDSA_SECP384R1_SHA384,
  kEcdsaSecp521r1Sha512 = SSL_SIGN_ECDSA_SECP521R1_SHA512,
  kRsaPssRsaeSha256 = SSL_SIGN_RSA_PSS_RSAE_SHA256,
  kRsaPssRsaeSha384 = SSL_SIGN_RSA_PSS_RSAE_SHA384,
  kRsaPssRsaeSha512 = SSL_SIGN_RSA_PSS_RSAE_SHA512,
};

class COMPONENT_EXPORT(KCER) PublicKey {
 public:
  // Public for implementations of Kcer interface, should not be used by end
  // users.
  PublicKey(Token token, Pkcs11Id pkcs11_id, PublicKeySpki pub_key_spki);
  PublicKey(const PublicKey&);
  PublicKey& operator=(const PublicKey&);
  PublicKey(PublicKey&&);
  PublicKey& operator=(PublicKey&&);
  ~PublicKey();

  bool operator==(const PublicKey& other) const;
  bool operator!=(const PublicKey& other) const;

  Token GetToken() const { return token_; }
  const Pkcs11Id& GetPkcs11Id() const { return pkcs11_id_; }
  const PublicKeySpki& GetSpki() const { return pub_key_spki_; }

 private:
  Token token_;
  Pkcs11Id pkcs11_id_;
  PublicKeySpki pub_key_spki_;
};

// Additional info related to a key pair.
struct COMPONENT_EXPORT(KCER) KeyInfo {
  KeyInfo();
  ~KeyInfo();
  KeyInfo(const KeyInfo&);
  KeyInfo& operator=(const KeyInfo&);
  KeyInfo(KeyInfo&&);
  KeyInfo& operator=(KeyInfo&&);

  bool is_hardware_backed;
  KeyType key_type;
  std::vector<SigningScheme> supported_signing_schemes;
  std::optional<std::string> nickname;
};

class COMPONENT_EXPORT(KCER) Cert : public base::RefCountedThreadSafe<Cert> {
 public:
  // Public for implementations of Kcer interface, should not be used by end
  // users.
  Cert(Token token,
       Pkcs11Id pkcs11_id,
       std::string nickname,
       scoped_refptr<net::X509Certificate> x509_cert);

  Token GetToken() const { return token_; }
  const Pkcs11Id& GetPkcs11Id() const { return pkcs11_id_; }
  // Gets nickname of the certificate (not to be confused with
  // the nickname of the key).
  const std::string& GetNickname() const { return nickname_; }
  scoped_refptr<net::X509Certificate> GetX509Cert() const { return x509_cert_; }

 private:
  friend class base::RefCountedThreadSafe<Cert>;
  ~Cert();

  const Token token_;
  const Pkcs11Id pkcs11_id_;
  const std::string nickname_;
  const scoped_refptr<net::X509Certificate> x509_cert_;
};

// Handle for a private key. Can be trivially constructed from other related
// objects. It's primarily just a convenience class to call methods that usually
// would require a private key (e.g. Sign). If the corresponding private key
// does not actually exist, the methods will return an error.
class COMPONENT_EXPORT(KCER) PrivateKeyHandle {
 public:
  explicit PrivateKeyHandle(const PublicKey& public_key);
  explicit PrivateKeyHandle(const Cert&);
  PrivateKeyHandle(Token token, PublicKeySpki pub_key_spki);
  // If possible, prefer specifying the token (use another constructor) for
  // better performance. Calling a Kcer method using this overload will make it
  // search on all tokens for the key before proceeding with the request.
  explicit PrivateKeyHandle(PublicKeySpki pub_key_spki);
  // Used internally to convert from `PrivateKeyHandle(PublicKeySpki)`. `other`
  // must have no token set.
  PrivateKeyHandle(Token token, PrivateKeyHandle&& other);
  ~PrivateKeyHandle();

  PrivateKeyHandle(const PrivateKeyHandle&);
  PrivateKeyHandle& operator=(const PrivateKeyHandle&);
  PrivateKeyHandle(PrivateKeyHandle&&);
  PrivateKeyHandle& operator=(PrivateKeyHandle&&);

  // Public for implementations of Kcer only.
  const std::optional<Token>& GetTokenInternal() const { return token_; }
  const Pkcs11Id& GetPkcs11IdInternal() const { return pkcs11_id_; }
  const PublicKeySpki& GetSpkiInternal() const { return pub_key_spki_; }
  void SetPkcs11IdInternal(Pkcs11Id pkcs11_id) {
    pkcs11_id_ = std::move(pkcs11_id);
  }

 private:
  // Depending on how PrivateKeyHandle is constructed, some member variables
  // might not contain valid values, possible combinations:
  // * Only `token_` and `pkcs11_id_` are populated.
  // * Only `token_` and `pub_key_spki_` are populated.
  // * Only `pub_key_spki_` is populated.
  // * All member variables are populated.
  std::optional<Token> token_;
  Pkcs11Id pkcs11_id_;
  PublicKeySpki pub_key_spki_;
};

// All the methods provided by Kcer. If the underlying storage is not ready when
// a method is called, it will be queued and executed later.
// Implementation note: most methods take arguments by value so they can be
// moved into base::BindOnce (without extra copy) and posted on a different
// sequence (where tokens live). The callbacks might be executed synchronously
// (without re-posting them).
class COMPONENT_EXPORT(KCER) Kcer {
 public:
  // base::expected<void, Error> could also be expressed as
  // std::optional<Error>, but then result.has_value() would mean opposite
  // things for methods with base::expected vs std::optional.
  using StatusCallback = base::OnceCallback<void(base::expected<void, Error>)>;
  using GenerateKeyCallback =
      base::OnceCallback<void(base::expected<PublicKey, Error>)>;
  using ImportKeyCallback =
      base::OnceCallback<void(base::expected<PublicKey, Error>)>;
  using ExportPkcs12Callback =
      base::OnceCallback<void(base::expected<CertDer, Error>)>;
  using ListKeysCallback =
      base::OnceCallback<void(std::vector<PublicKey>,
                              base::flat_map<Token, Error>)>;
  using ListCertsCallback =
      base::OnceCallback<void(std::vector<scoped_refptr<const Cert>>,
                              base::flat_map<Token, Error>)>;
  using DoesKeyExistCallback =
      base::OnceCallback<void(base::expected<bool, Error>)>;
  using SignCallback =
      base::OnceCallback<void(base::expected<Signature, Error>)>;
  using GetAvailableTokensCallback =
      base::OnceCallback<void(base::flat_set<Token>)>;
  using GetTokenInfoCallback =
      base::OnceCallback<void(base::expected<TokenInfo, Error>)>;
  using GetKeyInfoCallback =
      base::OnceCallback<void(base::expected<KeyInfo, Error>)>;
  using GetKeyPermissionsCallback = base::OnceCallback<void(
      base::expected<std::optional<chaps::KeyPermissions>, Error>)>;
  using GetCertProvisioningProfileIdCallback = base::OnceCallback<void(
      base::expected<std::optional<std::string>, Error>)>;

  Kcer() = default;
  virtual ~Kcer() = default;

  // Saves the `callback` that will be called when client certificates are
  // imported / removed.
  virtual base::CallbackListSubscription AddObserver(
      base::RepeatingClosure callback) = 0;

  // Generates a new RSA key pair in the `token`. If `hardware_backed` is false,
  // the key pair will not be hardware protected (by the TPM). Software keys are
  // usually faster, but less secure. Returns a public key on success, an error
  // otherwise.
  // TODO(miersh): Software keys are currently only implemented in Ash because
  // they are only used there. When Kcer-without-NSS is implemented, they should
  // work everywhere.
  virtual void GenerateRsaKey(Token token,
                              RsaModulusLength modulus_length_bits,
                              bool hardware_backed,
                              GenerateKeyCallback callback) = 0;
  // Generates a new EC key pair in the `token`. If `hardware_backed` is false,
  // the key pair will not be hardware protected (by the TPM). Software keys are
  // usually faster, but less secure. Returns a public key on success, an error
  // otherwise.
  virtual void GenerateEcKey(Token token,
                             EllipticCurve curve,
                             bool hardware_backed,
                             GenerateKeyCallback callback) = 0;

  // Imports a key pair from bytes `key_pair` in the PKCS#8 format (DER encoded)
  // into the `token` (as software-backed). It is caller's responsibility to
  // make sure that the same key doesn't end up on several different tokens at
  // the same time (otherwise Kcer is allowed to perform any future operations,
  // such as RemoveKey, with only one of the keys). Returns a public key on
  // success, an error otherwise. WARNING: With the current implementation the
  // key can be used with most other methods, but it won't appear in the
  // ListKeys() results.
  // TODO(miersh): Make ListKeys() return imported keys.
  virtual void ImportKey(Token token,
                         Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                         ImportKeyCallback callback) = 0;
  // Imports a client certificate from bytes `cert` (DER-encoded X.509
  // certificate) into the `token`. A key pair for it should already exist on
  // the token (will fail otherwise). Returns an error on failure.
  virtual void ImportCertFromBytes(Token token,
                                   CertDer cert_der,
                                   StatusCallback callback) = 0;
  // Imports an X.509 certificate `cert` into the `token`. A key pair for it
  // should already exist on the token (will fail otherwise). Returns
  // base::nullopt on success, an error otherwise.
  virtual void ImportX509Cert(Token token,
                              scoped_refptr<net::X509Certificate> cert,
                              StatusCallback callback) = 0;
  // Imports a client certificate and its private key from `pkcs12_blob` encoded
  // in the PKCS#12 format into the `token`. If `hardware_backed` is false, the
  // key will not be hardware protected (by the TPM). Returns an error on
  // failure. If `mark_as_migrated` is true, all created objects will be marked
  // with a special attribute to allow a rollback for b/264387231.
  virtual void ImportPkcs12Cert(Token token,
                                Pkcs12Blob pkcs12_blob,
                                std::string password,
                                bool hardware_backed,
                                bool mark_as_migrated,
                                StatusCallback callback) = 0;

  // Exports an existing certificate in the PKCS#12 format. Returns a non-empty
  // certificate (as bytes) on success, an error otherwise. Only certificates
  // that were imported using `ImportPkcs12Cert` and are not hardware protected
  // are guaranteed to be exportable. Certificates with hardware protected keys
  // can never be exported in PKCS#12 format.
  virtual void ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                                ExportPkcs12Callback callback) = 0;

  // Removes the key pair and associated client certificates (if any exist).
  // Returns an error on failure.
  virtual void RemoveKeyAndCerts(PrivateKeyHandle key,
                                 StatusCallback callback) = 0;
  // Removes the client certificate. The key for the certificate will remain in
  // the storage. Returns success if the cert was removed or already not
  // present. Returns an error on failure.
  virtual void RemoveCert(scoped_refptr<const Cert> cert,
                          StatusCallback callback) = 0;

  // Lists available key pairs from `tokens`. Each key pair is represented by
  // its public key. Returns a vector of public keys that were successfully
  // retrieved and a map with errors from each token (can be empty).
  virtual void ListKeys(base::flat_set<Token> tokens,
                        ListKeysCallback callback) = 0;
  // Lists available client certificates from `tokens`. Returns a vector of
  // scoped_refptr<const Cert>'s that were successfully retrieved and a map with
  // errors from each token (can be empty).
  virtual void ListCerts(base::flat_set<Token> tokens,
                         ListCertsCallback callback) = 0;

  // Checks whether the private key for the handle `key` exists in the
  // certificate storage. Returns true if the key exists, false if it doesn't
  // exist, an error if failed to check.
  virtual void DoesPrivateKeyExist(PrivateKeyHandle key,
                                   DoesKeyExistCallback callback) = 0;

  // Signs `data` using the private `key`. The data will be hashed and signed
  // according to the `signing_scheme`. Returns a non-empty signature on
  // success, an error otherwise.
  virtual void Sign(PrivateKeyHandle key,
                    SigningScheme signing_scheme,
                    DataToSign data,
                    SignCallback callback) = 0;

  // Applies RSASSA-PKCS1-v1_5 padding and afterwards signs the `digest` (a
  // pre-hashed value) with the private `key` (must be RSA). PKCS1 DigestInfo is
  // expected to already be prepended to the `digest`. The size of `digest`
  // (number of octets) must be smaller than k-11, where k is the key size in
  // octets. Returns a non-empty signature on success, an error otherwise.
  //
  // WARNING: digest_with_prefix must be the result of hashing the data to be
  // signed with a secure hash function, then wrapping in the corresponding
  // DigestInfo structure, as described in RSASSA-PKCS1-v1_5. Passing in any
  // other input, notably skipping the hash operation, will not result in a
  // secure signature scheme. This function does not check either of these and,
  // instead, callers are assumed to be trusted to do this.
  //
  // This is currently only used for the CertProvisioning feature and may not be
  // used in other contexts. Please consult Kcer owners if the Sign API does not
  // meet your needs.
  virtual void SignRsaPkcs1Raw(PrivateKeyHandle key,
                               DigestWithPrefix digest_with_prefix,
                               SignCallback callback) = 0;

  // Returns tokens that are available to the current instance of Kcer.
  virtual void GetAvailableTokens(GetAvailableTokensCallback callback) = 0;

  // Retrieves additional info for the loaded `token`. Returns a `TokenInfo`
  // struct on success, kTokenNotAvailable if the `token` will never be loaded,
  // kTokenLoading if the `token` is still loading (can eventually transition
  // into kTokenNotAvailable), some other error otherwise.
  virtual void GetTokenInfo(Token token, GetTokenInfoCallback callback) = 0;
  // Retrieves additional info for the `key`. Returns a `KeyInfo` struct on
  // success, an error otherwise.
  virtual void GetKeyInfo(PrivateKeyHandle key,
                          GetKeyInfoCallback callback) = 0;
  // Retrieves key permissions for the `key` (see key_permissions.proto).
  virtual void GetKeyPermissions(PrivateKeyHandle key,
                                 GetKeyPermissionsCallback callback) = 0;
  // Retrieves "certificate provisioning profile id" for the `key` (i.e.
  // "cert_profile_id" from RequiredClientCertificateForUser.yaml and
  // RequiredClientCertificateForDevice.yaml).
  virtual void GetCertProvisioningProfileId(
      PrivateKeyHandle key,
      GetCertProvisioningProfileIdCallback callback) = 0;

  // Sets the `nickname` on the `key`. (Not to be confused with the nickname of
  // the certificate.) Returns an error on failure.
  // The nickname on the key is partially independent from the certificates'
  // nicknames and is stored as CKA_LABEL in PKCS#11 attributes of the key
  // object. When a new certificate is imported, its nickname might be copied
  // into the key's nickname (TODO(miersh): this part should be changed in the
  // future), but generally speaking they are not kept in sync.
  virtual void SetKeyNickname(PrivateKeyHandle key,
                              std::string nickname,
                              StatusCallback callback) = 0;
  // Sets the `key_permissions` on the `key`. Returns an error on failure.
  virtual void SetKeyPermissions(PrivateKeyHandle key,
                                 chaps::KeyPermissions key_permissions,
                                 StatusCallback callback) = 0;
  // Sets the Built-In Certificate Provisioning `profile_id` on the `key`.
  // Returns an error on failure.
  virtual void SetCertProvisioningProfileId(PrivateKeyHandle key,
                                            std::string profile_id,
                                            StatusCallback callback) = 0;
};

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_KCER_H_
