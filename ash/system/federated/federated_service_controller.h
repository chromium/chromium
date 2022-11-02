// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FEDERATED_FEDERATED_SERVICE_CONTROLLER_H_
#define ASH_SYSTEM_FEDERATED_FEDERATED_SERVICE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/federated/public/mojom/federated_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::federated {

// FederatedServiceController listens to LoginStatus and invokes federated
// service when user signs in therefore cryptohome is mounted. After that it
// keeps listening to the LoginStatus and changes the availability accordingly.
class ASH_EXPORT FederatedServiceController : public SessionObserver {
 public:
  FederatedServiceController();
  FederatedServiceController(const FederatedServiceController&) = delete;
  FederatedServiceController& operator=(const FederatedServiceController&) =
      delete;
  ~FederatedServiceController() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus login_status) override;

  // If false, federated customers reporting examples is a no-op. Federated
  // service will abandon the example because of no valid cryptohome hence no
  // example database.
  // To avoid data loss and meaningless calls, customers should always check
  // `service_available()` before reporting examples.
  bool service_available() const { return service_available_; }

 private:
  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  // A clone of primordial FederatedService interface.
  mojo::Remote<chromeos::federated::mojom::FederatedService> federated_service_;
  bool service_available_ = false;

  bool reported_ = false;

  base::WeakPtrFactory<FederatedServiceController> weak_ptr_factory_{this};
};
}  // namespace ash::federated

#endif  // ASH_SYSTEM_FEDERATED_FEDERATED_SERVICE_CONTROLLER_H_
