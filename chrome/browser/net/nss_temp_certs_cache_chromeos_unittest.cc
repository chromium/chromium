// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_temp_certs_cache_chromeos.h"

#include <cert.h>
#include <certdb.h>
#include <secitem.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "net/cert/x509_certificate.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace network {

namespace {

class NSSTempCertsCacheChromeOSTest : public testing::Test {
 public:
  NSSTempCertsCacheChromeOSTest() {}

  NSSTempCertsCacheChromeOSTest(const NSSTempCertsCacheChromeOSTest&) = delete;
  NSSTempCertsCacheChromeOSTest& operator=(
      const NSSTempCertsCacheChromeOSTest&) = delete;

  ~NSSTempCertsCacheChromeOSTest() override {}

 protected:
  // Checks if the certificate stored in |pem_cert_file| can be found in the
  // default NSS certificate database using CERT_FindCertByName.
  // Stores the result in *|out_available|.
  // Note: This funcion uses ASSERT_ macros, so the caller must verify for
  // failures after it returns.
  void CheckIsCertificateAvailable(const base::FilePath& pem_cert_file,
                                   bool* out_available) {
    std::string cert_contents_buffer;
    bssl::der::Input subject;
    ASSERT_NO_FATAL_FAILURE(GetCertificateSubjectDN(
        pem_cert_file, &cert_contents_buffer, &subject));

    SECItem subject_item;
    subject_item.len = subject.size();
    subject_item.data = const_cast<unsigned char*>(subject.data());

    net::ScopedCERTCertificate found_cert(
        CERT_FindCertByName(CERT_GetDefaultCertDB(), &subject_item));
    *out_available = static_cast<bool>(found_cert);
  }

  // Determines the subject DN of the certificate stored in
  // |pem_cert_file|. Stores the result in *|out_subject|.
  // The der::Input data structure contains unowned pointers into the
  // certificate data buffer. The caller must pass a buffer in
  // |cert_contents_buffer| and ensure to only use *|out_subject| while
  // *|cert_contents_buffer| is in scope.
  // Note: This function uses ASSERT_ macros, so the caller must verify for
  // failures after it returns.
  void GetCertificateSubjectDN(const base::FilePath& pem_cert_file,
                               std::string* cert_contents_buffer,
                               bssl::der::Input* out_subject) {
    std::string file_data;
    ASSERT_TRUE(base::ReadFileToString(pem_cert_file, &file_data));

    std::vector<std::string> pem_headers;
    pem_headers.push_back("CERTIFICATE");
    bssl::PEMTokenizer pem_tokenizer(file_data, pem_headers);
    ASSERT_TRUE(pem_tokenizer.GetNext());
    *cert_contents_buffer = pem_tokenizer.data();

    // Parsing the certificate.
    bssl::der::Input tbs_certificate_tlv;
    bssl::der::Input signature_algorithm_tlv;
    bssl::der::BitString signature_value;
    bssl::CertErrors errors;
    ASSERT_TRUE(bssl::ParseCertificate(
        bssl::der::Input(*cert_contents_buffer), &tbs_certificate_tlv,
        &signature_algorithm_tlv, &signature_value, &errors));

    bssl::ParsedTbsCertificate tbs;
    bssl::ParseCertificateOptions options;
    options.allow_invalid_serial_numbers = true;
    ASSERT_TRUE(
        bssl::ParseTbsCertificate(tbs_certificate_tlv, options, &tbs, nullptr));
    *out_subject = tbs.subject_tlv;
  }
};

// Checks that a certificate made available through the
// NSSTempCertsCacheChromeOS can be found by NSS. We specifically check for
// lookup through the CERT_FindCertByName function, as this is what is used in
// client certificate matching (see MatchClientCertificateIssuers in
// net/third_party/nss/ssl/cmpcert.cc). Additionally, checks that the
// certificate is not available after the NSSTempCertsCacheChromeOS goes out of
// scope.
TEST_F(NSSTempCertsCacheChromeOSTest, CertMadeAvailable) {
  base::FilePath cert_file_path =
      net::GetTestCertsDirectory().AppendASCII("client_1_ca.pem");
  {
    std::string x509_authority_cert;
    ASSERT_TRUE(base::ReadFileToString(cert_file_path, &x509_authority_cert));
    net::CertificateList x509_authority_certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            base::as_bytes(base::make_span(x509_authority_cert)),
            net::X509Certificate::Format::FORMAT_AUTO);

    NSSTempCertsCacheChromeOS cache(x509_authority_certs);

    bool cert_available = false;
    ASSERT_NO_FATAL_FAILURE(
        CheckIsCertificateAvailable(cert_file_path, &cert_available));
    EXPECT_TRUE(cert_available);
  }

  bool cert_available_no_cache = true;
  ASSERT_NO_FATAL_FAILURE(
      CheckIsCertificateAvailable(cert_file_path, &cert_available_no_cache));
  EXPECT_FALSE(cert_available_no_cache);
}

}  // namespace
}  // namespace network
