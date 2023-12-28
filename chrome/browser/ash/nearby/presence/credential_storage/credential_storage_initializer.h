// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_CREDENTIAL_STORAGE_INITIALIZER_H_
#define CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_CREDENTIAL_STORAGE_INITIALIZER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/nearby/presence/credential_storage/nearby_presence_credential_storage.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::nearby::presence {

// `CredentialStorageInitializer` owns and initializes
// `NearbyPresenceCredentialStorage`.
class CredentialStorageInitializer {
 public:
  CredentialStorageInitializer(
      mojo::PendingReceiver<mojom::NearbyPresenceCredentialStorage>
          pending_receiver,
      Profile* profile);

  CredentialStorageInitializer(const CredentialStorageInitializer&) = delete;
  CredentialStorageInitializer& operator=(CredentialStorageInitializer&) =
      delete;

  ~CredentialStorageInitializer();

  // Initializes the underlying `NearbyPresenceCredentialStorage`.
  void Initialize();

 protected:
  // Test only constructor used to inject a NearbyPresenceCredentialStorageBase.
  explicit CredentialStorageInitializer(
      std::unique_ptr<NearbyPresenceCredentialStorageBase>
          nearby_presence_credential_storage);

 private:
  void OnInitialized(bool initialization_success);

  bool is_initialized_ = false;

  std::unique_ptr<NearbyPresenceCredentialStorageBase>
      nearby_presence_credential_storage_;

  base::WeakPtrFactory<CredentialStorageInitializer> weak_ptr_factory_{this};
};

}  // namespace ash::nearby::presence

#endif  // CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_CREDENTIAL_STORAGE_INITIALIZER_H_
