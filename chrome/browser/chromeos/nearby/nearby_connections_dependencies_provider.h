// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NEARBY_NEARBY_CONNECTIONS_DEPENDENCIES_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_NEARBY_NEARBY_CONNECTIONS_DEPENDENCIES_PROVIDER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/nearby/public/mojom/sharing.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace chromeos {
namespace nearby {

class BluetoothAdapterManager;

// Provides dependencies required to initialize Nearby Connections. Implemented
// as a KeyedService because WebRTC dependencies are linked to the user's
// identity.
class NearbyConnectionsDependenciesProvider : public KeyedService {
 public:
  NearbyConnectionsDependenciesProvider(
      Profile* profile,
      signin::IdentityManager* identity_manager);
  ~NearbyConnectionsDependenciesProvider() override;

  // Note: Returns null during session shutdown.
  virtual location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr
  GetDependencies();

  virtual void PrepareForShutdown();

 private:
  friend class NearbyProcessManagerImplTest;

  // KeyedService:
  void Shutdown() override;

  // Test-only constructor.
  NearbyConnectionsDependenciesProvider();

  mojo::PendingRemote<bluetooth::mojom::Adapter>
  GetBluetoothAdapterPendingRemote();

  location::nearby::connections::mojom::WebRtcDependenciesPtr
  GetWebRtcDependencies();

  std::unique_ptr<BluetoothAdapterManager> bluetooth_manager_;

  bool shut_down_ = false;

  Profile* profile_ = nullptr;
  signin::IdentityManager* identity_manager_ = nullptr;

  base::WeakPtrFactory<NearbyConnectionsDependenciesProvider> weak_ptr_factory_{
      this};
};

}  // namespace nearby
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NEARBY_NEARBY_CONNECTIONS_DEPENDENCIES_PROVIDER_H_
