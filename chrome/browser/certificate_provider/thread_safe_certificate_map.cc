// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_provider/thread_safe_certificate_map.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "chromeos/components/certificate_provider/certificate_info.h"
#include "net/base/hash_value.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace chromeos {
namespace certificate_provider {
namespace {

std::string GetSubjectPublicKeyInfo(const net::X509Certificate& certificate) {
  std::string_view spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer()),
          &spki_bytes)) {
    return {};
  }
  return std::string(spki_bytes);
}

}  // namespace

ThreadSafeCertificateMap::ThreadSafeCertificateMap() {}

ThreadSafeCertificateMap::~ThreadSafeCertificateMap() {}

void ThreadSafeCertificateMap::UpdateCertificatesForExtension(
    const std::string& extension_id,
    const CertificateInfoList& certificates) {
  base::AutoLock auto_lock(lock_);
  RemoveCertificatesProvidedByExtension(extension_id);

  for (const CertificateInfo& cert_info : certificates) {
    const net::SHA256HashValue fingerprint =
        net::X509Certificate::CalculateFingerprint256(
            cert_info.certificate->cert_buffer());
    fingerprint_to_extension_and_cert_[fingerprint][extension_id] = cert_info;

    const std::string spki = GetSubjectPublicKeyInfo(*cert_info.certificate);
    spki_to_extension_and_cert_[spki][extension_id] = cert_info;
  }
}

std::vector<scoped_refptr<net::X509Certificate>>
ThreadSafeCertificateMap::GetCertificates() {
  base::AutoLock auto_lock(lock_);
  std::vector<scoped_refptr<net::X509Certificate>> certificates;
  for (const auto& fingerprint_entry : fingerprint_to_extension_and_cert_) {
    const ExtensionToCertificateMap* extension_to_certificate_map =
        &fingerprint_entry.second;
    if (!extension_to_certificate_map->empty()) {
      // If there are multiple entries with the same fingerprint, they are the
      // same certificate as SHA256 should not have collisions.
      // Since we need each certificate only once, we can return any entry.
      certificates.push_back(
          extension_to_certificate_map->begin()->second.certificate);
    }
  }
  return certificates;
}

bool ThreadSafeCertificateMap::LookUpCertificate(
    const net::X509Certificate& cert,
    bool* is_currently_provided,
    CertificateInfo* info,
    std::string* extension_id) {
  *is_currently_provided = false;
  const net::SHA256HashValue fingerprint =
      net::X509Certificate::CalculateFingerprint256(cert.cert_buffer());

  base::AutoLock auto_lock(lock_);
  const auto it = fingerprint_to_extension_and_cert_.find(fingerprint);
  if (it == fingerprint_to_extension_and_cert_.end())
    return false;

  ExtensionToCertificateMap* const map = &it->second;
  if (!map->empty()) {
    // If multiple entries are found, it is unspecified which is returned.
    const auto map_entry = map->begin();
    *is_currently_provided = true;
    *info = map_entry->second;
    *extension_id = map_entry->first;
  }
  return true;
}

bool ThreadSafeCertificateMap::LookUpCertificateBySpki(
    const std::string& subject_public_key_info,
    bool* is_currently_provided,
    CertificateInfo* info,
    std::string* extension_id) {
  *is_currently_provided = false;
  base::AutoLock auto_lock(lock_);
  const auto it = spki_to_extension_and_cert_.find(subject_public_key_info);
  if (it == spki_to_extension_and_cert_.end())
    return false;

  ExtensionToCertificateMap* const map = &it->second;
  if (!map->empty()) {
    // If multiple entries are found, it is unspecified which is returned.
    const auto map_entry = map->begin();
    *is_currently_provided = true;
    *info = map_entry->second;
    *extension_id = map_entry->first;
  }
  return true;
}

void ThreadSafeCertificateMap::RemoveExtension(
    const std::string& extension_id) {
  base::AutoLock auto_lock(lock_);
  RemoveCertificatesProvidedByExtension(extension_id);
}

void ThreadSafeCertificateMap::RemoveCertificatesProvidedByExtension(
    const std::string& extension_id) {
  for (auto& entry : fingerprint_to_extension_and_cert_) {
    ExtensionToCertificateMap* map = &entry.second;
    map->erase(extension_id);
  }

  for (auto& entry : spki_to_extension_and_cert_) {
    ExtensionToCertificateMap* map = &entry.second;
    map->erase(extension_id);
  }
}

}  // namespace certificate_provider
}  // namespace chromeos
