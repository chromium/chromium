// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/preconfigured_desk_templates_external_data_handler.h"

#include <utility>

#include "chrome/browser/ash/settings/cros_settings.h"
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
  // TODO(brianbeck): Inform DeskClient of change
}

void PreconfiguredDeskTemplatesExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  // TODO(brianbeck): Inform DeskClient of change
}

void PreconfiguredDeskTemplatesExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  // TODO(brianbeck): Inform DeskClient of change
}

}  // namespace policy
