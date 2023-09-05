// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/federated/federated_client_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/federated/federated_service_controller_impl.h"
#include "ash/system/federated/test_federated_service_controller.h"
#include "base/notreached.h"
#include "chromeos/ash/services/federated/public/cpp/federated_example_util.h"

// TODO tidy up method naming conventions e.g. IsServiceAvailable is
// confusingly used by this class for itself, and by fed service for itself.
namespace ash::federated {

namespace {

using chromeos::federated::mojom::Example;
using chromeos::federated::mojom::Features;

// TODO use a word other than "query".
ExamplePtr CreateStringsServiceExamplePtr(const std::string& query) {
  ExamplePtr example = Example::New();
  example->features = Features::New();
  auto& feature_map = example->features->feature;
  feature_map["query"] = CreateStringList({query});
  return example;
}

}  // namespace

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

bool FederatedClientManager::IsFederatedServiceAvailable() const {
  // TODO(b/289140140): Check further flags or conditions as needed.
  return ash::features::IsFederatedServiceEnabled() && controller_ &&
         controller_->IsServiceAvailable();
}

void FederatedClientManager::ReportExample(
    const std::string& client_name,
    chromeos::federated::mojom::ExamplePtr example) {
  if (!ash::features::IsFederatedStringsServiceEnabled()) {
    return;
  }

  ReportExampleToFederatedService(client_name, std::move(example));
}

bool FederatedClientManager::IsFederatedStringsServiceAvailable() const {
  return IsFederatedServiceAvailable() &&
         ash::features::IsFederatedStringsServiceEnabled();
}

void FederatedClientManager::ReportStringViaStringsService(
    const std::string& client_name,
    const std::string& client_string) {
  if (!ash::features::IsFederatedStringsServiceEnabled()) {
    return;
  }

  // std::move for example??
  ReportExampleToFederatedService(
      client_name, CreateStringsServiceExamplePtr(client_string));
}

void FederatedClientManager::TryToBindFederatedServiceIfNecessary() {
  if (federated_service_.is_bound()) {
    return;
  }

  if (IsFederatedServiceAvailable()) {
    ash::federated::ServiceConnection::GetInstance()->BindReceiver(
        federated_service_.BindNewPipeAndPassReceiver());
  }
}

void FederatedClientManager::ReportExampleToFederatedService(
    const std::string& client_name,
    ExamplePtr example) {
  TryToBindFederatedServiceIfNecessary();

  if (!controller_->IsServiceAvailable()) {
    // TODO(b/289140140): UMA metrics.
  } else if (!federated_service_.is_connected()) {
    // TODO(b/289140140): UMA metrics.
  } else {
    // Federated service available and connected.
    federated_service_->ReportExample(client_name, std::move(example));
    // TODO(b/289140140): UMA metrics.
  }
}

}  // namespace ash::federated
