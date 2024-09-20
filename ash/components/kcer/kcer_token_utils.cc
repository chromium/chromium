// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/kcer/kcer_token_utils.h"

#include "ash/components/kcer/helpers/key_helper.h"
#include "ash/components/kcer/kcer_histograms.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/openssl_util.h"

namespace kcer::internal {
namespace {

// A helper method for error handling. When some method fails and should return
// the `error` through the `callback`, but also should clean up something first,
// this helper allows to bind the error to the callback and create a new
// callback for the clean up code.
template <typename T>
base::OnceCallback<void(uint32_t)> Bind(
    base::OnceCallback<void(base::expected<T, Error>)> callback,
    Error error) {
  return base::IgnoreArgs<uint32_t>(
      base::BindOnce(std::move(callback), base::unexpected(error)));
}

void RecordUmaImportSuccess(KeyType key_type, bool is_multiple_cert) {
  if (key_type == kcer::KeyType::kRsa) {
    RecordKcerPkcs12ImportUmaEvent(
        kcer::internal::KcerPkcs12ImportEvent::SuccessRsaCertImportTask);
  } else {
    RecordKcerPkcs12ImportUmaEvent(
        kcer::internal::KcerPkcs12ImportEvent::SuccessEcCertImportTask);
  }

  RecordKcerPkcs12ImportUmaEvent(
      kcer::internal::KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport);
  if (is_multiple_cert) {
    RecordKcerPkcs12ImportUmaEvent(
        kcer::internal::KcerPkcs12ImportEvent::SuccessMultipleCertImport);
  }
}

}  // namespace

//==============================================================================

PublicKeySpki MakeRsaSpki(const base::span<const uint8_t>& modulus,
                          const base::span<const uint8_t>& exponent) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<BIGNUM> modulus_bignum(
      BN_bin2bn(modulus.data(), modulus.size(), nullptr));
  bssl::UniquePtr<BIGNUM> exponent_bignum(
      BN_bin2bn(exponent.data(), exponent.size(), nullptr));
  if (!modulus_bignum || !exponent_bignum) {
    return {};
  }

  bssl::UniquePtr<RSA> rsa(
      RSA_new_public_key(modulus_bignum.get(), exponent_bignum.get()));
  if (!rsa) {
    return {};
  }

  bssl::UniquePtr<EVP_PKEY> ssl_public_key(EVP_PKEY_new());
  if (!ssl_public_key || !EVP_PKEY_set1_RSA(ssl_public_key.get(), rsa.get())) {
    return {};
  }

  bssl::ScopedCBB cbb;
  uint8_t* der = nullptr;
  size_t der_len = 0;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_public_key(cbb.get(), ssl_public_key.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return {};
  }
  bssl::UniquePtr<uint8_t> der_deleter(der);

  return PublicKeySpki(std::vector<uint8_t>(der, der + der_len));
}

PublicKeySpki MakeEcSpki(const base::span<const uint8_t>& ec_point) {
  bssl::UniquePtr<EC_KEY> ec(EC_KEY_new());
  if (!ec) {
    return {};
  }

  if (!EC_KEY_set_group(ec.get(), EC_group_p256())) {
    return {};
  }

  EC_KEY* ec_ptr = ec.get();
  const uint8_t* data_2 = ec_point.data();
  size_t data_2_len = ec_point.size();
  if (!o2i_ECPublicKey(&ec_ptr, &data_2, data_2_len)) {
    return {};
  }

  bssl::UniquePtr<EVP_PKEY> ssl_public_key(EVP_PKEY_new());
  if (!ssl_public_key ||
      !EVP_PKEY_set1_EC_KEY(ssl_public_key.get(), ec.get())) {
    return {};
  }

  bssl::ScopedCBB cbb;
  uint8_t* der = nullptr;
  size_t der_len = 0;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_public_key(cbb.get(), ssl_public_key.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return {};
  }
  bssl::UniquePtr<uint8_t> der_deleter(der);

  return PublicKeySpki(std::vector<uint8_t>(der, der + der_len));
}

Pkcs11Id MakePkcs11Id(base::span<const uint8_t> public_key_data) {
  if (public_key_data.size() <= base::kSHA1Length) {
    return Pkcs11Id(
        std::vector<uint8_t>(public_key_data.begin(), public_key_data.end()));
  }

  base::SHA1Digest hash = base::SHA1Hash(public_key_data);
  return Pkcs11Id(std::vector<uint8_t>(hash.begin(), hash.end()));
}

base::expected<PublicKey, Error> MakeRsaPublicKey(
    Token token,
    base::span<const uint8_t> modulus,
    base::span<const uint8_t> public_exponent) {
  if (modulus.empty() || public_exponent.empty()) {
    return base::unexpected(Error::kFailedToReadAttribute);
  }

  PublicKeySpki spki = MakeRsaSpki(modulus, public_exponent);
  if (spki->empty()) {
    return base::unexpected(Error::kFailedToCreateSpki);
  }

  Pkcs11Id pkcs11_id = MakePkcs11Id(modulus);
  if (pkcs11_id->empty()) {
    return base::unexpected(Error::kFailedToGetPkcs11Id);
  }

  return PublicKey(token, pkcs11_id, std::move(spki));
}

base::expected<PublicKey, Error> MakeEcPublicKey(
    Token token,
    base::span<const uint8_t> ec_point) {
  if (ec_point.empty()) {
    return base::unexpected(Error::kFailedToReadAttribute);
  }

  PublicKeySpki spki = MakeEcSpki(ec_point);
  if (spki->empty()) {
    return base::unexpected(Error::kFailedToCreateSpki);
  }

  Pkcs11Id pkcs11_id = MakePkcs11Id(ec_point);
  if (pkcs11_id->empty()) {
    return base::unexpected(Error::kFailedToGetPkcs11Id);
  }

  return PublicKey(token, pkcs11_id, std::move(spki));
}

//==============================================================================

KcerTokenUtils::KcerTokenUtils(Token token, HighLevelChapsClient* chaps_client)
    : token_(token), chaps_client_(chaps_client) {}
KcerTokenUtils::~KcerTokenUtils() = default;

void KcerTokenUtils::Initialize(SessionChapsClient::SlotId pkcs_11_slot_id) {
  pkcs_11_slot_id_ = pkcs_11_slot_id;
}

void KcerTokenUtils::FindPrivateKey(
    Pkcs11Id id,
    base::OnceCallback<void(std::vector<ObjectHandle>, uint32_t)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const chromeos::PKCS11_CK_OBJECT_CLASS kPrivKeyClass =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  chaps::AttributeList private_key_attrs;
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_CLASS,
               MakeSpan(&kPrivKeyClass));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_ID, id.value());

  chaps_client_->FindObjects(pkcs_11_slot_id_, std::move(private_key_attrs),
                             std::move(callback));
}

//==============================================================================

void KcerTokenUtils::ImportCert(
    const bssl::UniquePtr<X509>& cert,
    const Pkcs11Id& pkcs11_id,
    const std::string& nickname,
    const CertDer& cert_der,
    bool is_hardware_backed,
    bool mark_as_migrated,
    base::OnceCallback<void(std::optional<Error> kcer_error,
                            ObjectHandle cert_handle,
                            uint32_t result_code)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Pkcs12Reader reader;

  base::span<const uint8_t> issuer_name_der;
  Pkcs12ReaderStatusCode status =
      reader.GetIssuerNameDer(cert.get(), issuer_name_der);
  if (status != Pkcs12ReaderStatusCode::kSuccess) {
    return std::move(callback).Run(Error::kFailedToGetIssuerName,
                                   ObjectHandle(0),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR);
  }

  base::span<const uint8_t> subject_name_der;
  status = reader.GetSubjectNameDer(cert.get(), subject_name_der);
  if (status != Pkcs12ReaderStatusCode::kSuccess) {
    return std::move(callback).Run(Error::kFailedToGetSubjectName,
                                   ObjectHandle(0),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR);
  }

  bssl::UniquePtr<uint8_t> serial_number_der;
  int serial_number_der_size = 0;
  status = reader.GetSerialNumberDer(cert.get(), serial_number_der,
                                     serial_number_der_size);
  if (status != Pkcs12ReaderStatusCode::kSuccess) {
    return std::move(callback).Run(Error::kFailedToGetSerialNumber,
                                   ObjectHandle(0),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR);
  }

  if (issuer_name_der.empty() || subject_name_der.empty() ||
      !serial_number_der || (serial_number_der_size <= 0)) {
    return std::move(callback).Run(Error::kInvalidCertificate, ObjectHandle(0),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR);
  }

  chromeos::PKCS11_CK_OBJECT_CLASS cert_class =
      chromeos::PKCS11_CKO_CERTIFICATE;
  chromeos::PKCS11_CK_CERTIFICATE_TYPE cert_type = chromeos::PKCS11_CKC_X_509;
  chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;

  chaps::AttributeList cert_attrs;
  AddAttribute(cert_attrs, chromeos::PKCS11_CKA_CLASS, MakeSpan(&cert_class));
  AddAttribute(cert_attrs, chromeos::PKCS11_CKA_CERTIFICATE_TYPE,
               MakeSpan(&cert_type));
  AddAttribute(cert_attrs, chromeos::PKCS11_CKA_TOKEN, MakeSpan(&kTrue));
  AddAttribute(cert_attrs, chromeos::PKCS11_CKA_ID, pkcs11_id.value());
  AddAttribute(cert_attrs, chromeos::PKCS11_CKA_LABEL,
               base::as_byte_span(nickname));
  AddAttribute(cert_attrs, chromeos::PKCS11_CKA_VALUE, cert_der.value());
  AddAttribute(cert_attrs, chromeos::PKCS11_CKA_ISSUER, issuer_name_der);
  AddAttribute(cert_attrs, chromeos::PKCS11_CKA_SUBJECT, subject_name_der);
  AddAttribute(
      cert_attrs, chromeos::PKCS11_CKA_SERIAL_NUMBER,
      base::make_span(serial_number_der.get(), size_t(serial_number_der_size)));
  if (!is_hardware_backed) {
    AddAttribute(cert_attrs, chaps::kForceSoftwareAttribute, MakeSpan(&kTrue));
  }
  if (mark_as_migrated) {
    AddAttribute(cert_attrs,
                 pkcs11_custom_attributes::kCkaChromeOsMigratedFromNss,
                 MakeSpan(&kTrue));
  }

  chaps_client_->CreateObject(
      pkcs_11_slot_id_, cert_attrs,
      base::BindOnce(std::move(callback), /*kcer_error*/ std::nullopt));
}

//==============================================================================

KcerTokenUtils::ImportKeyTask::ImportKeyTask(
    KeyData in_key_data,
    bool in_hardware_backed,
    bool in_mark_as_migrated,
    Kcer::GenerateKeyCallback in_callback)
    : key_data(std::move(in_key_data)),
      hardware_backed(in_hardware_backed),
      mark_as_migrated(in_mark_as_migrated),
      callback(std::move(in_callback)) {}
KcerTokenUtils::ImportKeyTask::ImportKeyTask(ImportKeyTask&& other) = default;
KcerTokenUtils::ImportKeyTask::~ImportKeyTask() = default;

void KcerTokenUtils::ImportKey(ImportKeyTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  if (IsKeyRsaType(task.key_data.key)) {
    ImportRsaKey(std::move(task));
  } else if (IsKeyEcType(task.key_data.key)) {
    ImportEcKey(std::move(task));
  } else {
    return std::move(task.callback).Run(base::unexpected(Error::kNotSupported));
  }
}

void KcerTokenUtils::ImportRsaKey(ImportKeyTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bssl::UniquePtr<RSA> rsa_key(EVP_PKEY_get1_RSA(task.key_data.key.get()));
  if (!rsa_key) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToParseKey));
  }

  Pkcs12Reader pkcs12_reader;
  std::vector<uint8_t> public_modulus_bytes =
      pkcs12_reader.BignumToBytes(RSA_get0_n(rsa_key.get()));
  std::vector<uint8_t> public_exponent_bytes =
      pkcs12_reader.BignumToBytes(RSA_get0_e(rsa_key.get()));

  base::expected<PublicKey, Error> kcer_public_key =
      MakeRsaPublicKey(token_, public_modulus_bytes, public_exponent_bytes);
  if (!kcer_public_key.has_value()) {
    return std::move(task.callback)
        .Run(base::unexpected(kcer_public_key.error()));
  }

  Pkcs11Id key_id = kcer_public_key->GetPkcs11Id();
  FindPrivateKey(
      std::move(key_id),
      base::BindOnce(&KcerTokenUtils::ImportRsaKeyWithExistingKey,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(rsa_key), std::move(kcer_public_key).value()));
}

// Creates a private key object in Chaps.
void KcerTokenUtils::ImportRsaKeyWithExistingKey(
    ImportKeyTask task,
    bssl::UniquePtr<RSA> rsa_key,
    PublicKey kcer_public_key,
    std::vector<ObjectHandle> handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ImportKey(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }

  // The key was already found in Chaps and will not be imported again. Return
  // success.
  if (!handles.empty()) {
    return std::move(task.callback).Run(std::move(kcer_public_key));
  }

  Pkcs12Reader pkcs12_reader;
  std::vector<uint8_t> public_modulus_bytes =
      pkcs12_reader.BignumToBytes(RSA_get0_n(rsa_key.get()));
  std::vector<uint8_t> public_exponent_bytes =
      pkcs12_reader.BignumToBytes(RSA_get0_e(rsa_key.get()));
  std::vector<uint8_t> private_exponent_bytes =
      pkcs12_reader.BignumToBytes(RSA_get0_d(rsa_key.get()));
  std::vector<uint8_t> prime_factor_1 =
      pkcs12_reader.BignumToBytes(RSA_get0_p(rsa_key.get()));
  std::vector<uint8_t> prime_factor_2 =
      pkcs12_reader.BignumToBytes(RSA_get0_q(rsa_key.get()));
  std::vector<uint8_t> exponent_1 =
      pkcs12_reader.BignumToBytes(RSA_get0_dmp1(rsa_key.get()));
  std::vector<uint8_t> exponent_2 =
      pkcs12_reader.BignumToBytes(RSA_get0_dmq1(rsa_key.get()));
  std::vector<uint8_t> coefficient =
      pkcs12_reader.BignumToBytes(RSA_get0_iqmp(rsa_key.get()));

  if (public_modulus_bytes.empty() || public_exponent_bytes.empty() ||
      private_exponent_bytes.empty() || prime_factor_1.empty() ||
      prime_factor_2.empty() || exponent_1.empty() || exponent_2.empty() ||
      coefficient.empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToParseKey));
  }

  constexpr chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;
  chromeos::PKCS11_CK_BBOOL is_software_backed = task.hardware_backed
                                                     ? chromeos::PKCS11_CK_FALSE
                                                     : chromeos::PKCS11_CK_TRUE;
  chromeos::PKCS11_CK_OBJECT_CLASS key_class = chromeos::PKCS11_CKO_PRIVATE_KEY;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_RSA;
  chaps::AttributeList attrs;
  AddAttribute(attrs, chromeos::PKCS11_CKA_CLASS, MakeSpan(&key_class));
  AddAttribute(attrs, chromeos::PKCS11_CKA_KEY_TYPE, MakeSpan(&key_type));
  AddAttribute(attrs, chromeos::PKCS11_CKA_TOKEN, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_SENSITIVE, MakeSpan(&kTrue));
  if (!task.hardware_backed) {
    AddAttribute(attrs, chaps::kForceSoftwareAttribute,
                 MakeSpan(&is_software_backed));
  }
  AddAttribute(attrs, chromeos::PKCS11_CKA_EXTRACTABLE,
               MakeSpan(&is_software_backed));
  AddAttribute(attrs, chromeos::PKCS11_CKA_PRIVATE, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_UNWRAP, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_DECRYPT, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_SIGN, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_SIGN_RECOVER, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_MODULUS, public_modulus_bytes);
  AddAttribute(attrs, chromeos::PKCS11_CKA_ID,
               kcer_public_key.GetPkcs11Id().value());
  AddAttribute(attrs, chromeos::PKCS11_CKA_PUBLIC_EXPONENT,
               public_exponent_bytes);
  AddAttribute(attrs, chromeos::PKCS11_CKA_PRIVATE_EXPONENT,
               private_exponent_bytes);
  AddAttribute(attrs, chromeos::PKCS11_CKA_PRIME_1, prime_factor_1);
  AddAttribute(attrs, chromeos::PKCS11_CKA_PRIME_2, prime_factor_2);
  AddAttribute(attrs, chromeos::PKCS11_CKA_EXPONENT_1, exponent_1);
  AddAttribute(attrs, chromeos::PKCS11_CKA_EXPONENT_2, exponent_2);
  AddAttribute(attrs, chromeos::PKCS11_CKA_COEFFICIENT, coefficient);
  if (task.mark_as_migrated) {
    AddAttribute(attrs, pkcs11_custom_attributes::kCkaChromeOsMigratedFromNss,
                 MakeSpan(&kTrue));
  }

  auto chaps_callback = base::BindOnce(
      &KcerTokenUtils::DidImportRsaPrivateKey, weak_factory_.GetWeakPtr(),
      std::move(task), std::move(kcer_public_key), public_modulus_bytes,
      public_exponent_bytes);
  chaps_client_->CreateObject(pkcs_11_slot_id_, attrs,
                              std::move(chaps_callback));
}

// Creates a corresponding public key object in Chaps.
void KcerTokenUtils::DidImportRsaPrivateKey(
    ImportKeyTask task,
    PublicKey kcer_public_key,
    std::vector<uint8_t> public_modulus_bytes,
    std::vector<uint8_t> public_exponent_bytes,
    ObjectHandle priv_key_handle,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ImportKey(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToImportKey));
  }

  constexpr chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_RSA;
  chromeos::PKCS11_CK_OBJECT_CLASS key_class = chromeos::PKCS11_CKO_PUBLIC_KEY;

  chaps::AttributeList attrs;
  AddAttribute(attrs, chromeos::PKCS11_CKA_CLASS, MakeSpan(&key_class));
  AddAttribute(attrs, chromeos::PKCS11_CKA_KEY_TYPE, MakeSpan(&key_type));
  AddAttribute(attrs, chromeos::PKCS11_CKA_TOKEN, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_WRAP, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_ENCRYPT, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_VERIFY, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_MODULUS, public_modulus_bytes);
  AddAttribute(attrs, chromeos::PKCS11_CKA_ID,
               kcer_public_key.GetPkcs11Id().value());
  AddAttribute(attrs, chromeos::PKCS11_CKA_PUBLIC_EXPONENT,
               public_exponent_bytes);
  if (!task.hardware_backed) {
    AddAttribute(attrs, chaps::kForceSoftwareAttribute, MakeSpan(&kTrue));
  }
  if (task.mark_as_migrated) {
    AddAttribute(attrs, pkcs11_custom_attributes::kCkaChromeOsMigratedFromNss,
                 MakeSpan(&kTrue));
  }

  auto chaps_callback = base::BindOnce(
      &KcerTokenUtils::DidImportKey, weak_factory_.GetWeakPtr(),
      std::move(task), std::move(kcer_public_key), priv_key_handle);
  chaps_client_->CreateObject(pkcs_11_slot_id_, attrs,
                              std::move(chaps_callback));
}

void KcerTokenUtils::ImportEcKey(ImportKeyTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bssl::UniquePtr<EC_KEY> ec_key(EVP_PKEY_get1_EC_KEY(task.key_data.key.get()));
  if (!ec_key) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToParseKey));
  }

  std::vector<uint8_t> ec_point_oct = GetEcPublicKeyBytes(ec_key.get());
  base::expected<PublicKey, Error> kcer_public_key =
      MakeEcPublicKey(token_, ec_point_oct);
  if (!kcer_public_key.has_value()) {
    return std::move(task.callback)
        .Run(base::unexpected(kcer_public_key.error()));
  }

  Pkcs11Id key_id = kcer_public_key->GetPkcs11Id();
  FindPrivateKey(
      std::move(key_id),
      base::BindOnce(&KcerTokenUtils::ImportEcKeyWithExistingKey,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(ec_key), std::move(kcer_public_key).value(),
                     std::move(ec_point_oct)));
}

void KcerTokenUtils::ImportEcKeyWithExistingKey(
    ImportKeyTask task,
    bssl::UniquePtr<EC_KEY> ec_key,
    PublicKey kcer_public_key,
    std::vector<uint8_t> ec_point_oct,
    std::vector<ObjectHandle> handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ImportKey(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }

  if (!handles.empty()) {
    return std::move(task.callback).Run(std::move(kcer_public_key));
  }

  std::vector<uint8_t> ec_point_der = DerEncodeAsn1OctetString(ec_point_oct);
  std::vector<uint8_t> priv_key_bytes = GetEcPrivateKeyBytes(ec_key.get());
  std::vector<uint8_t> ec_params_der = GetEcParamsDer(ec_key.get());
  if (priv_key_bytes.empty() || ec_params_der.empty() || ec_point_der.empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToParseKey));
  }

  constexpr chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;
  chromeos::PKCS11_CK_BBOOL is_software_backed = task.hardware_backed
                                                     ? chromeos::PKCS11_CK_FALSE
                                                     : chromeos::PKCS11_CK_TRUE;
  chromeos::PKCS11_CK_OBJECT_CLASS key_class = chromeos::PKCS11_CKO_PRIVATE_KEY;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_EC;
  chaps::AttributeList attrs;
  AddAttribute(attrs, chromeos::PKCS11_CKA_CLASS, MakeSpan(&key_class));
  AddAttribute(attrs, chromeos::PKCS11_CKA_KEY_TYPE, MakeSpan(&key_type));
  AddAttribute(attrs, chromeos::PKCS11_CKA_TOKEN, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_SENSITIVE, MakeSpan(&kTrue));
  if (!task.hardware_backed) {
    AddAttribute(attrs, chaps::kForceSoftwareAttribute,
                 MakeSpan(&is_software_backed));
  }
  AddAttribute(attrs, chromeos::PKCS11_CKA_EXTRACTABLE,
               MakeSpan(&is_software_backed));
  AddAttribute(attrs, chromeos::PKCS11_CKA_SIGN, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_SIGN_RECOVER, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_DERIVE, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_ID,
               kcer_public_key.GetPkcs11Id().value());
  AddAttribute(attrs, chromeos::PKCS11_CKA_VALUE, priv_key_bytes);
  AddAttribute(attrs, chromeos::PKCS11_CKA_EC_POINT, ec_point_der);
  AddAttribute(attrs, chromeos::PKCS11_CKA_PRIVATE, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_EC_PARAMS, ec_params_der);
  if (task.mark_as_migrated) {
    AddAttribute(attrs, pkcs11_custom_attributes::kCkaChromeOsMigratedFromNss,
                 MakeSpan(&kTrue));
  }

  auto chaps_callback = base::BindOnce(
      &KcerTokenUtils::DidImportEcPrivateKey, weak_factory_.GetWeakPtr(),
      std::move(task), std::move(kcer_public_key), std::move(ec_point_der),
      std::move(ec_params_der));
  chaps_client_->CreateObject(pkcs_11_slot_id_, attrs,
                              std::move(chaps_callback));
}

void KcerTokenUtils::DidImportEcPrivateKey(ImportKeyTask task,
                                           PublicKey kcer_public_key,
                                           std::vector<uint8_t> ec_point_der,
                                           std::vector<uint8_t> ec_params_der,
                                           ObjectHandle priv_key_handle,
                                           uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ImportKey(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToImportKey));
  }

  constexpr chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_EC;
  chromeos::PKCS11_CK_OBJECT_CLASS key_class = chromeos::PKCS11_CKO_PUBLIC_KEY;

  chaps::AttributeList attrs;
  AddAttribute(attrs, chromeos::PKCS11_CKA_CLASS, MakeSpan(&key_class));
  AddAttribute(attrs, chromeos::PKCS11_CKA_KEY_TYPE, MakeSpan(&key_type));
  AddAttribute(attrs, chromeos::PKCS11_CKA_TOKEN, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_VERIFY, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_DERIVE, MakeSpan(&kTrue));
  AddAttribute(attrs, chromeos::PKCS11_CKA_EC_PARAMS, ec_params_der);
  AddAttribute(attrs, chromeos::PKCS11_CKA_EC_POINT, ec_point_der);
  AddAttribute(attrs, chromeos::PKCS11_CKA_ID,
               kcer_public_key.GetPkcs11Id().value());
  if (!task.hardware_backed) {
    AddAttribute(attrs, chaps::kForceSoftwareAttribute, MakeSpan(&kTrue));
  }
  if (task.mark_as_migrated) {
    AddAttribute(attrs, pkcs11_custom_attributes::kCkaChromeOsMigratedFromNss,
                 MakeSpan(&kTrue));
  }

  auto chaps_callback = base::BindOnce(
      &KcerTokenUtils::DidImportKey, weak_factory_.GetWeakPtr(),
      std::move(task), std::move(kcer_public_key), priv_key_handle);
  chaps_client_->CreateObject(pkcs_11_slot_id_, attrs,
                              std::move(chaps_callback));
}

void KcerTokenUtils::DidImportKey(ImportKeyTask task,
                                  PublicKey kcer_public_key,
                                  ObjectHandle priv_key_handle,
                                  ObjectHandle pub_key_handle,
                                  uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ImportKey(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {priv_key_handle},
        Bind(std::move(task.callback), Error::kFailedToImportKey));
  }
  return std::move(task.callback).Run(kcer_public_key);
}

//==============================================================================

void KcerTokenUtils::ImportPkcs12(KeyData key_data,
                                  std::vector<CertData> certs_data,
                                  bool hardware_backed,
                                  bool mark_as_migrated,
                                  ImportPkcs12Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  kcer::KeyType key_type;
  if (IsKeyRsaType(key_data.key)) {
    RecordKcerPkcs12ImportUmaEvent(
        KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask);
    key_type = KeyType::kRsa;
  } else if (IsKeyEcType(key_data.key)) {
    RecordKcerPkcs12ImportUmaEvent(
        KcerPkcs12ImportEvent::AttemptedEcKeyImportTask);
    key_type = KeyType::kEcc;
  } else {
    LOG(ERROR) << "Unexpected key type";
    return std::move(callback).Run(/*did_modify=*/false,
                                   base::unexpected(Error::kUnknownKeyType));
  }

  auto import_callback = base::BindOnce(
      &KcerTokenUtils::ImportPkc12DidImportKey, weak_factory_.GetWeakPtr(),
      key_type, Pkcs11Id(key_data.cka_id_value), std::move(certs_data),
      hardware_backed, mark_as_migrated, std::move(callback));

  ImportKey(ImportKeyTask(std::move(key_data), hardware_backed,
                          mark_as_migrated, std::move(import_callback)));
}

void KcerTokenUtils::ImportPkc12DidImportKey(
    kcer::KeyType key_type,
    Pkcs11Id pkcs11_id,
    std::vector<CertData> certs_data,
    bool hardware_backed,
    bool mark_as_migrated,
    ImportPkcs12Callback callback,
    base::expected<PublicKey, Error> imported_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!imported_key.has_value()) {
    return std::move(callback).Run(/*did_modify=*/false,
                                   base::unexpected(imported_key.error()));
  }
  if (key_type == kcer::KeyType::kRsa) {
    RecordKcerPkcs12ImportUmaEvent(
        KcerPkcs12ImportEvent::SuccessRsaKeyImportTask);
  } else {
    RecordKcerPkcs12ImportUmaEvent(
        KcerPkcs12ImportEvent::SuccessEcKeyImportTask);
  }

  bool is_multi_cert_import = (certs_data.size() > 1);
  if (is_multi_cert_import) {
    RecordKcerPkcs12ImportUmaEvent(
        KcerPkcs12ImportEvent::AttemptedMultipleCertImport);
  }

  ImportAllCerts(ImportAllCertsTask(
      std::move(pkcs11_id), std::move(certs_data), hardware_backed,
      mark_as_migrated, is_multi_cert_import, key_type, std::move(callback)));
}

//==============================================================================

KcerTokenUtils::ImportAllCertsTask::ImportAllCertsTask(
    Pkcs11Id in_pkcs11_id,
    std::vector<CertData> in_certs_data,
    bool in_hardware_backed,
    bool in_mark_as_migrated,
    bool in_multi_cert_import,
    KeyType in_key_type,
    ImportPkcs12Callback in_callback)
    : pkcs11_id(std::move(in_pkcs11_id)),
      certs_data(std::move(in_certs_data)),
      hardware_backed(in_hardware_backed),
      mark_as_migrated(in_mark_as_migrated),
      multi_cert_import(in_multi_cert_import),
      key_type(in_key_type),
      callback(std::move(in_callback)) {}
KcerTokenUtils::ImportAllCertsTask::ImportAllCertsTask(
    ImportAllCertsTask&& other) = default;
KcerTokenUtils::ImportAllCertsTask::~ImportAllCertsTask() = default;

void KcerTokenUtils::ImportAllCerts(ImportAllCertsTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  RecordKcerPkcs12ImportUmaEvent(
      KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask);
  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(/*did_modify=*/false,
             base::unexpected(Error::kPkcs11SessionFailure));
  }

  // The original vector of certs has to stay unchanged in case a retry is
  // required. Create an additional vector to track which certs still need to be
  // processed in the current attempt. Raw pointers must not outlive the
  // originals stored in the `task`.
  std::vector<const CertData*> certs_data;
  certs_data.reserve(task.certs_data.size());
  for (const CertData& data : task.certs_data) {
    certs_data.push_back(&data);
  }

  return ImportAllCertsImpl(std::move(task), std::move(certs_data),
                            /*imports_failed=*/0);
}

void KcerTokenUtils::ImportAllCertsImpl(ImportAllCertsTask task,
                                        std::vector<const CertData*> certs_data,
                                        int imports_failed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (certs_data.empty()) {
    base::expected<void, Error> result;
    if (imports_failed != 0) {
      result = base::unexpected(Error::kFailedToImportCertificate);
    } else {
      RecordUmaImportSuccess(task.key_type, task.multi_cert_import);
    }
    return std::move(task.callback).Run(/*did_modify=*/true, std::move(result));
  }

  const CertData* cur_cert = certs_data.back();
  certs_data.pop_back();

  Pkcs11Id pkcs11_id = task.pkcs11_id;
  bool hardware_backed = task.hardware_backed;
  bool mark_as_migrated = task.mark_as_migrated;

  auto callback =
      base::BindOnce(&KcerTokenUtils::ImportAllCertsDidImportOneCert,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(certs_data), imports_failed);

  return ImportCert(cur_cert->x509, pkcs11_id, cur_cert->nickname,
                    cur_cert->cert_der, hardware_backed, mark_as_migrated,
                    std::move(callback));
}

void KcerTokenUtils::ImportAllCertsDidImportOneCert(
    ImportAllCertsTask task,
    std::vector<const CertData*> certs_data,
    int imports_failed,
    std::optional<Error> kcer_error,
    SessionChapsClient::ObjectHandle cert_handle,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    // Try again from the beginning. If some keys or certs were imported before
    // the session got closed, they will be found and skipped on the next
    // attempt.
    return ImportAllCerts(std::move(task));
  }

  if (kcer_error.has_value() || (result_code != chromeos::PKCS11_CKR_OK)) {
    ++imports_failed;
  }

  return ImportAllCertsImpl(std::move(task), std::move(certs_data),
                            imports_failed);
}

}  // namespace kcer::internal
