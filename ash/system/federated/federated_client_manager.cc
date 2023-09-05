// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/federated/federated_client_manager.h"

#include "base/notreached.h"

namespace ash::federated {

FederatedClientManager::FederatedClientManager() {
  // TODO(b/289140140): Implement.
}

FederatedClientManager::~FederatedClientManager() = default;

bool FederatedClientManager::IsServiceAvailable() const {
  // TODO(b/289140140): Implement.
  NOTIMPLEMENTED();
  return false;
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
