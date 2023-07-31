// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_NEARBY_PRESENCE_CREDENTIAL_STORAGE_H_
#define CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_NEARBY_PRESENCE_CREDENTIAL_STORAGE_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"

namespace ash::nearby::presence {

// Implementation of the Mojo NearbyPresenceCredentialStorage interface. It
// handles requests to read/write to the credential storage database for Nearby
// Presence.
class NearbyPresenceCredentialStorage
    : public mojom::NearbyPresenceCredentialStorage {
 public:
  NearbyPresenceCredentialStorage();

  NearbyPresenceCredentialStorage(const NearbyPresenceCredentialStorage&) =
      delete;
  NearbyPresenceCredentialStorage& operator=(
      const NearbyPresenceCredentialStorage&) = delete;

  ~NearbyPresenceCredentialStorage() override;
};

}  // namespace ash::nearby::presence

#endif  // CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_NEARBY_PRESENCE_CREDENTIAL_STORAGE_H_
