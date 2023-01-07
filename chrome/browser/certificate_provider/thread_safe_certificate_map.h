// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_PROVIDER_THREAD_SAFE_CERTIFICATE_MAP_H_
#define CHROME_BROWSER_CERTIFICATE_PROVIDER_THREAD_SAFE_CERTIFICATE_MAP_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "chromeos/components/certificate_provider/certificate_info.h"

namespace net {
class X509Certificate;
struct SHA256HashValue;
}  // namespace net

namespace chromeos {
namespace certificate_provider {

class ThreadSafeCertificateMap {
 public:
  ThreadSafeCertificateMap();
  ThreadSafeCertificateMap(const ThreadSafeCertificateMap&) = delete;
  ThreadSafeCertificateMap& operator=(const ThreadSafeCertificateMap&) = delete;
  ~ThreadSafeCertificateMap();

  // Updates the certificates provided by extension |extension_id| to be
  // |certificates|.
  void UpdateCertificatesForExtension(const std::string& extension_id,
                                      const CertificateInfoList& certificates);

  // Returns all currently provided certificates.
  std::vector<scoped_refptr<net::X509Certificate>> GetCertificates();

  // Looks up the given certificate. If the certificate was added by any
  // previous Update() call, returns true.
  // If this certificate was provided in the most recent Update() call,
  // |is_currently_provided| will be set to true, |extension_id| be set to that
  // extension's id and |info| will be set to the stored info. Otherwise, if
  // this certificate was not provided in the the most recent Update() call,
  // sets |is_currently_provided| to false and doesn't modify |info| and
  // |extension_id|.
  bool LookUpCertificate(const net::X509Certificate& cert,
                         bool* is_currently_provided,
                         CertificateInfo* info,
                         std::string* extension_id);

  // Looks up for certificate and extension_id based on
  // |subject_public_key_info|, which is a DER-encoded X.509 Subject Public Key
  // Info. If the certificate was added by previous Update() call, returns true.
  // If this certificate was provided in the most recent Update() call,
  // |is_currently_provided| will be set to true and |info| and |extension_id|
  // will be populated according to the data that have been mapped to this
  // |subject_public_key_info|. Otherwise, if this certificate was not provided
  // in the most recent Update() call, sets |is_currently_provided| to false and
  // doesn't modify |info| and |extension_id|. If multiple entries are found, it
  // is unspecified which one will be returned.
  bool LookUpCertificateBySpki(const std::string& subject_public_key_info,
                               bool* is_currently_provided,
                               CertificateInfo* info,
                               std::string* extension_id);

  // Remove every association of stored certificates to the given extension.
  // The certificates themselves will be remembered.
  void RemoveExtension(const std::string& extension_id);

 private:
  void RemoveCertificatesProvidedByExtension(const std::string& extension_id);

  base::Lock lock_;
  using ExtensionToCertificateMap =
      base::flat_map<std::string, CertificateInfo>;
  // A map that has the certificates' fingerprints as keys.
  base::flat_map<net::SHA256HashValue, ExtensionToCertificateMap>
      fingerprint_to_extension_and_cert_;
  // A map that has a DER-encoded X.509 Subject Public Key Info as keys.
  base::flat_map<std::string, ExtensionToCertificateMap>
      spki_to_extension_and_cert_;
};

}  // namespace certificate_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CERTIFICATE_PROVIDER_THREAD_SAFE_CERTIFICATE_MAP_H_
