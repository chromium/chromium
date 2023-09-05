// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/federated/federated_client_manager.h"

#include "ash/shell.h"
#include "ash/system/federated/federated_service_controller_impl.h"
#include "ash/system/federated/test_federated_service_controller.h"
#include "base/notreached.h"

namespace ash::federated {

FederatedClientManager::FederatedClientManager() {
  if (use_fake_controller_for_testing_) {
    controller_ = new TestFederatedServiceController;
  } else {
    controller_ = Shell::Get()->federated_service_controller();
  }
}

FederatedClientManager::~FederatedClientManager() = default;

void FederatedClientManager::UseFakeAshInteractionForTest() {
  use_fake_controller_for_testing_ = true;
}

bool FederatedClientManager::IsServiceAvailable() const {
  // TODO(b/289140140): Complete implementation e.g. check appropriate feature
  // flags.
  return controller_ && controller_->IsServiceAvailable();
}

void FederatedClientManager::ReportExample(
    const std::string& client_name,
    chromeos::federated::mojom::ExamplePtr example) const {
  // TODO(b/289140140): Implement.
  NOTIMPLEMENTED();
}

void FederatedClientManager::ReportStringViaStringsService(
    const std::string& client_name,
    const std::string& client_string) const {
  // TODO(b/289140140): Implement.
  NOTIMPLEMENTED();
}

void FederatedClientManager::TryToBindFederatedServiceIfNecessary() {
  // TODO(b/289140140): Implement.
  NOTIMPLEMENTED();
}
void FederatedClientManager::ReportExampleToFederatedService(
    const std::string& client_name,
    ExamplePtr example) {
  // TODO(b/289140140): Implement.
  NOTIMPLEMENTED();
}

}  // namespace ash::federated
