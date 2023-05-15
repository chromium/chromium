// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KCER_NSS_CERT_CACHE_NSS_H_
#define CHROME_BROWSER_CHROMEOS_KCER_NSS_CERT_CACHE_NSS_H_

#include <set>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "net/cert/scoped_nss_types.h"

namespace kcer {

class Cert;

namespace internal {

// Cache for a collection of scoped_refptr<const Cert>-s.
class CertCacheNss {
 public:
  // Empty cache.
  CertCacheNss();
  // Cache that contains `certs` (`certs` can be unsorted).
  explicit CertCacheNss(base::span<scoped_refptr<const Cert>> certs);
  CertCacheNss(CertCacheNss&&);
  CertCacheNss& operator=(CertCacheNss&&);
  ~CertCacheNss();

  // Searches for Cert certificate with the same content as `cert` and returns a
  // scoped_refptr<const Cert> on success, a nullptr if `cert` was not found.
  scoped_refptr<const Cert> FindCert(
      const net::ScopedCERTCertificate& cert) const;
  // Returns ref-counting pointers to all certificate from the cache.
  std::vector<scoped_refptr<const Cert>> GetAllCerts() const;

 private:
  // Comparator for sorting scoped_refptr<const Cert>-s and for enabling
  // std::set::find() using the net::ScopedCERTCertificate representation of a
  // cert.
  struct CertComparator {
    using is_transparent = void;

    bool operator()(const scoped_refptr<const Cert>& a,
                    const net::ScopedCERTCertificate& b) const;
    bool operator()(const net::ScopedCERTCertificate& a,
                    const scoped_refptr<const Cert>& b) const;
    bool operator()(const scoped_refptr<const Cert>& a,
                    const scoped_refptr<const Cert>& b) const;
  };

  std::set<scoped_refptr<const Cert>, CertComparator> certs_;
};

}  // namespace internal
}  // namespace kcer

#endif  // CHROME_BROWSER_CHROMEOS_KCER_NSS_CERT_CACHE_NSS_H_
