// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_unittest_util.h"

#include "base/files/file_util.h"

// Reads a single PEM-encoded certificate from the testdata directory.
// Returns nullptr on failure.
scoped_refptr<net::X509Certificate> ReadTestCertificate(
    base::FilePath test_cert_path,
    const std::string& filename) {
  std::string cert_data;
  if (!base::ReadFileToString(test_cert_path.AppendASCII(filename),
                              &cert_data)) {
    return nullptr;
  }
  net::CertificateList certs =
      net::X509Certificate::CreateCertificateListFromBytes(
          base::as_bytes(base::make_span(cert_data)),
          net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  return certs.empty() ? nullptr : certs[0];
}
