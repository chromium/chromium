// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/update_user_pref_action_performer.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

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

// Returns false if the pref is invalid or the pref type is invalid.
// Use type NONE as target type to skip type checking.
bool CheckUserPrefType(const PrefService& pref_service,
                       const std::string& pref_name,
                       const base::Value::Type& target_type) {
  const PrefService::Preference* pref = pref_service.FindPreference(pref_name);
  if (!pref) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kUserPrefNotFound);
    CAMPAIGNS_LOG(ERROR) << "User pref action: " << pref_name << " not found";
    return false;
  }

  // Skip type checking if the target_type is type::NONE.
  if (target_type != base::Value::Type::NONE &&
      pref->GetType() != target_type) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kUserPrefValueTypeMismatch);
    CAMPAIGNS_LOG(ERROR) << "User pref action: " << pref_name
                         << " type mismatched";
    return false;
  }
  return true;
}

bool SetUserPrefValue(PrefService& pref_service,
                      const std::string& pref_name,
                      const base::Value* value) {
  if (!CheckUserPrefType(pref_service, pref_name, value->type())) {
    return false;
  }

  pref_service.Set(pref_name, *value);
  return true;
}

bool ClearUserPrefValue(PrefService& pref_service,
                        const std::string& pref_name) {
  if (!CheckUserPrefType(pref_service, pref_name, base::Value::Type::NONE)) {
    return false;
  }
  pref_service.ClearPref(pref_name);
  return true;
}

bool AppendValueToUserPref(PrefService& pref_service,
                           const std::string& pref_name,
                           const base::Value* value) {
  if (!CheckUserPrefType(pref_service, pref_name, base::Value::Type::LIST)) {
    return false;
  }

  if (std::ranges::contains(pref_service.GetList(pref_name), *value)) {
    CAMPAIGNS_LOG(ERROR) << "Pref value is already in the list.";
    return false;
  }

  auto& values = pref_service.GetList(pref_name);
  auto cached_values = values.Clone();
  cached_values.Append(value->Clone());
  pref_service.SetList(pref_name, std::move(cached_values));
  return true;
}

bool RemoveValueFromUserPref(PrefService& pref_service,
                             const std::string& pref_name,
                             const base::Value* value) {
  if (!CheckUserPrefType(pref_service, pref_name, base::Value::Type::LIST)) {
    return false;
  }

  if (!std::ranges::contains(pref_service.GetList(pref_name), *value)) {
    CAMPAIGNS_LOG(ERROR)
        << "Unable to remove: Pref value not in user preference.";
    return false;
  }

  auto& values = pref_service.GetList(pref_name);
  auto cached_values = values.Clone();
  cached_values.EraseValue(*value);
  pref_service.SetList(pref_name, std::move(cached_values));
  return true;
}

bool UpdateUserPrefValue(PrefService& pref_service,
                         const std::string& pref_name,
                         UpdateType type,
                         const base::Value* value) {
  switch (type) {
    case UpdateType::kSet:
      return SetUserPrefValue(pref_service, pref_name, value);
    case UpdateType::kClear:
      return ClearUserPrefValue(pref_service, pref_name);
    case UpdateType::kAppend:
      return AppendValueToUserPref(pref_service, pref_name, value);
    case UpdateType::kRemove:
      return RemoveValueFromUserPref(pref_service, pref_name, value);
  }
}

}  // namespace

UpdateUserPrefActionPerformer::UpdateUserPrefActionPerformer() = default;
UpdateUserPrefActionPerformer::~UpdateUserPrefActionPerformer() = default;

void UpdateUserPrefActionPerformer::Run(
    int campaign_id,
    std::optional<int> group_id,
    const base::DictValue* params,
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

  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const session_manager::Session* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  CHECK(active_session);
  // Null before the user's Profile is created and after it is destroyed.
  PrefService* pref_service =
      CHECK_DEREF(user_manager::UserManager::Get()->FindUserAndModify(
                      active_session->account_id()))
          .GetProfilePrefs();
  if (!pref_service) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kUserPrefServiceNotAvailable);
    CAMPAIGNS_LOG(ERROR) << "User pref service not available.";
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kUpdateUserPrefFailed);
    return;
  }

  if (!UpdateUserPrefValue(*pref_service, *pref_name, update_type, value)) {
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
