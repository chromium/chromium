// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NSS_TEMP_CERTS_CACHE_CHROMEOS_H_
#define CHROME_BROWSER_NET_NSS_TEMP_CERTS_CACHE_CHROMEOS_H_

#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"

namespace network {

// Holds NSS temporary certificates in memory as ScopedCERTCertificates, making
// them available e.g. for client certificate discovery.
class NSSTempCertsCacheChromeOS {
 public:
  explicit NSSTempCertsCacheChromeOS(const net::CertificateList& certificates);

  NSSTempCertsCacheChromeOS(const NSSTempCertsCacheChromeOS&) = delete;
  NSSTempCertsCacheChromeOS& operator=(const NSSTempCertsCacheChromeOS&) =
      delete;

  ~NSSTempCertsCacheChromeOS();

 private:
  // The actual cache of NSS temporary certificates.
  // Don't delete this field, even if it looks unused!
  // This is a list which owns ScopedCERTCertificate objects. This is sufficient
  // for NSS to be able to find them using CERT_FindCertByName, which is enough
  // for them to be used as intermediate certificates during client certificate
  // matching. Note that when the ScopedCERTCertificate objects go out of scope,
  // they don't necessarily become unavailable in NSS due to caching behavior.
  // However, this is not an issue, as these certificates are not imported into
  // permanent databases, nor are the trust settings mutated to trust them.
  net::ScopedCERTCertificateList temp_certs_;
};

}  // namespace network

#endif  // CHROME_BROWSER_NET_NSS_TEMP_CERTS_CACHE_CHROMEOS_H_
