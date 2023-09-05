// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FEDERATED_FEDERATED_CLIENT_MANAGER_H_
#define ASH_SYSTEM_FEDERATED_FEDERATED_CLIENT_MANAGER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/federated/federated_service_controller.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "chromeos/ash/services/federated/public/mojom/example.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::federated {

using chromeos::federated::mojom::ExamplePtr;

// Utility class for clients of the Federated Service in ash-chrome.
//
// TODO(b/289140140): UMA metrics.
// TODO(b/289140140): Expand documentation.
class ASH_EXPORT FederatedClientManager {
 public:
  FederatedClientManager();
  FederatedClientManager(const FederatedClientManager&) = delete;
  FederatedClientManager& operator=(const FederatedClientManager&) = delete;
  ~FederatedClientManager();

  // Normally, this class interacts with the ash shell. This may sometimes
  // be undesired in client tests e.g. in unit tests which do not operate
  // in a mocked ash environment.
  //
  // Call this method during test set-up (and before creation of an instance of
  // this class) to prevent this class from interacting with the ash shell.
  // Faked (and successful) ash interactions will occur instead.
  static void UseFakeAshInteractionForTest();

  // TODO expand and tidy documentation
  // Methods for generic clients:
  // Whether the Federated Service is available. TODO update
  // optional to call before ReportExample
  bool IsFederatedServiceAvailable() const;
  void ReportExample(const std::string& client_name, ExamplePtr example);

  // Methods for Federated Strings Service clients:
  // Whether the Federated Strings Service is available. TODO update
  // optional to call
  // TODO(b/289140140): Link to documentation when available.
  bool IsFederatedStringsServiceAvailable() const;
  void ReportStringViaStringsService(const std::string& client_name,
                                     const std::string& client_string);

 private:
  void TryToBindFederatedServiceIfNecessary();
  void ReportExampleToFederatedService(const std::string& client_name,
                                       ExamplePtr example);

  static inline bool use_fake_controller_for_testing_ = false;
  mojo::Remote<chromeos::federated::mojom::FederatedService> federated_service_;
  raw_ptr<FederatedServiceController> controller_ = nullptr;
};

}  // namespace ash::federated

#endif  // ASH_SYSTEM_FEDERATED_FEDERATED_CLIENT_MANAGER_H_
