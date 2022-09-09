// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_PRINT_SERVERS_EXTERNAL_DATA_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_PRINT_SERVERS_EXTERNAL_DATA_HANDLER_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/policy/external_data/handlers/cloud_external_data_policy_handler.h"

namespace ash {
class CrosSettings;
}  // namespace ash

namespace policy {

class DeviceLocalAccountPolicyService;

// This class observes the user setting "ExternalPrintServers" and propagates
// data loaded from this external policy to appropriate objects.
class PrintServersExternalDataHandler : public CloudExternalDataPolicyHandler {
 public:
  PrintServersExternalDataHandler(
      ash::CrosSettings* cros_settings,
      DeviceLocalAccountPolicyService* policy_service);

  PrintServersExternalDataHandler(const PrintServersExternalDataHandler&) =
      delete;
  PrintServersExternalDataHandler& operator=(
      const PrintServersExternalDataHandler&) = delete;

  ~PrintServersExternalDataHandler() override;

  // CloudExternalDataPolicyHandler:
  void OnExternalDataSet(const std::string& policy,
                         const std::string& user_id) override;
  void OnExternalDataCleared(const std::string& policy,
                             const std::string& user_id) override;
  void OnExternalDataFetched(const std::string& policy,
                             const std::string& user_id,
                             std::unique_ptr<std::string> data,
                             const base::FilePath& file_path) override;
  void RemoveForAccountId(const AccountId& account_id) override;

 private:
  CloudExternalDataPolicyObserver print_servers_observer_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_PRINT_SERVERS_EXTERNAL_DATA_HANDLER_H_
