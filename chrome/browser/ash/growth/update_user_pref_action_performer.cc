// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/update_user_pref_action_performer.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kName[] = "name";
constexpr char kType[] = "type";
constexpr char kValue[] = "value";

enum class UpdateType {
  kSet = 0,
  kClear,
  kAppend,
  kRemove,
  kMaxUpdateType = kRemove,
};

// Return nullptr if the pref service is not available or the pref is invalid or
// the pref type is invalid.
// Use type NONE as target type to skip type checking.
PrefService* GetPrefServiceAndCheckUserPrefType(
    const std::string& pref_name,
    const base::Value::Type& target_type) {
  PrefService* pref_service =
      ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (!pref_service) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kUserPrefServiceNotAvailable);
    CAMPAIGNS_LOG(ERROR) << "User pref service not available.";
    return nullptr;
  }

  const PrefService::Preference* pref = pref_service->FindPreference(pref_name);
  if (!pref) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kUserPrefNotFound);
    CAMPAIGNS_LOG(ERROR) << "User pref action: " << pref_name << " not found";
    return nullptr;
  }

  // Skip type checking if the target_type is type::NONE.
  if (target_type != base::Value::Type::NONE &&
      pref->GetType() != target_type) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kUserPrefValueTypeMismatch);
    CAMPAIGNS_LOG(ERROR) << "User pref action: " << pref_name
                         << " type mismatched";
    return nullptr;
  }
  return pref_service;
}

bool SetUserPrefValue(const std::string& pref_name, const base::Value* value) {
  PrefService* pref_service =
      GetPrefServiceAndCheckUserPrefType(pref_name, value->type());
  if (!pref_service) {
    return false;
  }

  pref_service->Set(pref_name, *value);
  return true;
}

bool ClearUserPrefValue(const std::string& pref_name) {
  PrefService* pref_service =
      GetPrefServiceAndCheckUserPrefType(pref_name, base::Value::Type::NONE);
  if (!pref_service) {
    return false;
  }
  pref_service->ClearPref(pref_name);
  return true;
}

bool AppendValueToUserPref(const std::string& pref_name,
                           const base::Value* value) {
  PrefService* pref_service =
      GetPrefServiceAndCheckUserPrefType(pref_name, base::Value::Type::LIST);
  if (!pref_service) {
    return false;
  }

  if (base::Contains(pref_service->GetList(pref_name), *value)) {
    CAMPAIGNS_LOG(ERROR) << "Pref value is already in the list.";
    return false;
  }

  auto& values = pref_service->GetList(pref_name);
  auto cached_values = values.Clone();
  cached_values.Append(value->Clone());
  pref_service->SetList(pref_name, std::move(cached_values));
  return true;
}

bool RemoveValueFromUserPref(const std::string& pref_name,
                             const base::Value* value) {
  PrefService* pref_service =
      GetPrefServiceAndCheckUserPrefType(pref_name, base::Value::Type::LIST);
  if (!pref_service) {
    return false;
  }

  if (!base::Contains(pref_service->GetList(pref_name), *value)) {
    CAMPAIGNS_LOG(ERROR)
        << "Unable to remove: Pref value not in user preference.";
    return false;
  }

  auto& values = pref_service->GetList(pref_name);
  auto cached_values = values.Clone();
  cached_values.EraseValue(*value);
  pref_service->SetList(pref_name, std::move(cached_values));
  return true;
}

bool UpdateUserPrefValue(const std::string& pref_name,
                         UpdateType type,
                         const base::Value* value) {
  switch (type) {
    case UpdateType::kSet:
      return SetUserPrefValue(pref_name, value);
    case UpdateType::kClear:
      return ClearUserPrefValue(pref_name);
    case UpdateType::kAppend:
      return AppendValueToUserPref(pref_name, value);
    case UpdateType::kRemove:
      return RemoveValueFromUserPref(pref_name, value);
  }
}

}  // namespace

UpdateUserPrefActionPerformer::UpdateUserPrefActionPerformer() = default;
UpdateUserPrefActionPerformer::~UpdateUserPrefActionPerformer() = default;

void UpdateUserPrefActionPerformer::Run(
    int campaign_id,
    std::optional<int> group_id,
    const base::Value::Dict* params,
    growth::ActionPerformer::Callback callback) {
  if (!params) {
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    CAMPAIGNS_LOG(ERROR) << "Update User Pref params not found.";
    return;
  }

  auto* pref_name = params->FindString(kName);
  if (!pref_name) {
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    CAMPAIGNS_LOG(ERROR) << kName << " parameter not found.";
    return;
  }

  auto type = params->FindInt(kType);
  if (!type) {
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    CAMPAIGNS_LOG(ERROR) << kType << " parameter not found.";
    return;
  }

  if (type.value() < 0 ||
      type.value() > static_cast<int>(UpdateType::kMaxUpdateType)) {
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    CAMPAIGNS_LOG(ERROR) << kType << " invalid value: " << type.value();
    return;
  }

  auto update_type = static_cast<UpdateType>(type.value());
  auto* value = params->Find(kValue);

  // Value is required for all the actions except clear action.
  if (update_type != UpdateType::kClear && !value) {
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    CAMPAIGNS_LOG(ERROR) << kValue << " parameter not found.";
    return;
  }

  if (!UpdateUserPrefValue(*pref_name, update_type, value)) {
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kUpdateUserPrefFailed);
    return;
  }
  std::move(callback).Run(growth::ActionResult::kSuccess,
                          /*action_result_reason=*/std::nullopt);
}

growth::ActionType UpdateUserPrefActionPerformer::ActionType() const {
  return growth::ActionType::kUpdateUserPref;
}
