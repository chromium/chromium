// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_CERT_MANAGER_H_
#define ASH_COMPONENTS_ARC_NET_CERT_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {

// CertManager imports plain-text certificates and private keys into Chrome OS'
// key store (chaps).
class CertManager {
 public:
  using ImportPrivateKeyAndCertCallback =
      base::OnceCallback<void(const absl::optional<std::string>& cert_id,
                              const absl::optional<int>& slot_id)>;

  virtual ~CertManager() = default;

  // Asynchronously import a PEM-formatted private and user certificate into
  // the NSS certificate database. Calls a callback with its ID and the slot
  // ID of the database. This method will asynchronously fetch the database.
  virtual void ImportPrivateKeyAndCert(
      const std::string& key_pem,
      const std::string& cert_pem,
      ImportPrivateKeyAndCertCallback callback) = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_NET_CERT_MANAGER_H_
