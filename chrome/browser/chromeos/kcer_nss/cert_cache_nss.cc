// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kcer_nss/cert_cache_nss.h"

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "chromeos/components/kcer/kcer.h"
#include "net/cert/scoped_nss_types.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace kcer::internal {

namespace {

// Extracts a pointer to bytes and length of the certificate itself and compares
// based on them. `cert` from the constructors must remain valid throughout the
// lifetime of CmpAdapter.
class CmpAdapter {
 public:
  explicit CmpAdapter(const scoped_refptr<const Cert>& cert)
      : data_(CRYPTO_BUFFER_data(cert->GetX509Cert()->cert_buffer()),
              CRYPTO_BUFFER_len(cert->GetX509Cert()->cert_buffer())) {}
  explicit CmpAdapter(const net::ScopedCERTCertificate& cert)
      : data_(cert->derCert.data, cert->derCert.len) {}

  bool operator<(const CmpAdapter& other) {
    return base::ranges::lexicographical_compare(data_, other.data_);
  }

 private:
  base::span<const uint8_t> data_;
};

}  // namespace

//====================== CertCacheNss ==========================================

CertCacheNss::CertCacheNss() = default;
CertCacheNss::CertCacheNss(CertCacheNss&&) = default;
CertCacheNss& CertCacheNss::operator=(CertCacheNss&&) = default;
CertCacheNss::~CertCacheNss() = default;

CertCacheNss::CertCacheNss(base::span<scoped_refptr<const Cert>> certs)
    : certs_(certs.begin(), certs.end()) {}

scoped_refptr<const Cert> CertCacheNss::FindCert(
    const net::ScopedCERTCertificate& cert) const {
  auto iter = certs_.find(cert);
  return (iter != certs_.end()) ? *iter : nullptr;
}

std::vector<scoped_refptr<const Cert>> CertCacheNss::GetAllCerts() const {
  return std::vector<scoped_refptr<const Cert>>(certs_.begin(), certs_.end());
}

//====================== CertCacheNss::CertComparator ==========================

bool CertCacheNss::CertComparator::operator()(
    const scoped_refptr<const Cert>& a,
    const net::ScopedCERTCertificate& b) const {
  return CmpAdapter(a) < CmpAdapter(b);
}
bool CertCacheNss::CertComparator::operator()(
    const net::ScopedCERTCertificate& a,
    const scoped_refptr<const Cert>& b) const {
  return CmpAdapter(a) < CmpAdapter(b);
}
bool CertCacheNss::CertComparator::operator()(
    const scoped_refptr<const Cert>& a,
    const scoped_refptr<const Cert>& b) const {
  return CmpAdapter(a) < CmpAdapter(b);
}

}  // namespace kcer::internal
