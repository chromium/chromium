// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_ATTESTATION_CERTIFICATES_SYNCER_H_
#define ASH_SERVICES_DEVICE_SYNC_ATTESTATION_CERTIFICATES_SYNCER_H_

#include <string>
#include <vector>

#include "base/callback.h"

namespace ash {
namespace device_sync {

// Uploads the attestation certs to cryptauth.
class AttestationCertificatesSyncer {
 public:
  using NotifyCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;
  using GetAttestationCertificatesFunction =
      base::RepeatingCallback<void(const NotifyCallback, const std::string&)>;

  virtual ~AttestationCertificatesSyncer() = default;
  AttestationCertificatesSyncer(const AttestationCertificatesSyncer&) = delete;
  AttestationCertificatesSyncer& operator=(
      const AttestationCertificatesSyncer&) = delete;

  virtual bool IsUpdateRequired() = 0;
  virtual void SetLastSyncTimestamp() = 0;
  virtual void UpdateCerts(NotifyCallback callback,
                           const std::string& user_key) = 0;
  virtual void ScheduleSyncForTest() = 0;

 protected:
  AttestationCertificatesSyncer() = default;
};

}  // namespace device_sync
}  // namespace ash

#endif  // ASH_SERVICES_DEVICE_SYNC_ATTESTATION_CERTIFICATES_SYNCER_H_
