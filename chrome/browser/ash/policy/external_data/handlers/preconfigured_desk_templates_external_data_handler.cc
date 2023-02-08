// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/preconfigured_desk_templates_external_data_handler.h"

#include <utility>

#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "components/policy/policy_constants.h"

namespace policy {

PreconfiguredDeskTemplatesExternalDataHandler::
    PreconfiguredDeskTemplatesExternalDataHandler(
        ash::CrosSettings* cros_settings,
        DeviceLocalAccountPolicyService* policy_service)
    : preconfigured_desk_templates_observer_(cros_settings,
                                             policy_service,
                                             key::kPreconfiguredDeskTemplates,
                                             this) {
  preconfigured_desk_templates_observer_.Init();
}

PreconfiguredDeskTemplatesExternalDataHandler::
    ~PreconfiguredDeskTemplatesExternalDataHandler() = default;

void PreconfiguredDeskTemplatesExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  DesksClient* dc = DesksClient::Get();
  if (dc) {
    dc->RemovePolicyPreconfiguredTemplate(
        CloudExternalDataPolicyHandler::GetAccountId(user_id));
  }
}

void PreconfiguredDeskTemplatesExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  DesksClient* dc = DesksClient::Get();
  if (dc) {
    dc->SetPolicyPreconfiguredTemplate(
        CloudExternalDataPolicyHandler::GetAccountId(user_id), std::move(data));
  }
}

void PreconfiguredDeskTemplatesExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id,
    base::OnceClosure on_removed) {
  DesksClient* dc = DesksClient::Get();
  if (dc)
    dc->RemovePolicyPreconfiguredTemplate(account_id);
  std::move(on_removed).Run();
}

}  // namespace policy
