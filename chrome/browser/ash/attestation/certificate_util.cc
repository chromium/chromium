// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/certificate_util.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace ash {
namespace attestation {

CertificateExpiryStatus CheckCertificateExpiry(
    const std::string& certificate_chain,
    base::TimeDelta expiry_threshold) {
  bool is_expiring_soon = false;
  bool invalid_certificate_found = false;
  bool any_certificate_found = false;
  bssl::PEMTokenizer pem_tokenizer(certificate_chain, {"CERTIFICATE"});
  while (pem_tokenizer.GetNext()) {
    any_certificate_found = true;
    scoped_refptr<net::X509Certificate> x509 =
        net::X509Certificate::CreateFromBytes(
            base::as_byte_span(pem_tokenizer.data()));
    if (!x509.get() || x509->valid_expiry().is_null()) {
      // In theory this should not happen but in practice parsing X.509 can be
      // brittle and there are a lot of factors including which underlying
      // module is parsing the certificate, whether that module performs more
      // checks than just ASN.1/DER format, and the server module that generated
      // the certificate(s).
      invalid_certificate_found = true;
      continue;
    }
    const base::Time current_time = base::Time::Now();
    if (current_time > x509->valid_expiry()) {
      // Found valid expired token, other tokens can be ignored.
      return CertificateExpiryStatus::kExpired;
    }
    if (current_time + expiry_threshold > x509->valid_expiry()) {
      // Check this flag after the loop to not to loose possible expired tokens.
      is_expiring_soon = true;
    }
  }
  if (is_expiring_soon) {
    // Found at least one expiring soon token and can ignore invalid tokens.
    return CertificateExpiryStatus::kExpiringSoon;
  }
  if (invalid_certificate_found) {
    return CertificateExpiryStatus::kInvalidX509;
  }
  if (!any_certificate_found) {
    return CertificateExpiryStatus::kInvalidPemChain;
  }
  return CertificateExpiryStatus::kValid;
}

std::string CertificateExpiryStatusToString(CertificateExpiryStatus status) {
  switch (status) {
    case CertificateExpiryStatus::kValid:
      return "Valid";
    case CertificateExpiryStatus::kExpiringSoon:
      return "ExpiringSoon";
    case CertificateExpiryStatus::kExpired:
      return "Expired";
    case CertificateExpiryStatus::kInvalidPemChain:
      return "InvalidPemChain";
    case CertificateExpiryStatus::kInvalidX509:
      return "InvalidX509";
  }

  NOTREACHED_IN_MIGRATION() << "Unknown certificate status";
}

}  // namespace attestation
}  // namespace ash
