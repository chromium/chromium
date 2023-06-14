// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PKCS12_READER_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PKCS12_READER_H_

#include "base/containers/span.h"
#include "chrome/browser/chromeos/platform_keys/chaps_slot_session.h"
#include "third_party/boringssl/src/include/openssl/pkcs7.h"

namespace chromeos::platform_keys {

enum class Pkcs12ReaderStatusCode {
  kSuccess = 0,
  kCreateKeyFailed = 1,
  kCertificateDataMissed = 2,
  kCreateCertFailed = 3,
  kKeyDataMissed = 4,
  kKeyExtractionFailed = 5,
  kChapsSessionMissed = 6,
  kPkcs12CertDerMissed = 7,
  kPkcs12CertDerFailed = 8,
  kPkcs12CertIssuerNameMissed = 9,
  kPkcs12CertIssuerDerNameFailed = 10,
  kPkcs12CertSubjectNameMissed = 11,
  kPkcs12CertSubjectNameDerFailed = 12,
  kPkcs12CertSerialNumberMissed = 13,
  kPkcs12CertSerialNumberDerFailed = 14,
  kKeyAttrDataMissing = 15,
  kFailureDuringCertImport = 16,
  kFailedToParsePkcs12Data = 17,
  kMissedPkcs12Data = 18,
  kPkcs12LabelCreationFailed = 19,
};

// Class helper for operations with X509 certificates data which are required
// for storing keys and certificates in Chaps.
class Pkcs12Reader {
 public:
  Pkcs12Reader() = default;

  virtual ~Pkcs12Reader() = default;

  // Populates key and certificates (`key`, `certs`) from the PKCS#12 object
  // `pkcs12_data` protected by the `password`. Returns status code.
  virtual Pkcs12ReaderStatusCode GetPkcs12KeyAndCerts(
      const std::vector<uint8_t>& pkcs12_data,
      const std::string& password,
      bssl::UniquePtr<EVP_PKEY>& key,
      bssl::UniquePtr<STACK_OF(X509)>& certs) const;

  // Populates der encoded certificate and its size (`cert_der`,
  // `cert_der_size`) from X509 (`cert`). Returns status code.
  virtual Pkcs12ReaderStatusCode GetDerEncodedCert(
      X509* cert,
      bssl::UniquePtr<uint8_t>& cert_der,
      int& cert_der_size) const;

  // Populates der encoded issuer name and its size (`issuer_name_data`) from
  // X509 (`cert`). `issuer_name_data` remains valid only as long as the cert is
  // alive because it is only referencing data. Returns status code.
  virtual Pkcs12ReaderStatusCode GetIssuerNameDer(
      X509* cert,
      base::span<const uint8_t>& issuer_name_data) const;

  // Populates der encoded subject name and its size (`subject_name_data`) from
  // X509 (`cert`). `subject_name_data` remains valid only as long as the cert
  // is alive because it is only referencing data. Returns status code.
  virtual Pkcs12ReaderStatusCode GetSubjectNameDer(
      X509* cert,
      base::span<const uint8_t>& subject_name_data) const;

  // Populates der encoded serial number and its size (`serial_number_der`,
  // `serial_number_der_size`) from X509 (`cert`). Returns status code.
  virtual Pkcs12ReaderStatusCode GetSerialNumberDer(
      X509* cert,
      bssl::UniquePtr<uint8_t>& serial_number_der,
      int& serial_number_der_size) const;

  // Populates label (`label`) from X509 (`cert`). Returns status code.
  virtual Pkcs12ReaderStatusCode GetLabel(X509* cert, std::string& label) const;

  // Converts BIGNUM (`bignum`) to bytes.
  virtual std::vector<uint8_t> BignumToBytes(const BIGNUM* bignum) const;
};

}  // namespace chromeos::platform_keys

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PKCS12_READER_H_
