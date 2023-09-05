// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FEDERATED_FEDERATED_CLIENT_MANAGER_H_
#define ASH_SYSTEM_FEDERATED_FEDERATED_CLIENT_MANAGER_H_

#include <string>

#include "ash/ash_export.h"
#include "chromeos/ash/services/federated/public/mojom/example.mojom.h"

namespace ash::federated {

using chromeos::federated::mojom::ExamplePtr;

// Utility class for clients of the Federated Service in ash-chrome.
//
// TODO(b/289140140): Implement.
// TODO(b/289140140): Expand documentation.
class ASH_EXPORT FederatedClientManager {
 public:
  FederatedClientManager();
  FederatedClientManager(const FederatedClientManager&) = delete;
  FederatedClientManager& operator=(const FederatedClientManager&) = delete;
  ~FederatedClientManager();

  // Whether the Federated Service is available.
  bool IsServiceAvailable() const;

  // For clients who manually manage their own federated tasks i.e. not for
  // clients of the Federated Strings Service.
  void ReportExample(const std::string& client_name, ExamplePtr example) const;

  // For clients of the Federated Strings Service.
  // TODO(b/289140140): Link to documentation when available.
  void ReportStringViaStringsService(const std::string& client_name,
                                     const std::string& client_string) const;

 private:
  void TryToBindFederatedServiceIfNecessary();
  void ReportExampleToFederatedService(const std::string& client_name,
                                       ExamplePtr example);
};

}  // namespace ash::federated

#endif  // ASH_SYSTEM_FEDERATED_FEDERATED_CLIENT_MANAGER_H_
