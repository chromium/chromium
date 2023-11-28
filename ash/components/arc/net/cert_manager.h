// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_CERT_MANAGER_H_
#define ASH_COMPONENTS_ARC_NET_CERT_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"

namespace arc {

// CertManager imports plain-text certificates and private keys into Chrome OS'
// key store (chaps).
class CertManager {
 public:
  using ImportPrivateKeyAndCertCallback =
      base::OnceCallback<void(const std::optional<std::string>& cert_id,
                              const std::optional<int>& slot_id)>;

  virtual ~CertManager() = default;

  // Asynchronously import a PEM-formatted private key and user certificate into
  // the NSS certificate database. Once done, |callback| will be called with its
  // ID and the slot ID of the database. This method will asynchronously fetch
  // the database. Calling this method will remove any previously imported
  // private keys and certificates with the same ID.
  // For Passpoint, the expected removal flow of private keys and certificates
  // are done in shill directly using PKCS#11 API. This means that any state of
  // NSS for the private keys and certificates are not cleaned. This resulted in
  // any subsequent provisionings of a deleted certificate to fail. In order to
  // not have the side effect, the removal is necessary.
  virtual void ImportPrivateKeyAndCert(
      const std::string& key_pem,
      const std::string& cert_pem,
      ImportPrivateKeyAndCertCallback callback) = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_NET_CERT_MANAGER_H_
