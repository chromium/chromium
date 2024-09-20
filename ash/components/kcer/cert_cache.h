// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_CERT_CACHE_H_
#define ASH_COMPONENTS_KCER_CERT_CACHE_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"

namespace kcer {

class Cert;

namespace internal {

// Cache for a collection of scoped_refptr<const Cert>-s.
class COMPONENT_EXPORT(KCER) CertCache {
 public:
  // Empty cache.
  CertCache();
  // Cache that contains `certs` (`certs` can be unsorted).
  explicit CertCache(base::span<scoped_refptr<const Cert>> certs);
  CertCache(CertCache&&);
  CertCache& operator=(CertCache&&);
  ~CertCache();

  // Searches for Cert certificate with the same content as `cert_der` and
  // returns a scoped_refptr<const Cert> on success, a nullptr if `cert` was not
  // found.
  scoped_refptr<const Cert> FindCert(
      const base::span<const uint8_t>& cert_der) const;

  // Returns ref-counting pointers to all certificate from the cache.
  std::vector<scoped_refptr<const Cert>> GetAllCerts() const;

 private:
  // Comparator for sorting scoped_refptr<const Cert>-s and for enabling
  // std::set::find() using the base::span<const uint8_t> representation of a
  // cert.
  struct CertComparator {
    using is_transparent = void;

    bool operator()(const scoped_refptr<const Cert>& a,
                    const base::span<const uint8_t>& b) const;
    bool operator()(const base::span<const uint8_t>& a,
                    const scoped_refptr<const Cert>& b) const;
    bool operator()(const scoped_refptr<const Cert>& a,
                    const scoped_refptr<const Cert>& b) const;
  };

  std::set<scoped_refptr<const Cert>, CertComparator> certs_;
};

}  // namespace internal
}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_CERT_CACHE_H_
