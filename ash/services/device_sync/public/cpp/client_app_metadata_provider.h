// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CLIENT_APP_METADATA_PROVIDER_H_
#define ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CLIENT_APP_METADATA_PROVIDER_H_

#include "base/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cryptauthv2 {
class ClientAppMetadata;
}  // namespace cryptauthv2

namespace chromeos {

namespace device_sync {

// Provides the cryptauthv2::ClientAppMetadata object associated with the
// current device. cryptauthv2::ClientAppMetadata describes properties of this
// Chromebook and is not expected to change except when the OS version is
// updated.
class ClientAppMetadataProvider {
 public:
  ClientAppMetadataProvider() = default;

  ClientAppMetadataProvider(const ClientAppMetadataProvider&) = delete;
  ClientAppMetadataProvider& operator=(const ClientAppMetadataProvider&) =
      delete;

  virtual ~ClientAppMetadataProvider() = default;

  using GetMetadataCallback = base::OnceCallback<void(
      const absl::optional<cryptauthv2::ClientAppMetadata>&)>;

  // Fetches the ClientAppMetadata for the current device; if the operation
  // fails, null is passed to the callback.
  virtual void GetClientAppMetadata(const std::string& gcm_registration_id,
                                    GetMetadataCallback callback) = 0;
};

}  // namespace device_sync

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
namespace device_sync {
using ::chromeos::device_sync::ClientAppMetadataProvider;
}
}  // namespace ash

#endif  // ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CLIENT_APP_METADATA_PROVIDER_H_
