// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FEDERATED_FEDERATED_SERVICE_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_FEDERATED_FEDERATED_SERVICE_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/federated/federated_service_controller.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/federated/public/mojom/federated_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::federated {

// FederatedServiceControllerImpl listens to LoginStatus and invokes federated
// service when user signs in therefore cryptohome is mounted. After that it
// keeps listening to the LoginStatus and changes the availability accordingly.
class ASH_EXPORT FederatedServiceControllerImpl
    : public FederatedServiceController,
      public ash::SessionObserver {
 public:
  FederatedServiceControllerImpl();
  FederatedServiceControllerImpl(const FederatedServiceControllerImpl&) =
      delete;
  FederatedServiceControllerImpl& operator=(
      const FederatedServiceControllerImpl&) = delete;
  ~FederatedServiceControllerImpl() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus login_status) override;

  // FederatedServiceController:
  // If false, federated customers reporting examples is a no-op. Federated
  // service will abandon the example because of no valid cryptohome hence no
  // example database.
  // To avoid data loss and meaningless calls, customers should always check
  // `IsServiceAvailable()` before reporting examples.
  bool IsServiceAvailable() const override;

 private:
  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  // A clone of primordial FederatedService interface.
  mojo::Remote<chromeos::federated::mojom::FederatedService> federated_service_;
  bool service_available_ = false;

  bool reported_ = false;

  base::WeakPtrFactory<FederatedServiceControllerImpl> weak_ptr_factory_{this};
};
}  // namespace ash::federated

#endif  // ASH_SYSTEM_FEDERATED_FEDERATED_SERVICE_CONTROLLER_IMPL_H_
