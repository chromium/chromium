// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/thread_safe_certificate_map.h"

#include "net/base/hash_value.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace chromeos {
namespace certificate_provider {
namespace {

std::string GetSubjectPublicKeyInfo(const net::X509Certificate& certificate) {
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer()),
          &spki_bytes)) {
    return {};
  }
  return spki_bytes.as_string();
}

void BuildFingerprintsMap(
    const std::map<std::string, certificate_provider::CertificateInfoList>&
        extension_to_certificates,
    ThreadSafeCertificateMap::FingerprintToCertAndExtensionMap*
        fingerprint_to_cert) {
  for (const auto& entry : extension_to_certificates) {
    const std::string& extension_id = entry.first;
    for (const CertificateInfo& cert_info : entry.second) {
      const net::SHA256HashValue fingerprint =
          net::X509Certificate::CalculateFingerprint256(
              cert_info.certificate->cert_buffer());
      fingerprint_to_cert->insert(std::make_pair(
          fingerprint,
          std::make_unique<ThreadSafeCertificateMap::CertAndExtension>(
              cert_info, extension_id)));
    }
  }
}

void BuildSpkiMap(
    const std::map<std::string, certificate_provider::CertificateInfoList>&
        extension_to_certificates,
    ThreadSafeCertificateMap::SpkiToCertAndExtensionMap* spki_to_cert) {
  for (const auto& entry : extension_to_certificates) {
    const std::string& extension_id = entry.first;
    for (const CertificateInfo& cert_info : entry.second) {
      const std::string spki = GetSubjectPublicKeyInfo(*cert_info.certificate);
      // If the same public key appears in the |extension_to_certificates| input
      // multiple times, it is unspecified which (cert_info, extension_id) will
      // end up in the output map.
      spki_to_cert->insert(std::make_pair(
          spki, std::make_unique<ThreadSafeCertificateMap::CertAndExtension>(
                    cert_info, extension_id)));
    }
  }
}

}  // namespace

ThreadSafeCertificateMap::CertAndExtension::CertAndExtension(
    const CertificateInfo& cert_info,
    const std::string& extension_id)
    : cert_info(cert_info), extension_id(extension_id) {}

ThreadSafeCertificateMap::CertAndExtension::~CertAndExtension() {}

ThreadSafeCertificateMap::ThreadSafeCertificateMap() {}

ThreadSafeCertificateMap::~ThreadSafeCertificateMap() {}

void ThreadSafeCertificateMap::Update(
    const std::map<std::string, certificate_provider::CertificateInfoList>&
        extension_to_certificates) {
  FingerprintToCertAndExtensionMap new_fingerprint_map;
  SpkiToCertAndExtensionMap new_spki_map;
  BuildFingerprintsMap(extension_to_certificates, &new_fingerprint_map);
  BuildSpkiMap(extension_to_certificates, &new_spki_map);

  base::AutoLock auto_lock(lock_);
  // Keep all old keys from the old maps (|fingerprint_to_cert_and_extension_|
  // and |spki_to_cert_and_extension_|), but remove the association to any
  // extension.
  for (const auto& entry : fingerprint_to_cert_and_extension_) {
    const net::SHA256HashValue& fingerprint = entry.first;
    // This doesn't modify the map if it already contains the key |fingerprint|.
    new_fingerprint_map.insert(std::make_pair(fingerprint, nullptr));
  }
  fingerprint_to_cert_and_extension_.swap(new_fingerprint_map);

  for (const auto& entry : spki_to_cert_and_extension_) {
    const std::string& spki = entry.first;
    // This doesn't modify the map if it already contains the key |spki|.
    new_spki_map.insert(std::make_pair(spki, nullptr));
  }
  spki_to_cert_and_extension_.swap(new_spki_map);
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
  const auto it = fingerprint_to_cert_and_extension_.find(fingerprint);
  if (it == fingerprint_to_cert_and_extension_.end())
    return false;

  CertAndExtension* const value = it->second.get();
  if (value) {
    *is_currently_provided = true;
    *info = value->cert_info;
    *extension_id = value->extension_id;
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
  const auto it = spki_to_cert_and_extension_.find(subject_public_key_info);
  if (it == spki_to_cert_and_extension_.end())
    return false;

  CertAndExtension* const value = it->second.get();
  if (value) {
    *is_currently_provided = true;
    *info = value->cert_info;
    *extension_id = value->extension_id;
  }
  return true;
}

void ThreadSafeCertificateMap::RemoveExtension(
    const std::string& extension_id) {
  base::AutoLock auto_lock(lock_);
  for (auto& entry : fingerprint_to_cert_and_extension_) {
    CertAndExtension* const value = entry.second.get();
    // Only remove the association of the fingerprint to the extension, but keep
    // the fingerprint.
    if (value && value->extension_id == extension_id)
      entry.second.reset();
  }

  for (auto& entry : spki_to_cert_and_extension_) {
    CertAndExtension* const value = entry.second.get();
    // Only remove the association of the SPKI to the extension, but keep the
    // SPKI.
    if (value && value->extension_id == extension_id)
      entry.second.reset();
  }
}

}  // namespace certificate_provider
}  // namespace chromeos
