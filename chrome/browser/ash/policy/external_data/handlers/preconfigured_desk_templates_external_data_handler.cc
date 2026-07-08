// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/preconfigured_desk_templates_external_data_handler.h"

#include <utility>

#include "base/check_deref.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"

namespace policy {

PreconfiguredDeskTemplatesExternalDataHandler::
    PreconfiguredDeskTemplatesExternalDataHandler(PrefService* local_state)
    : local_state_(CHECK_DEREF(local_state)) {}

PreconfiguredDeskTemplatesExternalDataHandler::
    ~PreconfiguredDeskTemplatesExternalDataHandler() = default;

void PreconfiguredDeskTemplatesExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  DesksClient* dc = DesksClient::Get();
  if (dc) {
    dc->RemovePolicyPreconfiguredTemplate(
        CloudExternalDataPolicyObserver::GetAccountId(local_state_.get(),
                                                      user_id));
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
        CloudExternalDataPolicyObserver::GetAccountId(local_state_.get(),
                                                      user_id),
        std::move(data));
  }
}

void PreconfiguredDeskTemplatesExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  DesksClient* dc = DesksClient::Get();
  if (dc) {
    dc->RemovePolicyPreconfiguredTemplate(account_id);
  }
}

}  // namespace policy
