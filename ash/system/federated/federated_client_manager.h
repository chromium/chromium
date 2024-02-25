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
#include "chromeos/ash/services/federated/public/mojom/tables.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::federated {

using chromeos::federated::mojom::ExamplePtr;

// Utility class for clients of the Federated Service in ash-chrome.
//
// Usage:
//
// Clients should own an instance of FederatedClientManager.
// TODO(b/289140140): Link to an example.
//
// To send an example to Federated Service storage:
// 1) Check service availability via the appropriate Is...ServiceAvailable()
// method.
// 2) Report the example via the appropriate Report...() method.
//
// TODO(b/289140140): UMA metrics.
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
  // Call this method during test set-up to prevent this class from interacting
  // with the ash shell. Faked (and successful) ash interactions will occur
  // instead.
  static void UseFakeAshInteractionForTest();

  // ***** Methods for generic clients *****

  // Whether the Federated Service is available.
  // It is recommended (but not necessary) for clients to call this before each
  // attempt to call |ReportExample()|.
  bool IsFederatedServiceAvailable();
  // Reports an example to Federated Service storage.
  // If the Federated Service is not available, this method is effectively a
  // no-op.
  void ReportExample(
      chromeos::federated::mojom::FederatedExampleTableId table_id,
      ExamplePtr example);
  // Same as ReportExample above, except FederatedClientManager will use the
  // inputs to construct an ExamplePtr (see .cc for exact schema). This is
  // expected to be useful mainly for PHH cases.
  void ReportSingleString(
      chromeos::federated::mojom::FederatedExampleTableId table_id,
      const std::string& example_feature_name,
      const std::string& example_str);

  // ***** Methods for Federated Strings Service clients *****

  // Whether the Federated Strings Service is available.
  // It is recommended (but not necessary) for clients to call this before each
  // attempt to call |ReportStringViaStringsService()|.
  bool IsFederatedStringsServiceAvailable();
  // Reports a Strings Service example to Federated Service storage.
  // If the Federated Service is not available, this method is effectively a
  // no-op.
  void ReportStringViaStringsService(
      chromeos::federated::mojom::FederatedExampleTableId table_id,
      const std::string& client_string);

  // Returns a count of examples which were sent to the Federated Service.
  // "Success" here means "successfully processed by FederatedClientManager, and
  // forwarded to CrOS Federated Service".
  //
  // It has no knowledge of how results are received CrOS-side (whether in prod,
  // or mocked for test).
  int get_num_successful_reports_for_test() const {
    return successful_reports_for_test_;
  }

 private:
  void TryToBindFederatedServiceIfNecessary();
  void ReportExampleToFederatedService(
      const chromeos::federated::mojom::FederatedExampleTableId table_id,
      ExamplePtr example);

  int successful_reports_for_test_ = 0;
  bool initialized_ = false;
  static inline bool use_fake_controller_for_testing_ = false;
  mojo::Remote<chromeos::federated::mojom::FederatedService> federated_service_;
  // Reason for `DisableDanglingPtrDetection`:
  // In prod: controller_ not owned by this class.
  // In non-ash unit tests: controller_ can be owned by this class.
  // TODO(b/299186135): Consider refactoring test management of ash and non-ash
  // environments, and the impact on management of ownership of this controller.
  raw_ptr<FederatedServiceController, DisableDanglingPtrDetection> controller_ =
      nullptr;
};

}  // namespace ash::federated

#endif  // ASH_SYSTEM_FEDERATED_FEDERATED_CLIENT_MANAGER_H_
