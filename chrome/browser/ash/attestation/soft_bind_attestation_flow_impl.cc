// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/soft_bind_attestation_flow_impl.h"

#include <optional>

#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chromeos/ash/components/attestation/attestation_flow_adaptive.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/openssl_util.h"
#include "crypto/random.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/pki/pem.h"
#include "third_party/securemessage/proto/securemessage.pb.h"

namespace ash {
namespace attestation {

namespace {

//  Adds a critical extension following the specification described at
//  https://datatracker.ietf.org/doc/html/rfc5280#section-4.2 and
//  the ASN.1 encoding defined at
//  https://datatracker.ietf.org/doc/html/rfc5280#section-4.1:
//  Extension  ::=  SEQUENCE  {
//        extnID      OBJECT IDENTIFIER,
//        critical    BOOLEAN DEFAULT FALSE,
//        extnValue   OCTET STRING
//                    -- contains the DER encoding of an ASN.1 value
//                    -- corresponding to the extension type identified
//                    -- by extnID
//        }
bool AddCriticalExtension(CBB* extensions,
                          const uint8_t* ext_oid,
                          size_t ext_oid_len,
                          const uint8_t* ext_value,
                          size_t ext_value_len) {
  CBB extension, oid, value;
  if (!CBB_add_asn1(extensions, &extension, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&extension, &oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&oid, ext_oid, ext_oid_len) ||
      !CBB_add_asn1_bool(&extension, 1) ||
      !CBB_add_asn1(&extension, &value, CBS_ASN1_OCTETSTRING) ||
      !CBB_add_bytes(&value, ext_value, ext_value_len) ||
      !CBB_flush(extensions)) {
    return false;
  }
  return true;
}

// The validity time of the leaf cert
constexpr base::TimeDelta kLeafCertValidityWindow = base::Hours(72);

// Trigger a new certificate if current certificate is nearing expiration.
constexpr base::TimeDelta kExpiryThresholdDays = base::Days(30);

// RSA SHA256 oid
const uint8_t kRsaSha256Oid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                 0x0d, 0x01, 0x01, 0x0b};

const uint8_t kBasicConstraintsOid[] = {0x55, 0x1d, 0x13};
const uint8_t kKeyUsageOid[] = {0x55, 0x1d, 0x0f};
// cA = FALSE
const uint8_t kBasicConstraintsContents[] = {0x30, 0x00};
// Tag 3 (bit string), length 2, 0 unused of 0x80 (0b10000000)
// Bit 0 specifies the digitalSignature usage.
const uint8_t kKeyUsageContents[] = {0x03, 0x02, 0x00, 0x80};

const char kLeafCertIssuerName[] =
    "O=Chrome Device Soft Bind,CN=Local Authority";
const char kLeafCertSubjectName[] =
    "O=Chrome Device Soft Bind,CN=Cryptauth User Key";

// If it takes more than 30 seconds to receive a response, it's not likely
// ever going to succeed.
constexpr base::TimeDelta kTimeout = base::Seconds(30);

}  // namespace

SoftBindAttestationFlowImpl::Session::Session(Callback callback,
                                              AccountId account_id,
                                              const std::string& user_key)
    : callback_(std::move(callback)),
      account_id_(account_id),
      user_key_(user_key) {
  base::RepeatingClosure timeout_callback =
      base::BindRepeating(&SoftBindAttestationFlowImpl::Session::OnTimeout,
                          weak_ptr_factory_.GetWeakPtr());
  timer_.Start(FROM_HERE, kTimeout, std::move(timeout_callback));
}

SoftBindAttestationFlowImpl::Session::~Session() = default;

void SoftBindAttestationFlowImpl::Session::OnTimeout() {
  LOG(WARNING) << "Timeout exceeded";
  ReportFailure("timeout");
}

bool SoftBindAttestationFlowImpl::Session::IsTimerRunning() const {
  return timer_.IsRunning();
}

void SoftBindAttestationFlowImpl::Session::StopTimer() {
  timer_.Stop();
}

bool SoftBindAttestationFlowImpl::Session::ResetTimer() {
  if (max_retries_-- > 0) {
    timer_.Reset();
    return true;
  }
  return false;
}

const AccountId& SoftBindAttestationFlowImpl::Session::GetAccountId() const {
  return account_id_;
}

const std::string& SoftBindAttestationFlowImpl::Session::GetUserKey() const {
  return user_key_;
}

void SoftBindAttestationFlowImpl::Session::ReportFailure(
    const std::string& error_message) {
  LOG(WARNING) << "Attestation session failure: " << error_message;
  if (!callback_) {
    LOG(WARNING) << "Callback is null";
    base::debug::DumpWithoutCrashing();
    return;
  }
  std::move(callback_).Run(std::vector<std::string>{"INVALID:" + error_message},
                           /*valid=*/false);
}

void SoftBindAttestationFlowImpl::Session::ReportSuccess(
    const std::vector<std::string>& certificate_chain) {
  if (!callback_) {
    LOG(WARNING) << "Attestation session success but callback is null";
    base::debug::DumpWithoutCrashing();
    return;
  }
  std::move(callback_).Run(certificate_chain, /*valid=*/true);
}

SoftBindAttestationFlowImpl::SoftBindAttestationFlowImpl()
    : attestation_client_(AttestationClient::Get()) {
  std::unique_ptr<ServerProxy> attestation_ca_client(new AttestationCAClient());
  attestation_flow_ = std::make_unique<AttestationFlowAdaptive>(
      std::move(attestation_ca_client));
}

SoftBindAttestationFlowImpl::~SoftBindAttestationFlowImpl() = default;

void SoftBindAttestationFlowImpl::SetAttestationFlowForTesting(
    std::unique_ptr<AttestationFlow> attestation_flow) {
  attestation_flow_ = std::move(attestation_flow);
}

void SoftBindAttestationFlowImpl::GetCertificate(Callback callback,
                                                 const AccountId& account_id,
                                                 const std::string& user_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsAttestationAllowedByPolicy()) {
    LOG(ERROR) << "Attestation not allowed by device policy";
    std::move(callback).Run(
        std::vector<std::string>{"INVALID:attestationNotAllowed"},
        /*valid=*/false);
    return;
  }
  GetCertificateInternal(
      /*force_new_key=*/false,
      std::make_unique<Session>(std::move(callback), account_id, user_key));
}

void SoftBindAttestationFlowImpl::GetCertificateInternal(
    bool force_new_key,
    std::unique_ptr<Session> session) {
  AccountId account_id(session->GetAccountId());
  AttestationFlow::CertificateCallback certificate_callback =
      base::BindOnce(&SoftBindAttestationFlowImpl::OnCertificateReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(session));
  attestation_flow_->GetCertificate(
      /*certificate_profile=*/PROFILE_SOFT_BIND_CERTIFICATE,
      /*account_id=*/account_id,
      /*request_origin=*/std::string(),
      /*force_new_key=*/force_new_key,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kSoftBindKey, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(certificate_callback));
}

void SoftBindAttestationFlowImpl::OnCertificateReady(
    std::unique_ptr<Session> session,
    AttestationStatus operation_status,
    const std::string& certificate_chain) {
  if (!session->IsTimerRunning()) {
    LOG(WARNING) << "Certificate ready but already timed out";
    return;
  }
  session->StopTimer();
  if (operation_status != ATTESTATION_SUCCESS) {
    LOG(ERROR) << "Attestation unsuccessful, not verified";
    session->ReportFailure("notVerified");
    return;
  }
  CertificateExpiryStatus expiry_status = CheckExpiry(certificate_chain);
  if (expiry_status == CertificateExpiryStatus::kExpired) {
    if (session->ResetTimer()) {
      GetCertificateInternal(/*force_new_key=*/true, std::move(session));
    } else {
      session->ReportFailure("tooManyRetries");
    }
    return;
  }
  VLOG(1) << "Intermediate certificate obtained successfully";

  // Construct a short-lived leaf certificate for the given user key and
  // sign with the intermediate soft-bind attestation key. This binding of
  // a hardware-backed attestation key (the intermediate) to a software key
  // (the user key), is the fundamental operation of the soft-bind
  // attestation scheme.

  std::string user_key(session->GetUserKey());
  securemessage::GenericPublicKey generic_public_key;
  generic_public_key.ParseFromString(user_key);
  std::string raw_x(generic_public_key.ec_p256_public_key().x());
  bssl::UniquePtr<BIGNUM> x(BN_new());
  BN_bin2bn(reinterpret_cast<const uint8_t*>(raw_x.data()), raw_x.size(),
            x.get());
  std::string raw_y(generic_public_key.ec_p256_public_key().y());
  bssl::UniquePtr<BIGNUM> y(BN_new());
  BN_bin2bn(reinterpret_cast<const uint8_t*>(raw_y.data()), raw_y.size(),
            y.get());

  bssl::UniquePtr<EC_GROUP> ec_group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> ec_point(EC_POINT_new(ec_group.get()));
  if (!EC_POINT_set_affine_coordinates(ec_group.get(), ec_point.get(), x.get(),
                                       y.get(), /* ctx= */ nullptr)) {
    LOG(ERROR) << "SoftBindAttestation: Could not set user key coordinates: "
               << ERR_error_string(ERR_get_error(), nullptr);
    session->ReportFailure("couldNotSetUserKeyCoordsNotOnCurve");
    return;
  }
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  if (!EC_KEY_set_public_key(ec_key.get(), ec_point.get())) {
    LOG(ERROR) << "SoftBindAttestation: Could not set user key public key: "
               << ERR_error_string(ERR_get_error(), nullptr);
    session->ReportFailure("couldNotSetUserKeyPublicKey");
    return;
  }
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (!EVP_PKEY_assign_EC_KEY(pkey.get(), ec_key.release())) {
    LOG(ERROR) << "SoftBindAttestation: Could not assign user key pkey: "
               << ERR_error_string(ERR_get_error(), nullptr);
    session->ReportFailure("couldNotAssignUserKeyPkey");
    return;
  }

  base::Time now = base::Time::Now();
  std::string leaf_cert;
  GenerateLeafCert(pkey.get(), now, now + kLeafCertValidityWindow, &leaf_cert);

  std::string tbs_cert(leaf_cert);

  ::attestation::SignRequest request;
  request.set_username(
      cryptohome::Identification(session->GetAccountId()).id());
  std::string key_name(kSoftBindKey);
  request.set_key_label(std::move(key_name));
  request.set_data_to_sign(std::move(leaf_cert));
  AttestationClient::Get()->Sign(
      request,
      base::BindOnce(&SoftBindAttestationFlowImpl::OnCertificateSigned,
                     weak_ptr_factory_.GetWeakPtr(), std::move(session),
                     tbs_cert, certificate_chain,
                     expiry_status == CertificateExpiryStatus::kExpiringSoon));
}

void SoftBindAttestationFlowImpl::OnCertificateSigned(
    std::unique_ptr<Session> session,
    const std::string& tbs_cert,
    const std::string& certificate_chain,
    bool should_renew,
    const ::attestation::SignReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Could not sign attestation certificate: " << reply.status();
    session->ReportFailure("couldNotSignCert");
    return;
  }

  bssl::ScopedCBB cbb;
  CBB signed_cert, signature, alg, alg_oid, alg_null;
  uint8_t* signed_cert_bytes;
  size_t signed_cert_len;
  if (!CBB_init(cbb.get(), 64) ||
      !CBB_add_asn1(cbb.get(), &signed_cert, CBS_ASN1_SEQUENCE) ||
      !CBB_add_bytes(&signed_cert,
                     reinterpret_cast<const uint8_t*>(tbs_cert.data()),
                     tbs_cert.size()) ||
      !CBB_add_asn1(&signed_cert, &alg, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&alg, &alg_oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&alg_oid, kRsaSha256Oid, 9) ||
      !CBB_add_asn1(&alg, &alg_null, CBS_ASN1_NULL) ||
      !CBB_add_asn1(&signed_cert, &signature, CBS_ASN1_BITSTRING) ||
      !CBB_add_u8(&signature, 0) ||
      !CBB_add_bytes(&signature,
                     reinterpret_cast<const uint8_t*>(reply.signature().data()),
                     reply.signature().size()) ||
      !CBB_flush(&signature) || !CBB_flush(&signed_cert) ||
      !CBB_finish(cbb.get(), &signed_cert_bytes, &signed_cert_len)) {
    LOG(ERROR) << "Could not sign attestation certificate";
    session->ReportFailure("couldNotSignCertCbb");
    return;
  }
  std::string der_encoded_cert;
  der_encoded_cert.assign(reinterpret_cast<char*>(signed_cert_bytes),
                          signed_cert_len);
  bssl::UniquePtr<uint8_t> delete_signed_cert_bytes(signed_cert_bytes);
  std::string pem_encoded_cert;
  net::X509Certificate::GetPEMEncodedFromDER(der_encoded_cert,
                                             &pem_encoded_cert);

  std::vector<std::string> cert_chain_with_leaf = {pem_encoded_cert};

  bssl::PEMTokenizer pem_tokenizer(certificate_chain, {"CERTIFICATE"});
  while (pem_tokenizer.GetNext()) {
    std::string pem_encoded_intermediate_cert;
    net::X509Certificate::GetPEMEncodedFromDER(pem_tokenizer.data(),
                                               &pem_encoded_intermediate_cert);
    cert_chain_with_leaf.push_back(pem_encoded_intermediate_cert);
  }

  session->ReportSuccess(cert_chain_with_leaf);

  // If certificate is close to expiry, send a new request to ensure
  // uninterrupted continuity.
  if (should_renew && renewals_in_progress_.count(certificate_chain) == 0) {
    renewals_in_progress_.insert(certificate_chain);
    AttestationFlow::CertificateCallback renew_callback = base::BindOnce(
        &SoftBindAttestationFlowImpl::RenewCertificateCallback,
        weak_ptr_factory_.GetWeakPtr(), std::move(certificate_chain));
    attestation_flow_->GetCertificate(
        /*certificate_profile=*/PROFILE_SOFT_BIND_CERTIFICATE,
        /*account_id=*/session->GetAccountId(),
        /*request_origin=*/std::string(), /*force_new_key=*/true,
        /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
        /*key_name=*/kSoftBindKey, /*profile_specific_data=*/std::nullopt,
        /*callback=*/std::move(renew_callback));
  }
}

bool SoftBindAttestationFlowImpl::IsAttestationAllowedByPolicy() const {
  bool enabled_for_device = false;
  if (!CrosSettings::Get()->GetBoolean(kAttestationForContentProtectionEnabled,
                                       &enabled_for_device)) {
    LOG(ERROR) << "Failed to get device attestation policy setting.";
    return false;
  }
  if (!enabled_for_device) {
    LOG(ERROR) << "Soft key bind attestation denied because Verified Access is "
               << "disabled for the device.";
    return false;
  }
  return true;
}

// TODO(b/185520169): create utility method for both this and content protection
CertificateExpiryStatus SoftBindAttestationFlowImpl::CheckExpiry(
    const std::string& certificate_chain) {
  int num_certificates = 0;
  bssl::PEMTokenizer pem_tokenizer(certificate_chain, {"CERTIFICATE"});
  while (pem_tokenizer.GetNext()) {
    ++num_certificates;
    scoped_refptr<net::X509Certificate> x509 =
        net::X509Certificate::CreateFromBytes(
            base::as_byte_span(pem_tokenizer.data()));
    if (!x509.get() || x509->valid_expiry().is_null()) {
      // This logic intentionally fails open. In theory this should not happen
      // but in practice parsing X.509 can be brittle and there are a lot of
      // factors including which underlying module is parsing the certificate,
      // whether that module performs more checks than just ASN.1/DER format,
      // and the server module that generated the certificate(s). Renewal is
      // expensive so we only renew certificates with good evidence that they
      // have expired or will soon expire; if we don't know, we don't renew.
      LOG(WARNING) << "Failed to parse certificate, cannot check expiry";
      return CertificateExpiryStatus::kInvalidX509;
    }
    if (base::Time::Now() > x509->valid_expiry()) {
      return CertificateExpiryStatus::kExpired;
    }
    if ((x509->valid_expiry() - base::Time::Now()) < kExpiryThresholdDays) {
      return CertificateExpiryStatus::kExpiringSoon;
    }
  }
  if (num_certificates == 0) {
    LOG(WARNING) << "Failed to parse certificate chain, cannot check expiry";
    return CertificateExpiryStatus::kInvalidPemChain;
  }
  return CertificateExpiryStatus::kValid;
}

void SoftBindAttestationFlowImpl::RenewCertificateCallback(
    const std::string& old_certificate_chain,
    AttestationStatus operation_status,
    const std::string& certificate_chain) {
  renewals_in_progress_.erase(old_certificate_chain);
  if (operation_status != ATTESTATION_SUCCESS) {
    LOG(WARNING) << "Failed to renew certificate";
    return;
  }
  VLOG(1) << "Certificate successfully renewed";
}

bool SoftBindAttestationFlowImpl::GenerateLeafCert(
    EVP_PKEY* key,
    base::Time not_valid_before,
    base::Time not_valid_after,
    std::string* der_encoded_cert) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::ScopedCBB cbb;
  CBB cert, version, validity, alg, alg_oid, alg_null;
  uint8_t* cert_bytes;
  size_t cert_len;
  uint64_t serial_number;
  crypto::RandBytes(base::byte_span_from_ref(serial_number));
  if (!CBB_init(cbb.get(), 64) ||
      !CBB_add_asn1(cbb.get(), &cert, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&cert, &version,
                    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0) ||
      !CBB_add_asn1_uint64(&version, 2) ||
      !CBB_add_asn1_uint64(&cert, serial_number) ||
      !CBB_add_asn1(&cert, &alg, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&alg, &alg_oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&alg_oid, kRsaSha256Oid, 9) ||
      !CBB_add_asn1(&alg, &alg_null, CBS_ASN1_NULL) ||
      !net::x509_util::AddName(&cert, kLeafCertIssuerName) ||
      !CBB_add_asn1(&cert, &validity, CBS_ASN1_SEQUENCE) ||
      !net::x509_util::CBBAddTime(&validity, not_valid_before) ||
      !net::x509_util::CBBAddTime(&validity, not_valid_after) ||
      !net::x509_util::AddName(&cert, kLeafCertSubjectName) ||
      !EVP_marshal_public_key(&cert, key)) {  // subjectPublicKeyInfo
    return false;
  }

  CBB outer_extensions, extensions;
  if (!CBB_add_asn1(&cert, &outer_extensions,
                    3 | CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED) ||
      !CBB_add_asn1(&outer_extensions, &extensions, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  if (!AddCriticalExtension(&extensions, kBasicConstraintsOid, 3,
                            kBasicConstraintsContents, 2) ||
      !AddCriticalExtension(&extensions, kKeyUsageOid, 3, kKeyUsageContents,
                            4)) {
    return false;
  }

  if (!CBB_flush(&cert)) {
    return false;
  }

  if (!CBB_finish(cbb.get(), &cert_bytes, &cert_len)) {
    return false;
  }

  der_encoded_cert->assign(reinterpret_cast<char*>(cert_bytes), cert_len);
  bssl::UniquePtr<uint8_t> delete_cert_bytes(cert_bytes);

  return true;
}

}  // namespace attestation
}  // namespace ash
