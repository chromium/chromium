// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FEDERATED_TEST_FEDERATED_SERVICE_CONTROLLER_H_
#define ASH_SYSTEM_FEDERATED_TEST_FEDERATED_SERVICE_CONTROLLER_H_

#include "ash/system/federated/federated_service_controller.h"

#include "ash/ash_export.h"

namespace ash::federated {

// Test version of FederatedServiceController.
// Mocks out IsServiceAvailable() to always return true.
class ASH_EXPORT TestFederatedServiceController
    : public ash::federated::FederatedServiceController {
 public:
  TestFederatedServiceController() = default;
  TestFederatedServiceController(const TestFederatedServiceController&) =
      delete;
  TestFederatedServiceController& operator=(
      const TestFederatedServiceController&) = delete;
  ~TestFederatedServiceController() override = default;

  // ash::federated::FederatedServiceController:
  bool IsServiceAvailable() const override;
};

}  // namespace ash::federated

#endif  // ASH_SYSTEM_FEDERATED_TEST_FEDERATED_SERVICE_CONTROLLER_H_
