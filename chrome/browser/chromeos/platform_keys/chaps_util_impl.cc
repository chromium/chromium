// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/chaps_util_impl.h"

#include <dlfcn.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <pkcs11.h>
#include <pkcs11t.h>

#include <ostream>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/chromeos/platform_keys/chaps_slot_session.h"
#include "chrome/browser/chromeos/platform_keys/pkcs12_reader.h"
#include "crypto/chaps_support.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"

namespace chromeos {
namespace platform_keys {

namespace {

// TODO(b/202374261): Move these into a shared header.
// Signals to chaps that a generated key should be software-backed.
constexpr CK_ATTRIBUTE_TYPE kForceSoftwareAttribute = CKA_VENDOR_DEFINED + 4;
// Chaps sets this for keys that are software-backed.
constexpr CK_ATTRIBUTE_TYPE kKeyInSoftware = CKA_VENDOR_DEFINED + 5;
constexpr char kPkcs12ImportFailed[] = "Chaps util PKCS12 import failed with ";
constexpr char kPkcs12KeyImportFailed[] = "Chaps util key import failed with ";
constexpr char kPkcs12CertImportFailed[] =
    "Chaps util cert import failed with ";
// Wraps public key and private key PKCS#11 object handles.
struct KeyPairHandles {
  CK_OBJECT_HANDLE public_key;
  CK_OBJECT_HANDLE private_key;
};

using Pkcs11Operation = base::RepeatingCallback<CK_RV()>;

// Performs |operation| and handles return values indicating that the PKCS11
// session has been closed by attempting to re-open the |chaps_session|.
// This is useful because the session could be closed e.g. because NSS could
// have called C_CloseAllSessions.
bool PerformWithRetries(ChapsSlotSession* chaps_session,
                        base::StringPiece operation_name,
                        const Pkcs11Operation& operation) {
  const int kMaxAttempts = 5;

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    CK_RV result = operation.Run();
    if (result == CKR_OK) {
      return true;
    }
    if (result != CKR_SESSION_HANDLE_INVALID && result != CKR_SESSION_CLOSED) {
      LOG(ERROR) << operation_name << " failed with " << result;
      return false;
    }
    if (!chaps_session->ReopenSession()) {
      return false;
    }
  }
  LOG(ERROR) << operation_name << " failed";
  return false;
}

// Uses |chaps_session| to generate a software-backed RSA key pair with modulus
// length |num_bits|.
absl::optional<KeyPairHandles> GenerateSoftwareBackedRSAKeyPair(
    ChapsSlotSession* chaps_session,
    uint16_t num_bits) {
  CK_ULONG modulus_bits = num_bits;
  CK_BBOOL true_value = CK_TRUE;
  CK_BBOOL false_value = CK_FALSE;
  CK_BYTE public_exponent[3] = {0x01, 0x00, 0x01};  // 65537

  // Public key attributes
  // Note: CKA_ID is set later (computed from the public key modulus) and
  // CKA_LABEL is not set to match NSS behavior
  // (https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/PKCS11_Implement).
  CK_ATTRIBUTE pub_attributes[] = {
      {CKA_TOKEN, &true_value, sizeof(true_value)},
      {CKA_PRIVATE, &false_value, sizeof(false_value)},
      {CKA_VERIFY, &true_value, sizeof(true_value)},
      {CKA_MODULUS_BITS, &modulus_bits, sizeof(modulus_bits)},
      {CKA_PUBLIC_EXPONENT, public_exponent, sizeof(public_exponent)}};

  // Private key attributes
  // Note: CKA_ID is set later (computed from the public key modulus) and
  // CKA_LABEL is not set to match NSS behavior
  // (https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/PKCS11_Implement).
  CK_ATTRIBUTE priv_attributes[] = {
      {CKA_TOKEN, &true_value, sizeof(true_value)},
      {CKA_PRIVATE, &true_value, sizeof(true_value)},
      {CKA_SENSITIVE, &true_value, sizeof(true_value)},
      {CKA_EXTRACTABLE, &false_value, sizeof(false_value)},
      {kForceSoftwareAttribute, &true_value, sizeof(true_value)},
      {CKA_SIGN, &true_value, sizeof(true_value)}};
  CK_MECHANISM mechanism = {CKM_RSA_PKCS_KEY_PAIR_GEN, /* pParameter */ nullptr,
                            /* ulParameterLen*/ 0};

  KeyPairHandles key_pair;

  if (!PerformWithRetries(
          chaps_session, "GenerateKeyPair",
          base::BindRepeating(&ChapsSlotSession::GenerateKeyPair,
                              base::Unretained(chaps_session), &mechanism,
                              pub_attributes, std::size(pub_attributes),
                              priv_attributes, std::size(priv_attributes),
                              &(key_pair.public_key),
                              &(key_pair.private_key)))) {
    return {};
  }
  return key_pair;
}

// Read the modulus of the public key identified by |pub_key_handle| and return
// it.
absl::optional<std::vector<CK_BYTE>> ExtractModulus(
    ChapsSlotSession* chaps_session,
    CK_OBJECT_HANDLE pub_key_handle) {
  std::vector<CK_BYTE> modulus(256);
  CK_ATTRIBUTE attrs_get_modulus[] = {
      {CKA_MODULUS, modulus.data(), modulus.size()}};
  if (!PerformWithRetries(
          chaps_session, "GetAttributeValue",
          base::BindRepeating(&ChapsSlotSession::GetAttributeValue,
                              base::Unretained(chaps_session), pub_key_handle,
                              attrs_get_modulus,
                              std::size(attrs_get_modulus)))) {
    return {};
  }
  return modulus;
}

// Read the modulus of the public key identified by |pub_key_handle| and return
// it.
absl::optional<bool> IsKeySoftwareBacked(ChapsSlotSession* chaps_session,
                                         CK_OBJECT_HANDLE private_key_handle) {
  CK_BBOOL key_in_software = CK_FALSE;
  CK_ATTRIBUTE attrs_get_key_in_software[] = {
      {kKeyInSoftware, &key_in_software, sizeof(key_in_software)}};
  if (!PerformWithRetries(
          chaps_session, "GetAttributeValue",
          base::BindRepeating(&ChapsSlotSession::GetAttributeValue,
                              base::Unretained(chaps_session),
                              private_key_handle, attrs_get_key_in_software,
                              std::size(attrs_get_key_in_software)))) {
    return {};
  }
  return key_in_software;
}

crypto::ScopedSECItem MakeIdFromPubKeyNss(std::vector<CK_BYTE>& rsa_modulus) {
  SECItem secitem_modulus;
  secitem_modulus.data = rsa_modulus.data();
  secitem_modulus.len = rsa_modulus.size();
  return crypto::ScopedSECItem(PK11_MakeIDFromPubKey(&secitem_modulus));
}

std::vector<uint8_t> SECItemToBytes(const crypto::ScopedSECItem& id) {
  return std::vector<uint8_t>(id->data, id->data + id->len);
}
// Create the CKA_ID value that NSS would use for |key_pair| and return it.
crypto::ScopedSECItem CreateNssCkaId(ChapsSlotSession* chaps_session,
                                     const KeyPairHandles& key_pair) {
  auto modulus = ExtractModulus(chaps_session, key_pair.public_key);
  if (!modulus) {
    return nullptr;
  }
  return MakeIdFromPubKeyNss(modulus.value());
}

// Set the CKA_ID attribute of the public and private key objects in |key_pair|
// to |cka_id|.
bool SetCkaId(ChapsSlotSession* chaps_session,
              KeyPairHandles& key_pair,
              SECItem* cka_id) {
  CK_ATTRIBUTE attrs_set_id[] = {{CKA_ID, cka_id->data, cka_id->len}};
  if (!PerformWithRetries(
          chaps_session, "SetAttributeValue",
          base::BindRepeating(&ChapsSlotSession::SetAttributeValue,
                              base::Unretained(chaps_session),
                              key_pair.private_key, attrs_set_id,
                              std::size(attrs_set_id)))) {
    return false;
  }
  if (!PerformWithRetries(
          chaps_session, "SetAttributeValue",
          base::BindRepeating(&ChapsSlotSession::SetAttributeValue,
                              base::Unretained(chaps_session),
                              key_pair.public_key, attrs_set_id,
                              std::size(attrs_set_id)))) {
    return false;
  }
  return true;
}

std::string MakePkcs12KeyImportErrorMessage(Pkcs12ReaderStatusCode error_code) {
  return kPkcs12KeyImportFailed +
         base::NumberToString(static_cast<int>(error_code));
}

std::string MakePkcs12CertImportErrorMessage(
    Pkcs12ReaderStatusCode error_code) {
  return kPkcs12CertImportFailed +
         base::NumberToString(static_cast<int>(error_code));
}

std::string MakePkcs12ImportErrorMessage(Pkcs12ReaderStatusCode error_code) {
  return kPkcs12ImportFailed +
         base::NumberToString(static_cast<int>(error_code));
}
Pkcs12ReaderStatusCode ImportRsaKey(ChapsSlotSession* chaps_session,
                                    bssl::UniquePtr<EVP_PKEY> key,
                                    bool is_software_backed,
                                    const Pkcs12Reader* pkcs12_reader,
                                    std::vector<uint8_t>& out_id,
                                    CK_OBJECT_HANDLE& out_key_handle) {
  if (!key) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kKeyDataMissed);
    return Pkcs12ReaderStatusCode::kKeyDataMissed;
  }

  // All the data variables must stay alive until `key_template` is sent to
  // Chaps.
  const RSA* rsa_key = EVP_PKEY_get0_RSA(key.get());
  std::vector<uint8_t> public_modulus_bytes =
      pkcs12_reader->BignumToBytes(RSA_get0_n(rsa_key));
  out_id = SECItemToBytes(MakeIdFromPubKeyNss(public_modulus_bytes));
  std::vector<uint8_t> public_exponent_bytes =
      pkcs12_reader->BignumToBytes(RSA_get0_e(rsa_key));
  std::vector<uint8_t> private_exponent_bytes =
      pkcs12_reader->BignumToBytes(RSA_get0_d(rsa_key));
  std::vector<uint8_t> prime_factor_1 =
      pkcs12_reader->BignumToBytes(RSA_get0_p(rsa_key));
  std::vector<uint8_t> prime_factor_2 =
      pkcs12_reader->BignumToBytes(RSA_get0_q(rsa_key));
  std::vector<uint8_t> exponent_1 =
      pkcs12_reader->BignumToBytes(RSA_get0_dmp1(rsa_key));
  std::vector<uint8_t> exponent_2 =
      pkcs12_reader->BignumToBytes(RSA_get0_dmq1(rsa_key));
  std::vector<uint8_t> coefficient =
      pkcs12_reader->BignumToBytes(RSA_get0_iqmp(rsa_key));

  if (public_modulus_bytes.empty() || out_id.empty() ||
      public_exponent_bytes.empty() || private_exponent_bytes.empty() ||
      prime_factor_1.empty() || prime_factor_2.empty() || exponent_1.empty() ||
      exponent_2.empty() || coefficient.empty()) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kKeyAttrDataMissing);
    return Pkcs12ReaderStatusCode::kKeyAttrDataMissing;
  }

  CK_BBOOL true_value = CK_TRUE;
  CK_OBJECT_CLASS key_class = CKO_PRIVATE_KEY;
  CK_KEY_TYPE key_type = CKK_RSA;
  CK_BBOOL force_software_attribute = is_software_backed ? CK_TRUE : CK_FALSE;
  CK_ATTRIBUTE attrs[] = {
      {CKA_CLASS, &key_class, sizeof(key_class)},
      {CKA_KEY_TYPE, &key_type, sizeof(key_type)},
      {CKA_TOKEN, &true_value, sizeof(CK_BBOOL)},
      {CKA_SENSITIVE, &true_value, sizeof(CK_BBOOL)},
      {kForceSoftwareAttribute, &force_software_attribute, sizeof(CK_BBOOL)},
      {CKA_PRIVATE, &true_value, sizeof(CK_BBOOL)},
      {CKA_UNWRAP, &true_value, sizeof(CK_BBOOL)},
      {CKA_DECRYPT, &true_value, sizeof(CK_BBOOL)},
      {CKA_SIGN, &true_value, sizeof(CK_BBOOL)},
      {CKA_SIGN_RECOVER, &true_value, sizeof(CK_BBOOL)},
      {CKA_MODULUS, public_modulus_bytes.data(), public_modulus_bytes.size()},
      {CKA_ID, out_id.data(), out_id.size()},
      {CKA_PUBLIC_EXPONENT, public_exponent_bytes.data(),
       public_exponent_bytes.size()},
      {CKA_PRIVATE_EXPONENT, private_exponent_bytes.data(),
       private_exponent_bytes.size()},
      {CKA_PRIME_1, prime_factor_1.data(), prime_factor_1.size()},
      {CKA_PRIME_2, prime_factor_2.data(), prime_factor_2.size()},
      {CKA_EXPONENT_1, exponent_1.data(), exponent_1.size()},
      {CKA_EXPONENT_2, exponent_2.data(), exponent_2.size()},
      {CKA_COEFFICIENT, coefficient.data(), coefficient.size()}};

  if (!PerformWithRetries(
          chaps_session, "CreateObject",
          base::BindRepeating(&ChapsSlotSession::CreateObject,
                              base::Unretained(chaps_session), attrs,
                              /*ulCount=*/std::size(attrs), &out_key_handle))) {
    LOG(ERROR) << MakePkcs12KeyImportErrorMessage(
        Pkcs12ReaderStatusCode::kCreateKeyFailed);
    return Pkcs12ReaderStatusCode::kCreateKeyFailed;
  }
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode ImportOneCert(ChapsSlotSession* chaps_session,
                                     X509* cert,
                                     const std::vector<uint8_t>& id,
                                     CK_OBJECT_HANDLE key_handle,
                                     const Pkcs12Reader* pkcs12_helper,
                                     bool is_software_backed) {
  if (!cert) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(
        Pkcs12ReaderStatusCode::kCertificateDataMissed);
    return Pkcs12ReaderStatusCode::kCertificateDataMissed;
  }

  CK_OBJECT_CLASS cert_class = CKO_CERTIFICATE;
  CK_CERTIFICATE_TYPE cert_type = CKC_X_509;
  CK_BBOOL true_value = CK_TRUE;

  int cert_der_size = 0;
  bssl::UniquePtr<uint8_t> cert_der;
  Pkcs12ReaderStatusCode get_cert_der_result =
      pkcs12_helper->GetDerEncodedCert(cert, cert_der, cert_der_size);

  if (get_cert_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_cert_der_result);
    return get_cert_der_result;
  }

  base::span<const uint8_t> issuer_name_data;
  Pkcs12ReaderStatusCode get_issuer_name_der_result =
      pkcs12_helper->GetIssuerNameDer(cert, issuer_name_data);
  if (get_issuer_name_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_issuer_name_der_result);
    return get_issuer_name_der_result;
  }

  base::span<const uint8_t> subject_name_data;
  Pkcs12ReaderStatusCode get_subject_name_der_result =
      pkcs12_helper->GetSubjectNameDer(cert, subject_name_data);
  if (get_subject_name_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_subject_name_der_result);
    return get_subject_name_der_result;
  }

  int serial_number_der_size = 0;
  bssl::UniquePtr<uint8_t> serial_number_der;
  Pkcs12ReaderStatusCode get_serial_der_result =
      pkcs12_helper->GetSerialNumberDer(cert, serial_number_der,
                                        serial_number_der_size);
  if (get_serial_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_serial_der_result);
    return get_serial_der_result;
  }

  std::string label;
  Pkcs12ReaderStatusCode get_label_result =
      pkcs12_helper->GetLabel(cert, label);
  if (get_label_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_label_result);
    return get_label_result;
  }

  CK_BBOOL force_software_attribute = is_software_backed ? CK_TRUE : CK_FALSE;

  CK_ATTRIBUTE attrs[] = {
      {CKA_CLASS, &cert_class, sizeof(cert_class)},
      {CKA_CERTIFICATE_TYPE, &cert_type, sizeof(cert_type)},
      {CKA_TOKEN, &true_value, sizeof(true_value)},
      {kForceSoftwareAttribute, &force_software_attribute, sizeof(CK_BBOOL)},
      {CKA_ID, const_cast<uint8_t*>(id.data()), id.size()},
      {CKA_LABEL, label.data(), label.size()},
      {CKA_VALUE, cert_der.get(),
       base::saturated_cast<CK_ULONG>(cert_der_size)},
      {CKA_ISSUER, const_cast<uint8_t*>(issuer_name_data.data()),
       issuer_name_data.size()},
      {CKA_SUBJECT, const_cast<uint8_t*>(subject_name_data.data()),
       subject_name_data.size()},
      {CKA_SERIAL_NUMBER, serial_number_der.get(),
       base::saturated_cast<CK_ULONG>(serial_number_der_size)}};

  CK_OBJECT_HANDLE cert_handle;
  if (!PerformWithRetries(
          chaps_session, "CreateObject",
          base::BindRepeating(&ChapsSlotSession::CreateObject,
                              base::Unretained(chaps_session), attrs,
                              /*ulCount=*/std::size(attrs), &cert_handle))) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(
        Pkcs12ReaderStatusCode::kCreateCertFailed);
    return Pkcs12ReaderStatusCode::kCreateCertFailed;
  }

  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode ImportAllCerts(ChapsSlotSession* chaps_session,
                                      bssl::UniquePtr<STACK_OF(X509)> certs,
                                      const std::vector<uint8_t>& id,
                                      CK_OBJECT_HANDLE key_handle,
                                      const Pkcs12Reader* pkcs12_helper,
                                      bool is_software_backed) {
  if (!certs) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(
        Pkcs12ReaderStatusCode::kCertificateDataMissed);
    return Pkcs12ReaderStatusCode::kCertificateDataMissed;
  }

  Pkcs12ReaderStatusCode is_every_cert_imported =
      Pkcs12ReaderStatusCode::kSuccess;
  for (size_t i = 0; i < sk_X509_num(certs.get()); ++i) {
    if (ImportOneCert(chaps_session, sk_X509_value(certs.get(), i), id,
                      key_handle, pkcs12_helper,
                      is_software_backed) != Pkcs12ReaderStatusCode::kSuccess) {
      is_every_cert_imported = Pkcs12ReaderStatusCode::kFailureDuringCertImport;
    }
  }
  return is_every_cert_imported;
}

}  // namespace

ChapsUtilImpl::ChapsUtilImpl(
    std::unique_ptr<ChapsSlotSessionFactory> chaps_slot_session_factory)
    : chaps_slot_session_factory_(std::move(chaps_slot_session_factory)) {}
ChapsUtilImpl::~ChapsUtilImpl() = default;

bool ChapsUtilImpl::GenerateSoftwareBackedRSAKey(
    PK11SlotInfo* slot,
    uint16_t num_bits,
    crypto::ScopedSECKEYPublicKey* out_public_key,
    crypto::ScopedSECKEYPrivateKey* out_private_key) {
  DCHECK(out_public_key);
  DCHECK(out_private_key);

  std::unique_ptr<ChapsSlotSession> chaps_session =
      GetChapsSlotSessionForSlot(slot);
  if (!chaps_session) {
    return false;
  }

  absl::optional<KeyPairHandles> key_pair =
      GenerateSoftwareBackedRSAKeyPair(chaps_session.get(), num_bits);
  if (!key_pair) {
    return false;
  }

  // Safety check that software-backed key generation was triggered.
  absl::optional<bool> is_software_backed =
      IsKeySoftwareBacked(chaps_session.get(), key_pair->private_key);
  if (!is_software_backed || !is_software_backed.value()) {
    return false;
  }

  crypto::ScopedSECItem cka_id =
      CreateNssCkaId(chaps_session.get(), key_pair.value());
  if (!cka_id) {
    return false;
  }
  if (!SetCkaId(chaps_session.get(), key_pair.value(), cka_id.get())) {
    return false;
  }

  out_private_key->reset(PK11_FindKeyByKeyID(slot, cka_id.get(), nullptr));
  if (!*out_private_key) {
    LOG(ERROR) << "Failed to find private key.";
    return false;
  }
  out_public_key->reset(SECKEY_ConvertToPublicKey(out_private_key->get()));
  if (!*out_public_key) {
    LOG(ERROR) << "Failed to extract public key.";
    return false;
  }
  return true;
}

bool ChapsUtilImpl::ImportPkcs12Certificate(
    PK11SlotInfo* slot,
    const std::vector<uint8_t>& pkcs12_data,
    const std::string& password,
    bool is_software_backed) {
  return ImportPkcs12CertificateImpl(slot, pkcs12_data, password,
                                     is_software_backed);
}

bool ChapsUtilImpl::ImportPkcs12CertificateImpl(
    PK11SlotInfo* slot,
    const std::vector<uint8_t>& pkcs12_data,
    const std::string& password,
    const bool is_software_backed,
    const Pkcs12Reader& pkcs12_helper_inc) {
  std::unique_ptr<ChapsSlotSession> chaps_session =
      GetChapsSlotSessionForSlot(slot);
  if (!chaps_session) {
    LOG(ERROR) << MakePkcs12ImportErrorMessage(
        Pkcs12ReaderStatusCode::kChapsSessionMissed);
    return false;
  }

  bssl::UniquePtr<EVP_PKEY> key;
  bssl::UniquePtr<STACK_OF(X509)> certs;
  Pkcs12ReaderStatusCode get_key_and_cert_status =
      pkcs12_helper_inc.GetPkcs12KeyAndCerts(pkcs12_data, password, key, certs);
  if (get_key_and_cert_status != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12ImportErrorMessage(get_key_and_cert_status);
    return false;
  }

  CK_OBJECT_HANDLE key_handle;
  // Same id will be used for the key and certs.
  std::vector<uint8_t> cka_id_value;

  Pkcs12ReaderStatusCode import_key_status =
      ImportRsaKey(chaps_session.get(), std::move(key), is_software_backed,
                   &pkcs12_helper_inc, cka_id_value, key_handle);
  if (import_key_status != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12ImportErrorMessage(import_key_status);
    return false;
  }

  Pkcs12ReaderStatusCode import_cert_status =
      ImportAllCerts(chaps_session.get(), std::move(certs), cka_id_value,
                     key_handle, &pkcs12_helper_inc, is_software_backed);
  if (import_cert_status != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12ImportErrorMessage(import_cert_status);
    return false;
  }

  return true;
}

std::unique_ptr<ChapsSlotSession> ChapsUtilImpl::GetChapsSlotSessionForSlot(
    PK11SlotInfo* slot) {
  if (!slot || (!is_chaps_provided_slot_for_testing_ &&
                !crypto::IsSlotProvidedByChaps(slot))) {
    return nullptr;
  }

  // Note that ChapsSlotSession(Factory) expects something else to have called
  // C_Initialize. It is a safe assumption that NSS has called C_Initialize for
  // chaps if |slot| is actually a chaps-provided slot, which is verified above.
  return chaps_slot_session_factory_->CreateChapsSlotSession(
      PK11_GetSlotID(slot));
}

}  // namespace platform_keys
}  // namespace chromeos
