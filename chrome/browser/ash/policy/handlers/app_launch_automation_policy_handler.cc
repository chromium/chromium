// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/app_launch_automation_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/desks_storage/core/admin_template_service.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace {

void UpdateModelWithPolicy(
    std::vector<std::unique_ptr<ash::DeskTemplate>> desk_templates,
    desks_storage::DeskModel* desk_model) {
  if (desk_model == nullptr) {
    return;
  }

  // If templates exist that aren't in the current policy we should delete them.
  std::vector<base::Uuid> desk_uuids_to_delete = desk_model->GetAllEntryUuids();
  std::set<base::Uuid> desk_uuids_to_delete_set(desk_uuids_to_delete.begin(),
                                                desk_uuids_to_delete.end());

  for (auto& desk_template : desk_templates) {
    // Something went wrong when parsing the template.
    if (desk_template == nullptr) {
      continue;
    }

    // Something has gone wrong if the field isn't a dict.
    CHECK(desk_template->policy_definition().type() == base::Value::Type::DICT);

    // Query model to determine if this entry exists already.
    auto get_entry_result = desk_model->GetEntryByUUID(desk_template->uuid());
    auto entry_status = get_entry_result.status;

    // If this template exists in the current policy then don't delete it after
    // updating the locally stored policy. Note: this call is a noop if the
    // template in question is a new template.
    if (entry_status == desks_storage::DeskModel::GetEntryByUuidStatus::kOk ||
        entry_status ==
            desks_storage::DeskModel::GetEntryByUuidStatus::kNotFound) {
      desk_uuids_to_delete_set.erase(desk_template->uuid());

      // There was an error when retrieving the template, do nothing and delete
      // the template.
    } else {
      continue;
    }

    // If the policy template already exists in the model and has been unchanged
    // since the last policy update don't overwrite the data.  This will
    // preserve the user's window information for that template.
    if (entry_status == desks_storage::DeskModel::GetEntryByUuidStatus::kOk &&
        get_entry_result.entry->policy_definition() ==
            desk_template->policy_definition()) {
      continue;
    }

    // If the policy template exists in an updated form or is new then either
    // add it to the model or overwrite the existing definition.
    desk_model->AddOrUpdateEntry(std::move(desk_template), base::DoNothing());
  }

  // Remove all templates that aren't in the policy.  If the policy is empty
  // then this will remove all admin templates from the device.
  for (auto uuid : desk_uuids_to_delete_set) {
    desk_model->DeleteEntry(uuid, base::DoNothing());
  }
}

desks_storage::DeskModel* GetDeskModel() {
  auto* admin_template_service =
      ash::Shell::Get()->saved_desk_delegate()->GetAdminTemplateService();

  if (admin_template_service == nullptr) {
    return nullptr;
  }

  return admin_template_service->GetFullDeskModel();
}

}  // namespace

namespace policy {

AppLaunchAutomationPolicyHandler::AppLaunchAutomationPolicyHandler(
    const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kAppLaunchAutomation,
          chrome_schema.GetKnownProperty(key::kAppLaunchAutomation),
          SCHEMA_ALLOW_UNKNOWN) {}

AppLaunchAutomationPolicyHandler::~AppLaunchAutomationPolicyHandler() = default;

bool AppLaunchAutomationPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  // TODO(b/268538092): Validate app launch automation policy value.
  return true;
}

void AppLaunchAutomationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, /*errors=*/nullptr, &policy_value) ||
      !policy_value || !policy_value->is_list()) {
    return;
  }

  UpdateModelWithPolicy(
      desks_storage::desk_template_conversion::
          ParseAdminTemplatesFromPolicyValue(policy_value->Clone()),
      GetDeskModel());

  prefs->SetValue(ash::prefs::kAppLaunchAutomation,
                  base::Value::FromUniquePtrValue(std::move(policy_value)));
}

}  // namespace policy
