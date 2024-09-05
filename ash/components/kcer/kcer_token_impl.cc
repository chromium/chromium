// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/kcer/kcer_token_impl.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/components/kcer/attributes.pb.h"
#include "ash/components/kcer/chaps/high_level_chaps_client.h"
#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "ash/components/kcer/helpers/key_helper.h"
#include "ash/components/kcer/helpers/pkcs12_reader.h"
#include "ash/components/kcer/helpers/pkcs12_validator.h"
#include "ash/components/kcer/kcer_token_utils.h"
#include "ash/components/kcer/kcer_utils.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/openssl_util.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_database.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/x509.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

using AttributeId = kcer::HighLevelChapsClient::AttributeId;

// Needed for bssl::UniquePtr<PKCS8_PRIV_KEY_INFO>.
BSSL_NAMESPACE_BEGIN
BORINGSSL_MAKE_DELETER(PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free)
BSSL_NAMESPACE_END

namespace kcer::internal {
namespace {

std::string_view AsString(base::span<const uint8_t> bytes) {
  return std::string_view(reinterpret_cast<const char*>(bytes.data()),
                          bytes.size());
}

const chaps::Attribute* GetAttribute(const chaps::AttributeList& attr_list,
                                     AttributeId attribute_id) {
  for (int i = 0; i < attr_list.attributes_size(); i++) {
    if (attr_list.attributes(i).type() == static_cast<uint32_t>(attribute_id)) {
      return &(attr_list.attributes(i));
    }
  }
  return nullptr;
}

base::span<const uint8_t> GetAttributeValue(
    const chaps::AttributeList& attr_list,
    AttributeId attribute_id) {
  const chaps::Attribute* attr = GetAttribute(attr_list, attribute_id);
  if (!attr || !attr->has_value()) {
    return {};
  }
  return base::as_bytes(base::make_span(attr->value()));
}

bool GetIsHardwareBacked(const chaps::Attribute* attr,
                         bool& is_hardware_backed) {
  if (!attr || !attr->has_value()) {
    return false;
  }
  if (attr->value().size() != sizeof(chromeos::PKCS11_CK_BBOOL)) {
    return false;
  }
  chromeos::PKCS11_CK_BBOOL key_in_software =
      *reinterpret_cast<const chromeos::PKCS11_CK_BBOOL*>(attr->value().data());
  is_hardware_backed = (key_in_software == chromeos::PKCS11_CK_FALSE);
  return true;
}

bool GetKeyType(const chaps::Attribute* attr, KeyType& key_type) {
  if (!attr || !attr->has_value()) {
    return false;
  }
  if (attr->value().size() != sizeof(chromeos::PKCS11_CK_KEY_TYPE)) {
    return false;
  }
  chromeos::PKCS11_CK_KEY_TYPE pkcs_key_type =
      *reinterpret_cast<const chromeos::PKCS11_CK_KEY_TYPE*>(
          attr->value().data());
  if (pkcs_key_type == chromeos::PKCS11_CKK_RSA) {
    key_type = KeyType::kRsa;
    return true;
  } else if (pkcs_key_type == chromeos::PKCS11_CKK_EC) {
    key_type = KeyType::kEcc;
    return true;
  }
  return false;
}

bool GetOptionalString(const chaps::Attribute* attr,
                       std::optional<std::string>& result_string) {
  if (!attr || !attr->has_value() || !attr->has_length()) {
    return false;
  }
  if (static_cast<uint32_t>(attr->length()) ==
      chromeos::PKCS11_CK_UNAVAILABLE_INFORMATION) {
    // The attribute was not set for the object.
    result_string.reset();
    return true;
  }
  result_string = attr->value();
  return true;
}

// Chaps wraps the EC point in a DER-encoded ASN.1 OctetString, which is
// required by PKCS#11 standard, but it needs to be removed before using it for
// boringssl.
bssl::UniquePtr<ASN1_OCTET_STRING> UnwrapEcPoint(
    base::span<const uint8_t> ec_point) {
  if (ec_point.empty()) {
    return {};
  }

  const uint8_t* data = ec_point.data();
  bssl::UniquePtr<ASN1_OCTET_STRING> result(
      d2i_ASN1_OCTET_STRING(nullptr, &data, ec_point.size()));
  return result;
}

// Backwards compatible with how NSS generated CKA_ID for RSA keys.
Pkcs11Id MakePkcs11IdFromRsaKey(bssl::UniquePtr<RSA> rsa_key) {
  const BIGNUM* modulus = RSA_get0_n(rsa_key.get());
  if (!modulus) {
    LOG(ERROR) << "Could not parse RSA public key";
    return {};
  }

  std::vector<uint8_t> modulus_bytes(BN_num_bytes(modulus));
  // BN_bn2bin returns an absolute value of `modulus`, but according to RFC 8017
  // Section 3.1 the RSA modulus is a positive integer.
  BN_bn2bin(modulus, modulus_bytes.data());

  return MakePkcs11Id(modulus_bytes);
}

// Backwards compatible with how NSS generated CKA_ID for EC keys.
Pkcs11Id MakePkcs11IdFromEcKey(bssl::UniquePtr<EC_KEY> ec_key) {
  const EC_POINT* point = EC_KEY_get0_public_key(ec_key.get());
  const EC_GROUP* group = EC_KEY_get0_group(ec_key.get());

  if (!point || !group) {
    LOG(ERROR) << "Could not parse EC public key";
    return {};
  }

  // Serialize the public key as an uncompressed point in X9.62 form.
  bssl::ScopedCBB cbb;
  uint8_t* point_bytes = nullptr;
  size_t point_bytes_len = 0;
  if (!CBB_init(cbb.get(), 0) ||
      !EC_POINT_point2cbb(
          cbb.get(), group, point,
          point_conversion_form_t::POINT_CONVERSION_UNCOMPRESSED,
          /*ctx=*/nullptr) ||
      !CBB_finish(cbb.get(), &point_bytes, &point_bytes_len)) {
    return {};
  }
  bssl::UniquePtr<uint8_t> point_bytes_deleter(point_bytes);

  return MakePkcs11Id(base::make_span(point_bytes, point_bytes_len));
}

// Calculates PKCS#11 id for the provided public key SPKI.
Pkcs11Id GetPkcs11IdFromSpki(const PublicKeySpki& public_key_spki) {
  if (public_key_spki->empty()) {
    LOG(ERROR) << "Empty public key provided";
    return {};
  }

  const std::vector<uint8_t>& spki = public_key_spki.value();
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> evp_key(EVP_parse_public_key(&cbs));
  if (!evp_key || CBS_len(&cbs) != 0) {
    LOG(ERROR) << "Could not parse public key";
    return {};
  }

  if (EVP_PKEY_base_id(evp_key.get()) == EVP_PKEY_RSA) {
    bssl::UniquePtr<RSA> rsa_key(EVP_PKEY_get1_RSA(evp_key.get()));
    if (!rsa_key) {
      return {};
    }
    return MakePkcs11IdFromRsaKey(std::move(rsa_key));
  }

  if (EVP_PKEY_base_id(evp_key.get()) == EVP_PKEY_EC) {
    bssl::UniquePtr<EC_KEY> ec_key(EVP_PKEY_get1_EC_KEY(evp_key.get()));
    if (!ec_key) {
      return {};
    }
    return MakePkcs11IdFromEcKey(std::move(ec_key));
  }

  return {};
}

// Returns true if the `key` already had PKCS#11 id or it was successfully set.
// Returns false if the `key` still doesn't have the id after the method
// finishes.
bool EnsurePkcs11IdIsSet(PrivateKeyHandle& key) {
  if (!key.GetPkcs11IdInternal()->empty()) {
    return true;
  }

  key.SetPkcs11IdInternal(GetPkcs11IdFromSpki(key.GetSpkiInternal()));
  return !key.GetPkcs11IdInternal()->empty();
}

uint64_t SigningSchemeToPkcs11Mechanism(SigningScheme scheme) {
  switch (scheme) {
    case SigningScheme::kRsaPkcs1Sha1:
    case SigningScheme::kRsaPkcs1Sha256:
    case SigningScheme::kRsaPkcs1Sha384:
    case SigningScheme::kRsaPkcs1Sha512:
      return chromeos::PKCS11_CKM_RSA_PKCS;
    case SigningScheme::kEcdsaSecp256r1Sha256:
    case SigningScheme::kEcdsaSecp384r1Sha384:
    case SigningScheme::kEcdsaSecp521r1Sha512:
      return chromeos::PKCS11_CKM_ECDSA;
    case SigningScheme::kRsaPssRsaeSha256:
    case SigningScheme::kRsaPssRsaeSha384:
    case SigningScheme::kRsaPssRsaeSha512:
      return chromeos::PKCS11_CKM_RSA_PKCS_PSS;
  }
}

void RunClosure(base::OnceClosure closure, uint32_t /*result_code*/) {
  std::move(closure).Run();
}
// A helper method for error handling. When some method fails and should return
// the `error` through the `callback`, but also should clean up something first,
// this helper allows to bind the error to the callback and create a new
// callback for the clean up code.
template <typename T>
base::OnceCallback<void(uint32_t)> Bind(
    base::OnceCallback<void(base::expected<T, Error>)> callback,
    Error error) {
  return base::BindOnce(&RunClosure, base::BindOnce(std::move(callback),
                                                    base::unexpected(error)));
}

// Creates a digest for `data_to_sign` with the correct prefix (if needed) for
// `kcer_signing_scheme`.
base::expected<DigestWithPrefix, Error> DigestOnWorkerThread(
    SigningScheme kcer_signing_scheme,
    DataToSign data_to_sign) {
  // SigningScheme is defined in a way where this cast is meaningful.
  uint16_t ssl_algorithm = static_cast<uint16_t>(kcer_signing_scheme);

  const EVP_MD* digest_method =
      SSL_get_signature_algorithm_digest(ssl_algorithm);
  uint8_t digest_buffer[EVP_MAX_MD_SIZE];
  uint8_t* digest = digest_buffer;
  unsigned int digest_len = 0;
  if (!digest_method ||
      !EVP_Digest(data_to_sign->data(), data_to_sign->size(), digest,
                  &digest_len, digest_method, nullptr)) {
    return base::unexpected(Error::kFailedToSignFailedToDigest);
  }

  bssl::UniquePtr<uint8_t> free_digest_info;
  if ((SSL_get_signature_algorithm_key_type(ssl_algorithm) == EVP_PKEY_RSA) &&
      !SSL_is_signature_algorithm_rsa_pss(ssl_algorithm)) {
    // PKCS#11 Sign expects the caller to prepend the DigestInfo for PKCS #1.
    int hash_nid =
        EVP_MD_type(SSL_get_signature_algorithm_digest(ssl_algorithm));
    int is_alloced = 0;
    size_t digest_with_prefix_len = 0;
    if (!RSA_add_pkcs1_prefix(&digest, &digest_with_prefix_len, &is_alloced,
                              hash_nid, digest, digest_len)) {
      return base::unexpected(Error::kFailedToSignFailedToAddPrefix);
    }
    digest_len = digest_with_prefix_len;
    if (is_alloced) {
      free_digest_info.reset(digest);
    }
  }

  return DigestWithPrefix(std::vector<uint8_t>(digest, digest + digest_len));
}

// The EC signature returned by Chaps is a concatenation of two numbers r and s
// (see PKCS#11 v2.40: 2.3.1 EC Signatures). Kcer needs to return it as a DER
// encoding of the following ASN.1 notations:
// Ecdsa-Sig-Value ::= SEQUENCE {
//     r       INTEGER,
//     s       INTEGER
// }
// (according to the RFC 8422, Section 5.4).
// This function reencodes the signature.
base::expected<std::vector<uint8_t>, Error> ReencodeEcSignature(
    base::span<const uint8_t> signature) {
  if (signature.size() % 2 != 0) {
    return base::unexpected(Error::kFailedToSignBadSignatureLength);
  }
  size_t order_size_bytes = signature.size() / 2;
  base::span<const uint8_t> r_bytes = signature.first(order_size_bytes);
  base::span<const uint8_t> s_bytes = signature.subspan(order_size_bytes);

  // Convert the RAW ECDSA signature to a DER-encoded ECDSA-Sig-Value.
  bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_SIG_new());
  if (!sig || !BN_bin2bn(r_bytes.data(), r_bytes.size(), sig->r) ||
      !BN_bin2bn(s_bytes.data(), s_bytes.size(), sig->s)) {
    return base::unexpected(Error::kFailedToDerEncode);
  }

  std::vector<uint8_t> result_signature;

  {
    const int len = i2d_ECDSA_SIG(sig.get(), nullptr);
    if (len <= 0) {
      return base::unexpected(Error::kFailedToSignBadSignatureLength);
    }
    result_signature.resize(len);
  }

  {
    uint8_t* ptr = result_signature.data();
    const int len = i2d_ECDSA_SIG(sig.get(), &ptr);
    if (len <= 0) {
      return base::unexpected(Error::kFailedToDerEncode);
    }
  }

  return result_signature;
}

std::vector<uint8_t> GetPssSignParams(SigningScheme kcer_signing_scheme) {
  chromeos::PKCS11_CK_RSA_PKCS_PSS_PARAMS pss_params;

  uint16_t ssl_algorithm = static_cast<uint16_t>(kcer_signing_scheme);
  CHECK(SSL_is_signature_algorithm_rsa_pss(ssl_algorithm));

  const EVP_MD* digest_method =
      SSL_get_signature_algorithm_digest(ssl_algorithm);

  switch (EVP_MD_type(digest_method)) {
    case NID_sha256:
      pss_params.hashAlg = CKM_SHA256;
      pss_params.mgf = CKG_MGF1_SHA256;
      break;
    case NID_sha384:
      pss_params.hashAlg = CKM_SHA384;
      pss_params.mgf = CKG_MGF1_SHA384;
      break;
    case NID_sha512:
      pss_params.hashAlg = CKM_SHA512;
      pss_params.mgf = CKG_MGF1_SHA512;
      break;
    default:
      return {};
  }

  // Use the hash length for the salt length.
  pss_params.sLen = EVP_MD_size(digest_method);

  const uint8_t* params_ptr = reinterpret_cast<const uint8_t*>(&pss_params);
  return std::vector<uint8_t>(params_ptr, params_ptr + sizeof(pss_params));
}

}  // namespace

KcerTokenImpl::KcerTokenImpl(Token token, HighLevelChapsClient* chaps_client)
    : token_(token),
      pkcs_11_slot_id_(0),
      chaps_client_(chaps_client),
      kcer_utils_(token, chaps_client) {
  CHECK(chaps_client_);
}
KcerTokenImpl::~KcerTokenImpl() {
  net::CertDatabase::GetInstance()->RemoveObserver(this);
}

// Returns a weak pointer for the token. The pointer can be used to post tasks
// for the token.
base::WeakPtr<KcerToken> KcerTokenImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// Initializes the token with the provided NSS slot. If `nss_slot` is nullptr,
// the initialization is considered failed and the token will return an error
// for all queued and future requests.
void KcerTokenImpl::InitializeWithoutNss(
    SessionChapsClient::SlotId pkcs11_slot_id) {
  pkcs_11_slot_id_ = pkcs11_slot_id;
  kcer_utils_.Initialize(pkcs_11_slot_id_);
  net::CertDatabase::GetInstance()->AddObserver(this);
  // This is supposed to be the first time the task queue is unblocked, no
  // other tasks should be already running.
  UnblockQueueProcessNextTask();
}

void KcerTokenImpl::OnClientCertStoreChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  cache_state_ = CacheState::kOutdated;

  // If task queue is not blocked, trigger cache update immediately.
  if (!is_blocked_) {
    UnblockQueueProcessNextTask();
  }
}

//==============================================================================

KcerTokenImpl::GenerateRsaKeyTask::GenerateRsaKeyTask(
    RsaModulusLength in_modulus_length_bits,
    bool in_hardware_backed,
    Kcer::GenerateKeyCallback in_callback)
    : modulus_length_bits(in_modulus_length_bits),
      hardware_backed(in_hardware_backed),
      callback(std::move(in_callback)) {}
KcerTokenImpl::GenerateRsaKeyTask::GenerateRsaKeyTask(
    GenerateRsaKeyTask&& other) = default;
KcerTokenImpl::GenerateRsaKeyTask::~GenerateRsaKeyTask() = default;

void KcerTokenImpl::GenerateRsaKey(RsaModulusLength modulus_length_bits,
                                   bool hardware_backed,
                                   Kcer::GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::GenerateRsaKey, weak_factory_.GetWeakPtr(),
        modulus_length_bits, hardware_backed, std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  GenerateRsaKeyImpl(GenerateRsaKeyTask(modulus_length_bits, hardware_backed,
                                        std::move(unblocking_callback)));
}

// Generates a new key pair.
void KcerTokenImpl::GenerateRsaKeyImpl(GenerateRsaKeyTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;
  chromeos::PKCS11_CK_ULONG modulus_bits =
      static_cast<uint32_t>(task.modulus_length_bits);
  chromeos::PKCS11_CK_BYTE public_exponent[3] = {0x01, 0x00, 0x01};  // 65537

  chaps::AttributeList public_key_attrs;
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_ENCRYPT,
               MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_VERIFY, MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_WRAP, MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_MODULUS_BITS,
               MakeSpan(&modulus_bits));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_PUBLIC_EXPONENT,
               base::as_byte_span(public_exponent));

  chaps::AttributeList private_key_attrs;
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_TOKEN, MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_PRIVATE,
               MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_SENSITIVE,
               MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_DECRYPT,
               MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_SIGN, MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_UNWRAP,
               MakeSpan(&kTrue));

  if (!task.hardware_backed) {
    AddAttribute(private_key_attrs, chaps::kForceSoftwareAttribute,
                 MakeSpan(&kTrue));
  }

  auto chaps_callback =
      base::BindOnce(&KcerTokenImpl::DidGenerateRsaKey,
                     weak_factory_.GetWeakPtr(), std::move(task));

  chaps_client_->GenerateKeyPair(
      pkcs_11_slot_id_, chromeos::PKCS11_CKM_RSA_PKCS_KEY_PAIR_GEN,
      /*mechanism_parameter=*/{}, std::move(public_key_attrs),
      std::move(private_key_attrs), std::move(chaps_callback));
}

// Fetches the public key attributes of the generated key.
void KcerTokenImpl::DidGenerateRsaKey(GenerateRsaKeyTask task,
                                      ObjectHandle public_key_id,
                                      ObjectHandle private_key_id,
                                      uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateRsaKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToGenerateKey));
  }

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, public_key_id,
      {AttributeId::kModulus, AttributeId::kPublicExponent},
      base::BindOnce(&KcerTokenImpl::DidGetRsaPublicKey,
                     weak_factory_.GetWeakPtr(), std::move(task), public_key_id,
                     private_key_id));
}

// Computes PKCS#11 for the key and sets it.
void KcerTokenImpl::DidGetRsaPublicKey(
    GenerateRsaKeyTask task,
    ObjectHandle public_key_id,
    ObjectHandle private_key_id,
    chaps::AttributeList public_key_attributes,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateRsaKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToExportPublicKey));
  }

  base::span<const uint8_t> modulus =
      GetAttributeValue(public_key_attributes, AttributeId::kModulus);
  base::span<const uint8_t> public_exponent =
      GetAttributeValue(public_key_attributes, AttributeId::kPublicExponent);

  base::expected<PublicKey, Error> kcer_public_key =
      MakeRsaPublicKey(token_, modulus, public_exponent);
  if (!kcer_public_key.has_value()) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), kcer_public_key.error()));
  }

  chaps::AttributeList attr_list;
  AddAttribute(attr_list, chromeos::PKCS11_CKA_ID,
               kcer_public_key->GetPkcs11Id().value());

  auto chaps_callback =
      base::BindOnce(&KcerTokenImpl::DidAssignRsaKeyId,
                     weak_factory_.GetWeakPtr(), std::move(task), public_key_id,
                     private_key_id, std::move(kcer_public_key).value());
  chaps_client_->SetAttributeValue(pkcs_11_slot_id_,
                                   {public_key_id, private_key_id}, attr_list,
                                   std::move(chaps_callback));
}

void KcerTokenImpl::DidAssignRsaKeyId(GenerateRsaKeyTask task,
                                      ObjectHandle public_key_id,
                                      ObjectHandle private_key_id,
                                      PublicKey kcer_public_key,
                                      uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateRsaKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToWriteAttribute));
  }

  return std::move(task.callback).Run(std::move(kcer_public_key));
}

//==============================================================================

KcerTokenImpl::GenerateEcKeyTask::GenerateEcKeyTask(
    EllipticCurve in_curve,
    bool in_hardware_backed,
    Kcer::GenerateKeyCallback in_callback)
    : curve(in_curve),
      hardware_backed(in_hardware_backed),
      callback(std::move(in_callback)) {}
KcerTokenImpl::GenerateEcKeyTask::GenerateEcKeyTask(GenerateEcKeyTask&& other) =
    default;
KcerTokenImpl::GenerateEcKeyTask::~GenerateEcKeyTask() = default;

void KcerTokenImpl::GenerateEcKey(EllipticCurve curve,
                                  bool hardware_backed,
                                  Kcer::GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::GenerateEcKey, weak_factory_.GetWeakPtr(), curve,
        hardware_backed, std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  GenerateEcKeyImpl(GenerateEcKeyTask(curve, hardware_backed,
                                      std::move(unblocking_callback)));
}

// Generates an EC key pair.
void KcerTokenImpl::GenerateEcKeyImpl(GenerateEcKeyTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  if (task.curve != EllipticCurve::kP256) {
    return std::move(task.callback).Run(base::unexpected(Error::kBadKeyParams));
  }

  bssl::ScopedCBB cbb;
  uint8_t* ec_params_der = nullptr;
  size_t ec_params_der_len = 0;
  if (!CBB_init(cbb.get(), 0) ||
      !EC_KEY_marshal_curve_name(cbb.get(), EC_group_p256()) ||
      !CBB_finish(cbb.get(), &ec_params_der, &ec_params_der_len)) {
    return std::move(task.callback).Run(base::unexpected(Error::kBadKeyParams));
  }
  bssl::UniquePtr<uint8_t> der_deleter(ec_params_der);

  chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;

  chaps::AttributeList public_key_attrs;
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_ENCRYPT,
               MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_VERIFY, MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_WRAP, MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_EC_PARAMS,
               base::make_span(ec_params_der, ec_params_der_len));

  chaps::AttributeList private_key_attrs;
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_TOKEN, MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_PRIVATE,
               MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_SENSITIVE,
               MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_DECRYPT,
               MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_SIGN, MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_UNWRAP,
               MakeSpan(&kTrue));

  if (!task.hardware_backed) {
    AddAttribute(private_key_attrs, chaps::kForceSoftwareAttribute,
                 MakeSpan(&kTrue));
  }

  auto chaps_callback =
      base::BindOnce(&KcerTokenImpl::DidGenerateEcKey,
                     weak_factory_.GetWeakPtr(), std::move(task));
  chaps_client_->GenerateKeyPair(
      pkcs_11_slot_id_, chromeos::PKCS11_CKM_EC_KEY_PAIR_GEN,
      /*mechanism_parameter=*/{}, std::move(public_key_attrs),
      std::move(private_key_attrs), std::move(chaps_callback));
}

// Fetches the public key attributes of the generated key.
void KcerTokenImpl::DidGenerateEcKey(GenerateEcKeyTask task,
                                     ObjectHandle public_key_id,
                                     ObjectHandle private_key_id,
                                     uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateEcKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToGenerateKey));
  }

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, public_key_id, {AttributeId::kEcPoint},
      base::BindOnce(&KcerTokenImpl::DidGetEcPublicKey,
                     weak_factory_.GetWeakPtr(), std::move(task), public_key_id,
                     private_key_id));
}

// Computes PKCS#11 for the key and sets it.
void KcerTokenImpl::DidGetEcPublicKey(
    GenerateEcKeyTask task,
    ObjectHandle public_key_id,
    ObjectHandle private_key_id,
    chaps::AttributeList public_key_attributes,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateEcKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToExportPublicKey));
  }

  base::span<const uint8_t> wrapped_ec_point =
      GetAttributeValue(public_key_attributes, AttributeId::kEcPoint);
  bssl::UniquePtr<ASN1_OCTET_STRING> ec_point_oct =
      UnwrapEcPoint(wrapped_ec_point);
  if (!ec_point_oct) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToReadAttribute));
  }
  const uint8_t* ec_point_data = ASN1_STRING_data(ec_point_oct.get());
  size_t ec_point_data_len = ASN1_STRING_length(ec_point_oct.get());
  base::span<const uint8_t> ec_point =
      base::make_span(ec_point_data, ec_point_data_len);

  base::expected<PublicKey, Error> kcer_public_key =
      MakeEcPublicKey(token_, ec_point);
  if (!kcer_public_key.has_value()) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), kcer_public_key.error()));
  }

  chaps::AttributeList attr_list;
  AddAttribute(attr_list, chromeos::PKCS11_CKA_ID,
               kcer_public_key->GetPkcs11Id().value());

  auto chaps_callback =
      base::BindOnce(&KcerTokenImpl::DidAssignEcKeyId,
                     weak_factory_.GetWeakPtr(), std::move(task), public_key_id,
                     private_key_id, std::move(kcer_public_key).value());
  chaps_client_->SetAttributeValue(pkcs_11_slot_id_,
                                   {public_key_id, private_key_id}, attr_list,
                                   std::move(chaps_callback));
}

void KcerTokenImpl::DidAssignEcKeyId(GenerateEcKeyTask task,
                                     ObjectHandle public_key_id,
                                     ObjectHandle private_key_id,
                                     PublicKey kcer_public_key,
                                     uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateEcKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToWriteAttribute));
  }
  return std::move(task.callback).Run(std::move(kcer_public_key));
}

//==============================================================================

void KcerTokenImpl::ImportKey(Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                              Kcer::ImportKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::ImportKey, weak_factory_.GetWeakPtr(),
        std::move(pkcs8_private_key_info_der), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  const uint8_t* buffer = pkcs8_private_key_info_der->data();
  bssl::UniquePtr<PKCS8_PRIV_KEY_INFO> p8(d2i_PKCS8_PRIV_KEY_INFO(
      nullptr, &buffer, pkcs8_private_key_info_der->size()));
  if (!p8) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToParseKey));
  }

  KeyData key_data;
  key_data.key = bssl::UniquePtr<EVP_PKEY>(EVP_PKCS82PKEY(p8.get()));
  if (!key_data.key) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToParseKey));
  }

  Pkcs12Reader pkcs12_reader;
  Pkcs12ReaderStatusCode enrich_key_data_result =
      pkcs12_reader.EnrichKeyData(key_data);
  if (enrich_key_data_result != Pkcs12ReaderStatusCode::kSuccess) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  kcer_utils_.ImportKey(KcerTokenUtils::ImportKeyTask(
      std::move(key_data), /*in_hardware_backed=*/false,
      /*in_mark_as_migrated=*/false, std::move(unblocking_callback)));
}

//==============================================================================

KcerTokenImpl::ImportCertFromBytesTask::ImportCertFromBytesTask(
    CertDer in_cert_der,
    Kcer::StatusCallback in_callback)
    : cert_der(std::move(in_cert_der)), callback(std::move(in_callback)) {}
KcerTokenImpl::ImportCertFromBytesTask::ImportCertFromBytesTask(
    ImportCertFromBytesTask&& other) = default;
KcerTokenImpl::ImportCertFromBytesTask::~ImportCertFromBytesTask() = default;

void KcerTokenImpl::ImportCertFromBytes(CertDer cert_der,
                                        Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::ImportCertFromBytes, weak_factory_.GetWeakPtr(),
        std::move(cert_der), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  ImportCertFromBytesImpl(ImportCertFromBytesTask(
      std::move(cert_der), std::move(unblocking_callback)));
}

void KcerTokenImpl::ImportCertFromBytesImpl(ImportCertFromBytesTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_VALUE, task.cert_der.value());

  // Check whether the cert is already imported.
  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::ImportCertFromBytesWithExistingCerts,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Checks whether the private key for the cert exists.
void KcerTokenImpl::ImportCertFromBytesWithExistingCerts(
    ImportCertFromBytesTask task,
    std::vector<ObjectHandle> existing_certs,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ImportCertFromBytesImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }
  if (!existing_certs.empty()) {
    // The cert already exists, no need to import, return success.
    return std::move(task.callback).Run({});
  }

  std::string_view spki_string_piece;
  if (!net::asn1::ExtractSPKIFromDERCert(AsString(task.cert_der.value()),
                                         &spki_string_piece)) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kInvalidCertificate));
  }
  PublicKeySpki spki(std::vector<uint8_t>(
      spki_string_piece.data(),
      spki_string_piece.data() + spki_string_piece.size()));

  Pkcs11Id key_id = GetPkcs11IdFromSpki(spki);
  if (key_id->empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kInvalidCertificate));
  }

  Pkcs11Id key_id_copy = key_id;
  kcer_utils_.FindPrivateKey(
      std::move(key_id),
      base::BindOnce(&KcerTokenImpl::ImportCertFromBytesWithKeyHandle,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(key_id_copy)));
}

// Parses and imports the cert into Chaps.
void KcerTokenImpl::ImportCertFromBytesWithKeyHandle(
    ImportCertFromBytesTask task,
    Pkcs11Id pkcs11_id,
    std::vector<ObjectHandle> key_handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ImportCertFromBytesImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }
  if (key_handles.empty()) {
    return std::move(task.callback).Run(base::unexpected(Error::kKeyNotFound));
  }

  Pkcs12Reader reader;
  bssl::UniquePtr<X509> cert;
  Pkcs12ReaderStatusCode status = reader.GetCertFromDerData(
      task.cert_der.value().data(), task.cert_der.value().size(), cert);
  if (status != Pkcs12ReaderStatusCode::kSuccess) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kInvalidCertificate));
  }

  std::vector<scoped_refptr<const Cert>> existing_certs =
      cert_cache_.GetAllCerts();

  std::vector<std::string_view> existing_nicknames;
  existing_nicknames.reserve(existing_certs.size());
  for (const auto& existing_cert : existing_certs) {
    existing_nicknames.push_back(
        std::string_view(existing_cert->GetNickname()));
  }

  std::string label;
  Pkcs12ReaderStatusCode get_nickname_result =
      GetNickname(std::move(existing_certs), std::move(existing_nicknames),
                  cert.get(), reader, label);
  if (get_nickname_result != Pkcs12ReaderStatusCode::kSuccess) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToMakeCertNickname));
  }

  CertDer cert_der = task.cert_der;
  auto import_callback =
      base::BindOnce(&KcerTokenImpl::DidImportCertFromBytes,
                     weak_factory_.GetWeakPtr(), std::move(task));
  kcer_utils_.ImportCert(std::move(cert), std::move(pkcs11_id),
                         std::move(label), std::move(cert_der),
                         /*is_hardware_backed=*/true,
                         /*mark_as_migrated=*/false,
                         std::move(import_callback));
}

void KcerTokenImpl::DidImportCertFromBytes(ImportCertFromBytesTask task,
                                           std::optional<Error> kcer_error,
                                           ObjectHandle cert_handle,
                                           uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (kcer_error.has_value()) {
    return std::move(task.callback).Run(base::unexpected(kcer_error.value()));
  }
  if (SessionChapsClient::IsSessionError(result_code)) {
    return ImportCertFromBytesImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToImportCertificate));
  }
  return std::move(task.callback).Run({});
}

//==============================================================================

void KcerTokenImpl::ImportPkcs12Cert(Pkcs12Blob pkcs12_blob,
                                     std::string password,
                                     bool hardware_backed,
                                     bool mark_as_migrated,
                                     Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::ImportPkcs12Cert, weak_factory_.GetWeakPtr(),
        std::move(pkcs12_blob), std::move(password), hardware_backed,
        mark_as_migrated, std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  Pkcs12Reader pkcs12_reader;
  KeyData key_data;
  bssl::UniquePtr<STACK_OF(X509)> certs;
  Pkcs12ReaderStatusCode get_key_and_cert_status =
      pkcs12_reader.GetPkcs12KeyAndCerts(pkcs12_blob.value(), password,
                                         key_data.key, certs);
  if (get_key_and_cert_status != Pkcs12ReaderStatusCode::kSuccess) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(
            ConvertPkcs12ParsingError(get_key_and_cert_status)));
  }

  Pkcs12ReaderStatusCode enrich_key_data_result =
      pkcs12_reader.EnrichKeyData(key_data);
  if ((enrich_key_data_result != Pkcs12ReaderStatusCode::kSuccess) ||
      key_data.cka_id_value.empty()) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetKeyId));
  }

  std::vector<CertData> certs_data;
  Pkcs12ReaderStatusCode prepare_certs_status = ValidateAndPrepareCertData(
      cert_cache_, pkcs12_reader, std::move(certs), key_data, certs_data);
  if (prepare_certs_status == Pkcs12ReaderStatusCode::kAlreadyExists) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kAlreadyExists));
  }
  if ((prepare_certs_status != Pkcs12ReaderStatusCode::kSuccess) ||
      certs_data.empty()) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kInvalidPkcs12));
  }

  auto import_callback = base::BindOnce(&KcerTokenImpl::DidImportPkcs12Cert,
                                        weak_factory_.GetWeakPtr(),
                                        std::move(unblocking_callback));

  kcer_utils_.ImportPkcs12(std::move(key_data), std::move(certs_data),
                           hardware_backed, mark_as_migrated,
                           std::move(import_callback));
}

void KcerTokenImpl::DidImportPkcs12Cert(
    Kcer::StatusCallback callback,
    bool did_modify,
    base::expected<void, Error> import_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (did_modify) {
    return NotifyCertsChanged(
        base::BindOnce(std::move(callback), std::move(import_result)));
  }

  return std::move(callback).Run(std::move(import_result));
}

//==============================================================================

void KcerTokenImpl::ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                                     Kcer::ExportPkcs12Callback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

KcerTokenImpl::RemoveKeyAndCertsTask::RemoveKeyAndCertsTask(
    PrivateKeyHandle in_key,
    Kcer::StatusCallback in_callback)
    : key(std::move(in_key)), callback(std::move(in_callback)) {}
KcerTokenImpl::RemoveKeyAndCertsTask::RemoveKeyAndCertsTask(
    RemoveKeyAndCertsTask&& other) = default;
KcerTokenImpl::RemoveKeyAndCertsTask::~RemoveKeyAndCertsTask() = default;

void KcerTokenImpl::RemoveKeyAndCerts(PrivateKeyHandle key,
                                      Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::RemoveKeyAndCerts, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  RemoveKeyAndCertsImpl(
      RemoveKeyAndCertsTask(std::move(key), std::move(unblocking_callback)));
}

// Finds all objects related to the `task.key` by PKCS#11 id.
void KcerTokenImpl::RemoveKeyAndCertsImpl(RemoveKeyAndCertsTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_ID,
               task.key.GetPkcs11IdInternal().value());

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::RemoveKeyAndCertsWithObjectHandles,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Destroys all found objects.
void KcerTokenImpl::RemoveKeyAndCertsWithObjectHandles(
    RemoveKeyAndCertsTask task,
    std::vector<ObjectHandle> handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return RemoveKeyAndCertsImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }
  if (handles.empty()) {
    return std::move(task.callback).Run(base::unexpected(Error::kKeyNotFound));
  }

  chaps_client_->DestroyObjectsWithRetries(
      pkcs_11_slot_id_, std::move(handles),
      base::BindOnce(&KcerTokenImpl::DidRemoveKeyAndCerts,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Checks the result and notifies that some certs were changed.
void KcerTokenImpl::DidRemoveKeyAndCerts(RemoveKeyAndCertsTask task,
                                         uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::expected<void, Error> result;
  if (SessionChapsClient::IsSessionError(result_code)) {
    return RemoveKeyAndCertsImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    result = base::unexpected(Error::kFailedToRemoveObjects);
  }
  // Even if `DestroyObjectsWithRetries` fails, it might have removed at
  // least some objects, so notify about possible changes.
  NotifyCertsChanged(
      base::BindOnce(std::move(task.callback), std::move(result)));
}

//==============================================================================

KcerTokenImpl::RemoveCertTask::RemoveCertTask(scoped_refptr<const Cert> in_cert,
                                              Kcer::StatusCallback in_callback)
    : cert(std::move(in_cert)), callback(std::move(in_callback)) {}
KcerTokenImpl::RemoveCertTask::RemoveCertTask(RemoveCertTask&& other) = default;
KcerTokenImpl::RemoveCertTask::~RemoveCertTask() = default;

void KcerTokenImpl::RemoveCert(scoped_refptr<const Cert> cert,
                               Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(
        base::BindOnce(&KcerTokenImpl::RemoveCert, weak_factory_.GetWeakPtr(),
                       std::move(cert), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  RemoveCertImpl(
      RemoveCertTask(std::move(cert), std::move(unblocking_callback)));
}

// Searches for objects in Chaps containing the provided cert.
void KcerTokenImpl::RemoveCertImpl(RemoveCertTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  if (!task.cert || !task.cert->GetX509Cert()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToRemoveCertificate));
  }

  const CRYPTO_BUFFER* buffer = task.cert->GetX509Cert()->cert_buffer();
  base::span<const uint8_t> cert_der =
      base::make_span(CRYPTO_BUFFER_data(buffer), CRYPTO_BUFFER_len(buffer));

  CK_OBJECT_CLASS cert_class = CKO_CERTIFICATE;
  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_CLASS, MakeSpan(&cert_class));
  AddAttribute(attributes, chromeos::PKCS11_CKA_VALUE, cert_der);

  // Find all objects for the certificate. There should be at most one, but the
  // code can handle multiple.
  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::RemoveCertWithHandles,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Deletes all the found objects.
void KcerTokenImpl::RemoveCertWithHandles(RemoveCertTask task,
                                          std::vector<ObjectHandle> handles,
                                          uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return RemoveCertImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }
  if (handles.empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToRemoveCertificate));
  }

  chaps_client_->DestroyObjectsWithRetries(
      pkcs_11_slot_id_, std::move(handles),
      base::BindOnce(&KcerTokenImpl::DidRemoveCert, weak_factory_.GetWeakPtr(),
                     std::move(task)));
}

void KcerTokenImpl::DidRemoveCert(RemoveCertTask task, uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::expected<void, Error> result;
  if (SessionChapsClient::IsSessionError(result_code)) {
    return RemoveCertImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    result = base::unexpected(Error::kFailedToRemoveObjects);
  }
  // Even if `DestroyObjectsWithRetries` fails, it might have removed the cert,
  // so notify about possible changes.
  NotifyCertsChanged(
      base::BindOnce(std::move(task.callback), std::move(result)));
}

//==============================================================================

KcerTokenImpl::ListKeysTask::ListKeysTask(TokenListKeysCallback in_callback)
    : callback(std::move(in_callback)) {}
KcerTokenImpl::ListKeysTask::ListKeysTask(ListKeysTask&& other) = default;
KcerTokenImpl::ListKeysTask::~ListKeysTask() = default;

void KcerTokenImpl::ListKeys(TokenListKeysCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(&KcerTokenImpl::ListKeys,
                                                weak_factory_.GetWeakPtr(),
                                                std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  ListKeysImpl(ListKeysTask(std::move(unblocking_callback)));
}

// Starts by finding RSA key objects.
void KcerTokenImpl::ListKeysImpl(ListKeysTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  // For RSA keys the required attributes are stored in the private key objects.
  chromeos::PKCS11_CK_OBJECT_CLASS obj_class = chromeos::PKCS11_CKO_PRIVATE_KEY;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_RSA;
  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_CLASS, MakeSpan(&obj_class));
  AddAttribute(attributes, chromeos::PKCS11_CKA_KEY_TYPE, MakeSpan(&key_type));

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::ListKeysWithRsaHandles,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Starts iterating over the RSA keys.
void KcerTokenImpl::ListKeysWithRsaHandles(ListKeysTask task,
                                           std::vector<ObjectHandle> handles,
                                           uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ListKeysImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }

  ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                       std::vector<PublicKey>());
}

// This is called repeatedly until `handles` is empty.
void KcerTokenImpl::ListKeysGetOneRsaKey(ListKeysTask task,
                                         std::vector<ObjectHandle> handles,
                                         std::vector<PublicKey> result_keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (handles.empty()) {
    // All RSA keys are handled, now search for EC keys.
    return ListKeysFindEcKeys(std::move(task), std::move(result_keys));
  }

  ObjectHandle current_handle = handles.back();
  handles.pop_back();

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, current_handle,
      {AttributeId::kPkcs11Id, AttributeId::kModulus,
       AttributeId::kPublicExponent},
      base::BindOnce(&KcerTokenImpl::ListKeysDidGetOneRsaKey,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(handles), std::move(result_keys)));
}

// Receives attributes for a single RSA key and creates kcer::PublicKey from
// them.
void KcerTokenImpl::ListKeysDidGetOneRsaKey(ListKeysTask task,
                                            std::vector<ObjectHandle> handles,
                                            std::vector<PublicKey> result_keys,
                                            chaps::AttributeList attributes,
                                            uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ListKeysImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    // Try to get as many keys as possible even if some of them fail.
    return ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                                std::move(result_keys));
  }

  base::span<const uint8_t> pkcs11_id =
      GetAttributeValue(attributes, AttributeId::kPkcs11Id);
  base::span<const uint8_t> modulus =
      GetAttributeValue(attributes, AttributeId::kModulus);
  base::span<const uint8_t> public_exponent =
      GetAttributeValue(attributes, AttributeId::kPublicExponent);
  if (pkcs11_id.empty() || modulus.empty() || public_exponent.empty()) {
    LOG(WARNING) << "Invalid RSA key was fetched from Chaps, skipping it.";
    return ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                                std::move(result_keys));
  }

  PublicKeySpki spki = MakeRsaSpki(modulus, public_exponent);
  if (spki->empty()) {
    LOG(WARNING) << "Invalid RSA key was fetched from Chaps, skipping it.";
    return ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                                std::move(result_keys));
  }

  std::vector<uint8_t> id(pkcs11_id.begin(), pkcs11_id.end());
  result_keys.emplace_back(token_, Pkcs11Id(std::move(id)), std::move(spki));
  return ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                              std::move(result_keys));
}

// Finds EC key objects.
void KcerTokenImpl::ListKeysFindEcKeys(ListKeysTask task,
                                       std::vector<PublicKey> result_keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // For EC keys the required attributes are stored in the public key objects.
  chromeos::PKCS11_CK_OBJECT_CLASS obj_class = chromeos::PKCS11_CKO_PUBLIC_KEY;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_EC;
  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_CLASS, MakeSpan(&obj_class));
  AddAttribute(attributes, chromeos::PKCS11_CKA_KEY_TYPE, MakeSpan(&key_type));

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::ListKeysWithEcHandles,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(result_keys)));
}

// Starts iterating over the EC keys.
void KcerTokenImpl::ListKeysWithEcHandles(ListKeysTask task,
                                          std::vector<PublicKey> result_keys,
                                          std::vector<ObjectHandle> handles,
                                          uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ListKeysImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }

  ListKeysGetOneEcKey(std::move(task), std::move(handles),
                      std::move(result_keys));
}

// This is called repeatedly until `handles` is empty.
void KcerTokenImpl::ListKeysGetOneEcKey(ListKeysTask task,
                                        std::vector<ObjectHandle> handles,
                                        std::vector<PublicKey> result_keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (handles.empty()) {
    // All RSA and EC keys are handled, return the final result.
    return std::move(task.callback).Run(std::move(result_keys));
  }

  ObjectHandle current_handle = handles.back();
  handles.pop_back();

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, current_handle,
      {AttributeId::kPkcs11Id, AttributeId::kEcPoint},
      base::BindOnce(&KcerTokenImpl::ListKeysDidGetOneEcKey,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(handles), std::move(result_keys)));
}

// Receives attributes for a single EC key and creates kcer::PublicKey from
// them.
void KcerTokenImpl::ListKeysDidGetOneEcKey(ListKeysTask task,
                                           std::vector<ObjectHandle> handles,
                                           std::vector<PublicKey> result_keys,
                                           chaps::AttributeList attributes,
                                           uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ListKeysImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    // Try to get as many keys as possible even if some of them fail.
    return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                               std::move(result_keys));
  }

  base::span<const uint8_t> pkcs11_id =
      GetAttributeValue(attributes, AttributeId::kPkcs11Id);
  base::span<const uint8_t> wrapped_ec_point =
      GetAttributeValue(attributes, AttributeId::kEcPoint);
  if (pkcs11_id.empty() || wrapped_ec_point.empty()) {
    LOG(WARNING) << "Invalid EC key was fetched from Chaps, skipping it.";
    return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                               std::move(result_keys));
  }

  bssl::UniquePtr<ASN1_OCTET_STRING> ec_point_oct =
      UnwrapEcPoint(wrapped_ec_point);
  if (!ec_point_oct) {
    LOG(WARNING) << "Invalid EC key was fetched from Chaps, skipping it.";
    return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                               std::move(result_keys));
  }
  const uint8_t* ec_point_data = ASN1_STRING_data(ec_point_oct.get());
  size_t ec_point_data_len = ASN1_STRING_length(ec_point_oct.get());
  base::span<const uint8_t> ec_point =
      base::make_span(ec_point_data, ec_point_data_len);

  PublicKeySpki spki = MakeEcSpki(ec_point);
  if (spki->empty()) {
    LOG(WARNING) << "Invalid EC key was fetched from Chaps, skipping it.";
    return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                               std::move(result_keys));
  }

  std::vector<uint8_t> id(pkcs11_id.begin(), pkcs11_id.end());
  PublicKey public_key(token_, Pkcs11Id(std::move(id)), std::move(spki));

  chromeos::PKCS11_CK_OBJECT_CLASS obj_class = chromeos::PKCS11_CKO_PRIVATE_KEY;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_EC;
  chaps::AttributeList private_key_attributes;
  AddAttribute(private_key_attributes, chromeos::PKCS11_CKA_CLASS,
               MakeSpan(&obj_class));
  AddAttribute(private_key_attributes, chromeos::PKCS11_CKA_KEY_TYPE,
               MakeSpan(&key_type));
  AddAttribute(private_key_attributes, chromeos::PKCS11_CKA_ID,
               public_key.GetPkcs11Id().value());

  // Check that the private key for public key exists in Chaps. RSA keys don't
  // need this check because key attributes can be read from the RSA private key
  // objects.
  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(private_key_attributes),
      base::BindOnce(&KcerTokenImpl::ListKeysDidFindEcPrivateKey,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(handles), std::move(result_keys),
                     std::move(public_key)));
}

void KcerTokenImpl::ListKeysDidFindEcPrivateKey(
    ListKeysTask task,
    std::vector<ObjectHandle> handles,
    std::vector<PublicKey> result_keys,
    PublicKey current_public_key,
    std::vector<ObjectHandle> private_key_handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!private_key_handles.empty()) {
    result_keys.push_back(std::move(current_public_key));
  }

  return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                             std::move(result_keys));
}

//==============================================================================

void KcerTokenImpl::ListCerts(TokenListCertsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(&KcerTokenImpl::ListCerts,
                                                weak_factory_.GetWeakPtr(),
                                                std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  // Return current certs from the cache. It's expected to always be up-to-date
  // at this point. UnblockQueueProcessNextTask() will update the cache first
  // when necessary.
  return std::move(unblocking_callback).Run(cert_cache_.GetAllCerts());
}

//==============================================================================

KcerTokenImpl::DoesPrivateKeyExistTask::DoesPrivateKeyExistTask(
    PrivateKeyHandle in_key,
    Kcer::DoesKeyExistCallback in_callback)
    : key(std::move(in_key)), callback(std::move(in_callback)) {}
KcerTokenImpl::DoesPrivateKeyExistTask::DoesPrivateKeyExistTask(
    DoesPrivateKeyExistTask&& other) = default;
KcerTokenImpl::DoesPrivateKeyExistTask::~DoesPrivateKeyExistTask() = default;

void KcerTokenImpl::DoesPrivateKeyExist(PrivateKeyHandle key,
                                        Kcer::DoesKeyExistCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::DoesPrivateKeyExist, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  DoesPrivateKeyExistImpl(
      DoesPrivateKeyExistTask(std::move(key), std::move(unblocking_callback)));
}

// Searches for the Chaps handle for `task.key`.
void KcerTokenImpl::DoesPrivateKeyExistImpl(DoesPrivateKeyExistTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  chromeos::PKCS11_CK_OBJECT_CLASS priv_key_class =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  chaps::AttributeList private_key_attrs;
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_CLASS,
               MakeSpan(&priv_key_class));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_ID,
               task.key.GetPkcs11IdInternal().value());

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(private_key_attrs),
      base::BindOnce(&KcerTokenImpl::DidDoesPrivateKeyExist,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

void KcerTokenImpl::DidDoesPrivateKeyExist(
    DoesPrivateKeyExistTask task,
    std::vector<ObjectHandle> object_list,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return DoesPrivateKeyExistImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }

  return std::move(task.callback).Run(!object_list.empty());
}

//==============================================================================

KcerTokenImpl::SignTask::SignTask(PrivateKeyHandle in_key,
                                  SigningScheme in_signing_scheme,
                                  DataToSign in_data,
                                  Kcer::SignCallback in_callback)
    : key(std::move(in_key)),
      signing_scheme(in_signing_scheme),
      data(std::move(in_data)),
      callback(std::move(in_callback)) {}
KcerTokenImpl::SignTask::SignTask(SignTask&& other) = default;
KcerTokenImpl::SignTask::~SignTask() = default;

void KcerTokenImpl::Sign(PrivateKeyHandle key,
                         SigningScheme signing_scheme,
                         DataToSign data,
                         Kcer::SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::Sign, weak_factory_.GetWeakPtr(), std::move(key),
        signing_scheme, std::move(data), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  SignImpl(SignTask(std::move(key), signing_scheme, std::move(data),
                    std::move(unblocking_callback)));
}

// Finds the key.
void KcerTokenImpl::SignImpl(SignTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  Pkcs11Id key_id = task.key.GetPkcs11IdInternal();
  kcer_utils_.FindPrivateKey(
      std::move(key_id),
      base::BindOnce(&KcerTokenImpl::SignWithKeyHandle,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Digests the data.
void KcerTokenImpl::SignWithKeyHandle(SignTask task,
                                      std::vector<ObjectHandle> key_handles,
                                      uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SignImpl(std::move(task));
  }
  if ((result_code != chromeos::PKCS11_CKR_OK) || key_handles.empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }
  DCHECK_EQ(key_handles.size(), 1u);

  DataToSign data = task.data;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DigestOnWorkerThread, task.signing_scheme,
                     std::move(data)),
      base::BindOnce(&KcerTokenImpl::SignWithKeyHandleAndDigest,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     key_handles.front()));
}

// Signs the data.
void KcerTokenImpl::SignWithKeyHandleAndDigest(
    SignTask task,
    ObjectHandle key_handle,
    base::expected<DigestWithPrefix, Error> digest) {
  if (!digest.has_value()) {
    return std::move(task.callback).Run(base::unexpected(digest.error()));
  }

  uint64_t mechanism = SigningSchemeToPkcs11Mechanism(task.signing_scheme);
  std::vector<uint8_t> mechanism_params;

  if (mechanism == chromeos::PKCS11_CKM_RSA_PKCS_PSS) {
    mechanism_params = GetPssSignParams(task.signing_scheme);
  }

  auto chaps_callback = base::BindOnce(
      &KcerTokenImpl::DidSign, weak_factory_.GetWeakPtr(), std::move(task));

  chaps_client_->Sign(pkcs_11_slot_id_, mechanism, mechanism_params, key_handle,
                      std::move(digest).value().value(),
                      std::move(chaps_callback));
}

// Re-encodes the signature if needed.
void KcerTokenImpl::DidSign(SignTask task,
                            std::vector<uint8_t> signature,
                            uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SignImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback).Run(base::unexpected(Error::kFailedToSign));
  }

  // ECDSA signatures have to be reencoded.
  uint64_t mechanism = SigningSchemeToPkcs11Mechanism(task.signing_scheme);
  if (mechanism == chromeos::PKCS11_CKM_ECDSA) {
    base::expected<std::vector<uint8_t>, Error> reencoded_signature =
        ReencodeEcSignature(std::move(signature));
    if (!reencoded_signature.has_value()) {
      return std::move(task.callback)
          .Run(base::unexpected(reencoded_signature.error()));
    }
    signature = std::move(reencoded_signature).value();
  }

  return std::move(task.callback).Run(Signature(signature));
}

//==============================================================================

KcerTokenImpl::SignRsaPkcs1RawTask::SignRsaPkcs1RawTask(
    PrivateKeyHandle in_key,
    DigestWithPrefix in_digest_with_prefix,
    Kcer::SignCallback in_callback)
    : key(std::move(in_key)),
      digest_with_prefix(std::move(in_digest_with_prefix)),
      callback(std::move(in_callback)) {}
KcerTokenImpl::SignRsaPkcs1RawTask::SignRsaPkcs1RawTask(
    SignRsaPkcs1RawTask&& other) = default;
KcerTokenImpl::SignRsaPkcs1RawTask::~SignRsaPkcs1RawTask() = default;

void KcerTokenImpl::SignRsaPkcs1Raw(PrivateKeyHandle key,
                                    DigestWithPrefix digest_with_prefix,
                                    Kcer::SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::SignRsaPkcs1Raw, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(digest_with_prefix), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  SignRsaPkcs1RawImpl(SignRsaPkcs1RawTask(std::move(key),
                                          std::move(digest_with_prefix),
                                          std::move(unblocking_callback)));
}

// Finds the key.
void KcerTokenImpl::SignRsaPkcs1RawImpl(SignRsaPkcs1RawTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  Pkcs11Id key_id = task.key.GetPkcs11IdInternal();
  kcer_utils_.FindPrivateKey(
      std::move(key_id),
      base::BindOnce(&KcerTokenImpl::SignRsaPkcs1RawWithKeyHandle,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Sings the data.
void KcerTokenImpl::SignRsaPkcs1RawWithKeyHandle(
    SignRsaPkcs1RawTask task,
    std::vector<ObjectHandle> key_handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SignRsaPkcs1RawImpl(std::move(task));
  }
  if ((result_code != chromeos::PKCS11_CKR_OK) || key_handles.empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }
  DCHECK_EQ(key_handles.size(), 1u);
  ObjectHandle key_handle = key_handles.front();

  uint64_t mechanism =
      SigningSchemeToPkcs11Mechanism(SigningScheme::kRsaPkcs1Sha256);

  std::vector<uint8_t> digest = task.digest_with_prefix.value();
  auto chaps_callback =
      base::BindOnce(&KcerTokenImpl::DidSignRsaPkcs1Raw,
                     weak_factory_.GetWeakPtr(), std::move(task));

  chaps_client_->Sign(pkcs_11_slot_id_, mechanism,
                      /*mechanism_parameter=*/std::vector<uint8_t>(),
                      key_handle, std::move(digest), std::move(chaps_callback));
}

void KcerTokenImpl::DidSignRsaPkcs1Raw(SignRsaPkcs1RawTask task,
                                       std::vector<uint8_t> signature,
                                       uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SignRsaPkcs1RawImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback).Run(base::unexpected(Error::kFailedToSign));
  }

  return std::move(task.callback).Run(Signature(signature));
}

//==============================================================================

void KcerTokenImpl::GetTokenInfo(Kcer::GetTokenInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(&KcerTokenImpl::GetTokenInfo,
                                                weak_factory_.GetWeakPtr(),
                                                std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  TokenInfo result;
  result.pkcs11_id = pkcs_11_slot_id_.value();
  result.module_name = "Chaps";

  switch (token_) {
    case Token::kUser:
      result.token_name = "User Token";
      break;
    case Token::kDevice:
      result.token_name = "Device Token";
      break;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(unblocking_callback), std::move(result)));
}

//==============================================================================

void KcerTokenImpl::GetKeyAttributes(
    PrivateKeyHandle key,
    std::vector<HighLevelChapsClient::AttributeId> attribute_ids,
    GetKeyAttributesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  kcer_utils_.FindPrivateKey(
      key.GetPkcs11IdInternal(),
      base::BindOnce(&KcerTokenImpl::GetKeyAttributesWithKeyHandle,
                     weak_factory_.GetWeakPtr(), std::move(attribute_ids),
                     std::move(callback)));
}

void KcerTokenImpl::GetKeyAttributesWithKeyHandle(
    std::vector<HighLevelChapsClient::AttributeId> attribute_ids,
    GetKeyAttributesCallback callback,
    std::vector<ObjectHandle> private_key_handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return std::move(callback).Run(/*kcer_error=*/std::nullopt,
                                   chaps::AttributeList(), result_code);
  }
  if ((result_code != chromeos::PKCS11_CKR_OK) || private_key_handles.empty()) {
    return std::move(callback).Run(Error::kKeyNotFound, chaps::AttributeList(),
                                   result_code);
  }
  if (private_key_handles.size() != 1) {
    // This shouldn't happen.
    return std::move(callback).Run(Error::kUnexpectedFindResult,
                                   chaps::AttributeList(), result_code);
  }

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, private_key_handles.front(), std::move(attribute_ids),
      base::BindOnce(std::move(callback), /*kcer_error=*/std::nullopt));
}

//==============================================================================

KcerTokenImpl::GetKeyInfoTask::GetKeyInfoTask(
    PrivateKeyHandle in_key,
    Kcer::GetKeyInfoCallback in_callback)
    : key(std::move(in_key)), callback(std::move(in_callback)) {}
KcerTokenImpl::GetKeyInfoTask::GetKeyInfoTask(GetKeyInfoTask&& other) = default;
KcerTokenImpl::GetKeyInfoTask::~GetKeyInfoTask() = default;

void KcerTokenImpl::GetKeyInfo(PrivateKeyHandle key,
                               Kcer::GetKeyInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(
        base::BindOnce(&KcerTokenImpl::GetKeyInfo, weak_factory_.GetWeakPtr(),
                       std::move(key), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  GetKeyInfoImpl(
      GetKeyInfoTask(std::move(key), std::move(unblocking_callback)));
}

// If PSS support for the token is not known yet - query it, all keys implicitly
// inherit it. Otherwise proceed to retrieving actual key info.
void KcerTokenImpl::GetKeyInfoImpl(GetKeyInfoTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  if (!token_supports_pss_.has_value()) {
    return chaps_client_->GetMechanismList(
        pkcs_11_slot_id_,
        base::BindOnce(&KcerTokenImpl::GetKeyInfoWithMechanismList,
                       weak_factory_.GetWeakPtr(), std::move(task)));
  }

  GetKeyInfoGetAttributes(std::move(task));
}

// Receives and caches PSS support for the token.
void KcerTokenImpl::GetKeyInfoWithMechanismList(
    GetKeyInfoTask task,
    const std::vector<uint64_t>& mechanism_list,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GetKeyInfoImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToRetrieveMechanismList));
  }

  for (uint64_t mechanism : mechanism_list) {
    if (mechanism == chromeos::PKCS11_CKM_RSA_PKCS_PSS) {
      token_supports_pss_ = true;
      break;
    }
  }
  if (!token_supports_pss_.has_value()) {
    token_supports_pss_ = false;
  }

  GetKeyInfoGetAttributes(std::move(task));
}

void KcerTokenImpl::GetKeyInfoGetAttributes(GetKeyInfoTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrivateKeyHandle key = task.key;
  GetKeyAttributes(
      std::move(key),
      {AttributeId::kKeyInSoftware, AttributeId::kKeyType, AttributeId::kLabel},
      base::BindOnce(&KcerTokenImpl::GetKeyInfoWithAttributes,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Parses the attributes into the result struct and returns it.
void KcerTokenImpl::GetKeyInfoWithAttributes(GetKeyInfoTask task,
                                             std::optional<Error> kcer_error,
                                             chaps::AttributeList attributes,
                                             uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (kcer_error.has_value()) {
    return std::move(task.callback).Run(base::unexpected(kcer_error.value()));
  }
  if (SessionChapsClient::IsSessionError(result_code)) {
    return GetKeyInfoImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToReadAttribute));
  }

  KeyInfo key_info;

  bool is_ok = true;
  is_ok = is_ok && GetIsHardwareBacked(
                       GetAttribute(attributes, AttributeId::kKeyInSoftware),
                       key_info.is_hardware_backed);
  is_ok = is_ok && GetKeyType(GetAttribute(attributes, AttributeId::kKeyType),
                              key_info.key_type);
  is_ok =
      is_ok && GetOptionalString(GetAttribute(attributes, AttributeId::kLabel),
                                 key_info.nickname);

  if (!is_ok) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToDecodeKeyAttributes));
  }

  CHECK(token_supports_pss_.has_value());
  key_info.supported_signing_schemes = GetSupportedSigningSchemes(
      token_supports_pss_.value(), key_info.key_type);

  return std::move(task.callback).Run(std::move(key_info));
}

//==============================================================================

KcerTokenImpl::GetKeyPermissionsTask::GetKeyPermissionsTask(
    PrivateKeyHandle in_key,
    Kcer::GetKeyPermissionsCallback in_callback)
    : key(std::move(in_key)), callback(std::move(in_callback)) {}
KcerTokenImpl::GetKeyPermissionsTask::GetKeyPermissionsTask(
    GetKeyPermissionsTask&& other) = default;
KcerTokenImpl::GetKeyPermissionsTask::~GetKeyPermissionsTask() = default;

void KcerTokenImpl::GetKeyPermissions(
    PrivateKeyHandle key,
    Kcer::GetKeyPermissionsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::GetKeyPermissions, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  GetKeyPermissionsImpl(
      GetKeyPermissionsTask(std::move(key), std::move(unblocking_callback)));
}

void KcerTokenImpl::GetKeyPermissionsImpl(GetKeyPermissionsTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  PrivateKeyHandle key = task.key;
  GetKeyAttributes(
      std::move(key), {AttributeId::kKeyPermissions},
      base::BindOnce(&KcerTokenImpl::GetKeyPermissionsWithAttributes,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

void KcerTokenImpl::GetKeyPermissionsWithAttributes(
    GetKeyPermissionsTask task,
    std::optional<Error> kcer_error,
    chaps::AttributeList attributes,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (kcer_error.has_value()) {
    return std::move(task.callback).Run(base::unexpected(kcer_error.value()));
  }
  if (SessionChapsClient::IsSessionError(result_code)) {
    return GetKeyPermissionsImpl(std::move(task));
  }
  if (result_code == chromeos::PKCS11_CKR_ATTRIBUTE_TYPE_INVALID) {
    // Key permissions were never set on this key.
    return std::move(task.callback).Run(std::nullopt);
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToReadAttribute));
  }
  if (attributes.attributes_size() != 1) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToDecodeKeyAttributes));
  }
  const chaps::Attribute& attr = attributes.attributes(0);
  if ((attr.type() != static_cast<uint32_t>(AttributeId::kKeyPermissions)) ||
      !attr.has_value() || attr.value().empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToDecodeKeyAttributes));
  }

  chaps::KeyPermissions key_permissions;
  if (!key_permissions.ParseFromArray(attr.value().data(),
                                      attr.value().size())) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToDecodeKeyAttributes));
  }
  return std::move(task.callback).Run(std::move(key_permissions));
}

//==============================================================================

KcerTokenImpl::GetCertProvisioningIdTask::GetCertProvisioningIdTask(
    PrivateKeyHandle in_key,
    Kcer::GetCertProvisioningProfileIdCallback in_callback)
    : key(std::move(in_key)), callback(std::move(in_callback)) {}
KcerTokenImpl::GetCertProvisioningIdTask::GetCertProvisioningIdTask(
    GetCertProvisioningIdTask&& other) = default;
KcerTokenImpl::GetCertProvisioningIdTask::~GetCertProvisioningIdTask() =
    default;

void KcerTokenImpl::GetCertProvisioningProfileId(
    PrivateKeyHandle key,
    Kcer::GetCertProvisioningProfileIdCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::GetCertProvisioningProfileId,
        weak_factory_.GetWeakPtr(), std::move(key), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  GetCertProvisioningIdImpl(GetCertProvisioningIdTask(
      std::move(key), std::move(unblocking_callback)));
}

void KcerTokenImpl::GetCertProvisioningIdImpl(GetCertProvisioningIdTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  PrivateKeyHandle key = task.key;
  GetKeyAttributes(
      std::move(key), {AttributeId::kCertProvisioningId},
      base::BindOnce(&KcerTokenImpl::GetCertProvisioningIdWithAttributes,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

void KcerTokenImpl::GetCertProvisioningIdWithAttributes(
    GetCertProvisioningIdTask task,
    std::optional<Error> kcer_error,
    chaps::AttributeList attributes,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (kcer_error.has_value()) {
    return std::move(task.callback).Run(base::unexpected(kcer_error.value()));
  }
  if (SessionChapsClient::IsSessionError(result_code)) {
    return GetCertProvisioningIdImpl(std::move(task));
  }
  if (result_code == chromeos::PKCS11_CKR_ATTRIBUTE_TYPE_INVALID) {
    // Cert provisioning profile id was never set on this key.
    return std::move(task.callback).Run(std::nullopt);
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToReadAttribute));
  }
  if (attributes.attributes_size() != 1) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToDecodeKeyAttributes));
  }
  const chaps::Attribute& attr = attributes.attributes(0);
  if ((attr.type() !=
       static_cast<uint32_t>(AttributeId::kCertProvisioningId)) ||
      !attr.has_value() || attr.value().empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToDecodeKeyAttributes));
  }
  return std::move(task.callback).Run(attr.value());
}

//==============================================================================

KcerTokenImpl::SetKeyAttributeTask::SetKeyAttributeTask(
    PrivateKeyHandle in_key,
    HighLevelChapsClient::AttributeId in_attribute_id,
    std::vector<uint8_t> in_attribute_value,
    Kcer::StatusCallback in_callback)
    : key(std::move(in_key)),
      attribute_id(in_attribute_id),
      attribute_value(std::move(in_attribute_value)),
      callback(std::move(in_callback)) {}
KcerTokenImpl::SetKeyAttributeTask::SetKeyAttributeTask(
    SetKeyAttributeTask&& other) = default;
KcerTokenImpl::SetKeyAttributeTask::~SetKeyAttributeTask() = default;

void KcerTokenImpl::SetKeyAttribute(
    PrivateKeyHandle key,
    HighLevelChapsClient::AttributeId attribute_id,
    std::vector<uint8_t> attribute_value,
    Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  SetKeyAttributeImpl(SetKeyAttributeTask(std::move(key), attribute_id,
                                          std::move(attribute_value),
                                          std::move(callback)));
}

// Finds the private key that will store the attribute.
void KcerTokenImpl::SetKeyAttributeImpl(SetKeyAttributeTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  chromeos::PKCS11_CK_OBJECT_CLASS obj_class = chromeos::PKCS11_CKO_PRIVATE_KEY;
  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_CLASS, MakeSpan(&obj_class));
  AddAttribute(attributes, chromeos::PKCS11_CKA_ID,
               task.key.GetPkcs11IdInternal().value());

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::SetKeyAttributeWithHandle,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Set attribute on the key.
void KcerTokenImpl::SetKeyAttributeWithHandle(
    SetKeyAttributeTask task,
    std::vector<ObjectHandle> private_key_handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SetKeyAttributeImpl(std::move(task));
  }
  if ((result_code != chromeos::PKCS11_CKR_OK) || private_key_handles.empty()) {
    return std::move(task.callback).Run(base::unexpected(Error::kKeyNotFound));
  }
  if (private_key_handles.size() != 1) {
    // This shouldn't happen.
    return std::move(task.callback)
        .Run(base::unexpected(Error::kUnexpectedFindResult));
  }

  chaps::AttributeList attributes;
  AddAttribute(attributes, static_cast<uint32_t>(task.attribute_id),
               task.attribute_value);

  chaps_client_->SetAttributeValue(
      pkcs_11_slot_id_, private_key_handles.front(), attributes,
      base::BindOnce(&KcerTokenImpl::SetKeyAttributeDidSetAttribute,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

void KcerTokenImpl::SetKeyAttributeDidSetAttribute(SetKeyAttributeTask task,
                                                   uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SetKeyAttributeImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToWriteAttribute));
  }
  return std::move(task.callback).Run({});
}

//==============================================================================

void KcerTokenImpl::SetKeyNickname(PrivateKeyHandle key,
                                   std::string nickname,
                                   Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::SetKeyNickname, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(nickname), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  return SetKeyAttribute(std::move(key),
                         HighLevelChapsClient::AttributeId::kLabel,
                         std::vector<uint8_t>(nickname.begin(), nickname.end()),
                         std::move(unblocking_callback));
}

//==============================================================================

void KcerTokenImpl::SetKeyPermissions(PrivateKeyHandle key,
                                      chaps::KeyPermissions key_permissions,
                                      Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::SetKeyPermissions, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(key_permissions), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  std::vector<uint8_t> serialized_permissions;
  serialized_permissions.resize(key_permissions.ByteSizeLong());
  key_permissions.SerializeToArray(serialized_permissions.data(),
                                   serialized_permissions.size());

  return SetKeyAttribute(
      std::move(key), HighLevelChapsClient::AttributeId::kKeyPermissions,
      serialized_permissions, std::move(unblocking_callback));
}

//==============================================================================

void KcerTokenImpl::SetCertProvisioningProfileId(
    PrivateKeyHandle key,
    std::string profile_id,
    Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(
        base::BindOnce(&KcerTokenImpl::SetCertProvisioningProfileId,
                       weak_factory_.GetWeakPtr(), std::move(key),
                       std::move(profile_id), std::move(callback)));
  }
  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  return SetKeyAttribute(
      std::move(key), HighLevelChapsClient::AttributeId::kCertProvisioningId,
      std::vector<uint8_t>(profile_id.begin(), profile_id.end()),
      std::move(unblocking_callback));
}

//==============================================================================

KcerTokenImpl::UpdateCacheTask::UpdateCacheTask(base::OnceClosure in_callback)
    : callback(std::move(in_callback)) {}
KcerTokenImpl::UpdateCacheTask::UpdateCacheTask(UpdateCacheTask&& other) =
    default;
KcerTokenImpl::UpdateCacheTask::~UpdateCacheTask() = default;

void KcerTokenImpl::UpdateCache() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(&KcerTokenImpl::UpdateCache,
                                                weak_factory_.GetWeakPtr()));
  }
  // Block task queue, attach unblocking task to the DoNothing closure.
  auto unblocking_callback =
      BlockQueueGetUnblocker(base::OnceClosure(base::DoNothing()));

  UpdateCacheImpl(UpdateCacheTask(std::move(unblocking_callback)));
}

// Finds all certificate objects in Chaps.
void KcerTokenImpl::UpdateCacheImpl(UpdateCacheTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return UpdateCacheWithCerts(std::move(task),
                                base::unexpected(Error::kPkcs11SessionFailure));
  }

  cache_state_ = CacheState::kUpdating;

  chromeos::PKCS11_CK_OBJECT_CLASS cert_class =
      chromeos::PKCS11_CKO_CERTIFICATE;
  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_CLASS, MakeSpan(&cert_class));

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::UpdateCacheWithCertHandles,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

void KcerTokenImpl::UpdateCacheWithCertHandles(
    UpdateCacheTask task,
    std::vector<ObjectHandle> handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (cache_state_ == CacheState::kOutdated) {
    // If the status switched from kUpdating, then a new update happened since
    // the cache started to update, `handles` might already be outdated.
    // Skip re-building the cache and try again by unblocking the queue and
    // returning to UnblockQueueProcessNextTask().
    return std::move(task.callback).Run();
  }
  if (SessionChapsClient::IsSessionError(result_code)) {
    return UpdateCacheImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return UpdateCacheWithCerts(
        std::move(task), base::unexpected(Error::kFailedToSearchForObjects));
  }

  UpdateCacheGetOneCert(std::move(task), std::move(handles),
                        std::vector<scoped_refptr<const Cert>>());
}

// This is called repeatedly until `handles` is empty.
void KcerTokenImpl::UpdateCacheGetOneCert(
    UpdateCacheTask task,
    std::vector<ObjectHandle> handles,
    std::vector<scoped_refptr<const Cert>> certs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (handles.empty()) {
    return UpdateCacheWithCerts(std::move(task), std::move(certs));
  }

  ObjectHandle current_handle = handles.back();
  handles.pop_back();

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, current_handle,
      {AttributeId::kPkcs11Id, AttributeId::kLabel, AttributeId::kValue},
      base::BindOnce(&KcerTokenImpl::UpdateCacheDidGetOneCert,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(handles), std::move(certs)));
}

// Parses attributes of a single cert and adds the new object to `certs`.
void KcerTokenImpl::UpdateCacheDidGetOneCert(
    UpdateCacheTask task,
    std::vector<ObjectHandle> handles,
    std::vector<scoped_refptr<const Cert>> certs,
    chaps::AttributeList attributes,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (cache_state_ == CacheState::kOutdated) {
    // If the status switched from kUpdating, then new update happened since
    // the cache started to update, `certs` might already be outdated.
    // Skip re-building the cache and try again by unblocking the queue and
    // returning to UnblockQueueProcessNextTask().
    return std::move(task.callback).Run();
  }
  if (SessionChapsClient::IsSessionError(result_code)) {
    return UpdateCacheImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    LOG(WARNING) << "Failed to get attributes for a cert, skipping it";
    // Try to get as many certs as possible even if some of them fail.
    return UpdateCacheGetOneCert(std::move(task), std::move(handles),
                                 std::move(certs));
  }

  base::span<const uint8_t> pkcs11_id =
      GetAttributeValue(attributes, AttributeId::kPkcs11Id);
  base::span<const uint8_t> nickname =
      GetAttributeValue(attributes, AttributeId::kLabel);
  base::span<const uint8_t> cert_der =
      GetAttributeValue(attributes, AttributeId::kValue);
  if (pkcs11_id.empty() || cert_der.empty()) {
    LOG(WARNING) << "Invalid cert was fetched from Chaps, skipping it";
    return UpdateCacheGetOneCert(std::move(task), std::move(handles),
                                 std::move(certs));
  }

  scoped_refptr<const Cert> existing_cert = cert_cache_.FindCert(cert_der);
  if (existing_cert) {
    certs.push_back(std::move(existing_cert));
    return UpdateCacheGetOneCert(std::move(task), std::move(handles),
                                 std::move(certs));
  }

  scoped_refptr<net::X509Certificate> x509_cert =
      net::X509Certificate::CreateFromBytes(cert_der);
  if (!x509_cert) {
    LOG(WARNING) << "Failed to parse a cert from Chaps, skipping it";
    return UpdateCacheGetOneCert(std::move(task), std::move(handles),
                                 std::move(certs));
  }

  std::vector<uint8_t> id(pkcs11_id.begin(), pkcs11_id.end());
  certs.push_back(base::MakeRefCounted<Cert>(
      token_, Pkcs11Id(std::move(id)),
      std::string(nickname.begin(), nickname.end()), std::move(x509_cert)));
  return UpdateCacheGetOneCert(std::move(task), std::move(handles),
                               std::move(certs));
}

void KcerTokenImpl::UpdateCacheWithCerts(
    UpdateCacheTask task,
    base::expected<std::vector<scoped_refptr<const Cert>>, Error> certs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (certs.has_value()) {
    cert_cache_ = CertCache(std::move(certs).value());
  } else {
    LOG(ERROR) << "Failed to update cert cache, error: "
               << static_cast<uint32_t>(certs.error());
  }
  // Even if the update failed, mark it as complete to avoid an infinite loop in
  // case of persistent errors.
  cache_state_ = CacheState::kUpToDate;

  return std::move(task.callback).Run();
}

//==============================================================================

void KcerTokenImpl::NotifyCertsChanged(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  // The Notify... above will post a task to invalidate the cache. Calling the
  // original callback for a request will automatically trigger updating cache
  // and executing the next request. Post a task with the original callback
  // (instead of calling it synchronously), so the cache update and the next
  // request happen after the notification.
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
}

template <typename... Args>
void RunUnblockerAndCallback(base::ScopedClosureRunner unblocker,
                             base::OnceCallback<void(Args...)> callback,
                             Args... args) {
  unblocker.RunAndReset();
  std::move(callback).Run(args...);
}

template <typename... Args>
base::OnceCallback<void(Args...)> KcerTokenImpl::BlockQueueGetUnblocker(
    base::OnceCallback<void(Args...)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CHECK(!is_blocked_);
  is_blocked_ = true;

  // `unblocker` is executed either manually or on destruction.
  base::ScopedClosureRunner unblocker(base::BindOnce(
      &KcerTokenImpl::UnblockQueueProcessNextTask, weak_factory_.GetWeakPtr()));
  return base::BindOnce(&RunUnblockerAndCallback<Args...>, std::move(unblocker),
                        std::move(callback));
}

void KcerTokenImpl::UnblockQueueProcessNextTask() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_blocked_ = false;

  if (cache_state_ == CacheState::kOutdated) {
    return UpdateCache();
  }

  if (task_queue_.empty()) {
    return;
  }

  base::OnceClosure next_task = std::move(task_queue_.front());
  task_queue_.pop_front();
  std::move(next_task).Run();
}

}  // namespace kcer::internal
