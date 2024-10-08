// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database_test_util.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/sha2.h"

namespace net {

ServerCertificateDatabase::CertInformation MakeCertInfo(
    std::string_view der_cert,
    chrome_browser_server_certificate_database::CertificateTrust::
        CertificateTrustType trust_type) {
  ServerCertificateDatabase::CertInformation cert_info;
  cert_info.sha256hash_hex =
      base::HexEncode(crypto::SHA256HashString(der_cert));
  cert_info.cert_metadata.mutable_trust()->set_trust_type(trust_type);
  cert_info.der_cert = base::ToVector(base::as_byte_span(der_cert));
  return cert_info;
}

}  // namespace net
