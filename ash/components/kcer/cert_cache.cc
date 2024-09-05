// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/cert_cache.h"

#include <stdint.h>

#include <vector>

#include "ash/components/kcer/kcer.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "net/cert/x509_util.h"

namespace kcer::internal {

namespace {

// Extracts the bytes (DER encoding) of a certificate itself and compares based
// on them. `cert` from the constructors must remain valid throughout the
// lifetime of CmpAdapter.
class CmpAdapter {
 public:
  explicit CmpAdapter(const scoped_refptr<const Cert>& cert)
      : data_(CRYPTO_BUFFER_data(cert->GetX509Cert()->cert_buffer()),
              CRYPTO_BUFFER_len(cert->GetX509Cert()->cert_buffer())) {}
  explicit CmpAdapter(const base::span<const uint8_t>& cert) : data_(cert) {}

  bool operator<(const CmpAdapter& other) {
    return base::ranges::lexicographical_compare(data_, other.data_);
  }

 private:
  base::raw_span<const uint8_t> data_;
};

}  // namespace

//====================== CertCache ==========================================

CertCache::CertCache() = default;
CertCache::CertCache(CertCache&&) = default;
CertCache& CertCache::operator=(CertCache&&) = default;
CertCache::~CertCache() = default;

CertCache::CertCache(base::span<scoped_refptr<const Cert>> certs)
    : certs_(certs.begin(), certs.end()) {}

scoped_refptr<const Cert> CertCache::FindCert(
    const base::span<const uint8_t>& cert_der) const {
  auto iter = certs_.find(cert_der);
  return (iter != certs_.end()) ? *iter : nullptr;
}

std::vector<scoped_refptr<const Cert>> CertCache::GetAllCerts() const {
  return std::vector<scoped_refptr<const Cert>>(certs_.begin(), certs_.end());
}

//====================== CertCache::CertComparator ==========================

bool CertCache::CertComparator::operator()(
    const scoped_refptr<const Cert>& a,
    const base::span<const uint8_t>& b) const {
  return CmpAdapter(a) < CmpAdapter(b);
}
bool CertCache::CertComparator::operator()(
    const base::span<const uint8_t>& a,
    const scoped_refptr<const Cert>& b) const {
  return CmpAdapter(a) < CmpAdapter(b);
}
bool CertCache::CertComparator::operator()(
    const scoped_refptr<const Cert>& a,
    const scoped_refptr<const Cert>& b) const {
  return CmpAdapter(a) < CmpAdapter(b);
}

}  // namespace kcer::internal
