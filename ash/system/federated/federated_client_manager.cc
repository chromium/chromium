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
using chromeos::federated::mojom::ExamplePtr;
using chromeos::federated::mojom::Features;
using chromeos::federated::mojom::FederatedExampleTableId;

ExamplePtr CreateSingleStringExamplePtr(const std::string& example_feature_name,
                                        const std::string& example_str) {
  ExamplePtr example = Example::New();
  example->features = Features::New();
  auto& feature_map = example->features->feature;
  feature_map[example_feature_name] = CreateStringList({example_str});
  return example;
}

}  // namespace

FederatedClientManager::FederatedClientManager() {}

FederatedClientManager::~FederatedClientManager() {
  if (use_fake_controller_for_testing_) {
    delete controller_;
  }
}

void FederatedClientManager::UseFakeAshInteractionForTest() {
  use_fake_controller_for_testing_ = true;
}

bool FederatedClientManager::IsFederatedServiceAvailable() {
  // Lazy initialization. Useful for controlling mocked ash interaction in some
  // unit test environments.
  if (!initialized_) {
    if (use_fake_controller_for_testing_) {
      controller_ = new TestFederatedServiceController;
    } else {
      controller_ = Shell::Get()->federated_service_controller();
    }
    initialized_ = true;
  }

  return ash::features::IsFederatedServiceEnabled() && controller_ &&
         controller_->IsServiceAvailable();
}

void FederatedClientManager::ReportExample(
    const FederatedExampleTableId table_id,
    ExamplePtr example) {
  ReportExampleToFederatedService(table_id, std::move(example));
}

void FederatedClientManager::ReportSingleString(
    const FederatedExampleTableId table_id,
    const std::string& example_feature_name,
    const std::string& example_str) {
  ExamplePtr example =
      CreateSingleStringExamplePtr(example_feature_name, example_str);
  ReportExample(table_id, std::move(example));
}

bool FederatedClientManager::IsFederatedStringsServiceAvailable() {
  return IsFederatedServiceAvailable() &&
         ash::features::IsFederatedStringsServiceEnabled();
}

void FederatedClientManager::ReportStringViaStringsService(
    const FederatedExampleTableId table_id,
    const std::string& client_string) {
  if (!ash::features::IsFederatedStringsServiceEnabled()) {
    return;
  }

  // TODO(b/289140140): Use a less generic word than "query".
  ReportExampleToFederatedService(
      table_id, CreateSingleStringExamplePtr(
                    /*example_feature_name*/ "query", client_string));
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
    const FederatedExampleTableId table_id,
    ExamplePtr example) {
  TryToBindFederatedServiceIfNecessary();

  if (!controller_->IsServiceAvailable()) {
    // TODO(b/289140140): UMA metrics.
  } else if (!federated_service_.is_connected()) {
    // TODO(b/289140140): UMA metrics.
  } else {
    // Federated service available and connected.
    federated_service_->ReportExampleToTable(table_id, std::move(example));
    ++successful_reports_for_test_;
    // TODO(b/289140140): UMA metrics.
  }
}

}  // namespace ash::federated
