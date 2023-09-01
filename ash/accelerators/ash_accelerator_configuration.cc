// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <vector>

#include "ash/accelerators/accelerator_prefs.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_configuration.mojom-shared.h"
#include "ash/public/mojom/accelerator_configuration.mojom.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/ui/wm/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace {

using AcceleratorActionMap = ui::AcceleratorMap<ash::AcceleratorAction>;
using ::ash::mojom::AcceleratorConfigResult;

constexpr char kAcceleratorModifiersKey[] = "modifiers";
constexpr char kAcceleratorKeyCodeKey[] = "key";
constexpr char kAcceleratorTypeKey[] = "type";
constexpr char kAcceleratorStateKey[] = "state";
constexpr char kAcceleratorKeyStateKey[] = "key_state";
constexpr char kAcceleratorModificationActionKey[] = "action";

PrefService* GetActiveUserPrefService() {
  if (!ash::Shell::HasInstance()) {
    return nullptr;
  }
  CHECK(ash::Shell::Get()->session_controller());

  return ash::Shell::Get()->session_controller()->GetActivePrefService();
}

void AppendAcceleratorData(
    std::vector<ash::AcceleratorData>& data,
    base::span<const ash::AcceleratorData> accelerators) {
  data.reserve(data.size() + accelerators.size());
  for (const auto& accelerator : accelerators) {
    data.push_back(accelerator);
  }
}

base::Value AcceleratorModificationDataToValue(
    const ui::Accelerator& accelerator,
    AcceleratorModificationAction action) {
  base::Value::Dict accelerator_values;
  accelerator_values.Set(kAcceleratorModifiersKey, accelerator.modifiers());
  accelerator_values.Set(kAcceleratorKeyCodeKey,
                         static_cast<int>(accelerator.key_code()));
  accelerator_values.Set(
      kAcceleratorTypeKey,
      static_cast<int>(ash::mojom::AcceleratorType::kDefault));
  accelerator_values.Set(
      kAcceleratorStateKey,
      static_cast<int>(ash::mojom::AcceleratorState::kEnabled));
  accelerator_values.Set(kAcceleratorKeyStateKey,
                         static_cast<int>(accelerator.key_state()));
  accelerator_values.Set(kAcceleratorModificationActionKey,
                         static_cast<int>(action));
  return base::Value(std::move(accelerator_values));
}

AcceleratorModificationData ValueToAcceleratorModificationData(
    const base::Value::Dict& value) {
  absl::optional<int> keycode = value.FindInt(kAcceleratorKeyCodeKey);
  absl::optional<int> modifier = value.FindInt(kAcceleratorModifiersKey);
  absl::optional<int> modification_action =
      value.FindInt(kAcceleratorModificationActionKey);
  absl::optional<int> key_state = value.FindInt(kAcceleratorKeyStateKey);
  CHECK(keycode.has_value());
  CHECK(modifier.has_value());
  CHECK(modification_action.has_value());
  ui::Accelerator accelerator(static_cast<ui::KeyboardCode>(*keycode),
                              static_cast<int>(*modifier));
  if (key_state.has_value()) {
    accelerator.set_key_state(
        static_cast<ui::Accelerator::KeyState>(key_state.value()));
  }
  return {accelerator,
          static_cast<AcceleratorModificationAction>(*modification_action)};
}

void SetLookupMaps(base::span<const ash::AcceleratorData> accelerators,
                   ash::ActionIdToAcceleratorsMap& id_to_accelerator,
                   AcceleratorActionMap& accelerator_to_id) {
  for (const auto& acceleratorData : accelerators) {
    ui::Accelerator accelerator(acceleratorData.keycode,
                                acceleratorData.modifiers);
    accelerator.set_key_state(acceleratorData.trigger_on_press
                                  ? ui::Accelerator::KeyState::PRESSED
                                  : ui::Accelerator::KeyState::RELEASED);
    accelerator_to_id.InsertNew(
        std::make_pair(accelerator, acceleratorData.action));
    id_to_accelerator[static_cast<uint32_t>(acceleratorData.action)].push_back(
        accelerator);
  }
}

std::vector<ash::AcceleratorData> GetDefaultAccelerators() {
  std::vector<ash::AcceleratorData> accelerators;
  AppendAcceleratorData(
      accelerators,
      base::make_span(ash::kAcceleratorData, ash::kAcceleratorDataLength));

  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kEnableWithPositionalAcceleratorsData,
                        ash::kEnableWithPositionalAcceleratorsDataLength));
    AppendAcceleratorData(
        accelerators,
        base::make_span(
            ash::kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData,
            ash::
                kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorDataLength));
  } else if (::features::IsNewShortcutMappingEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kEnableWithNewMappingAcceleratorData,
                        ash::kEnableWithNewMappingAcceleratorDataLength));
  } else {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kDisableWithNewMappingAcceleratorData,
                        ash::kDisableWithNewMappingAcceleratorDataLength));
  }
  if (ash::features::IsSameAppWindowCycleEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(
            ash::kEnableWithSameAppWindowCycleAcceleratorData,
            ash::kEnableWithSameAppWindowCycleAcceleratorDataLength));
  }
  if (chromeos::wm::features::IsWindowLayoutMenuEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kEnableWithFloatWindowAcceleratorData,
                        ash::kEnableWithFloatWindowAcceleratorDataLength));
  }
  if (ash::features::IsGameDashboardEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kToggleGameDashboardAcceleratorData,
                        ash::kToggleGameDashboardAcceleratorDataLength));
  }

  // Debug accelerators.
  if (ash::debug::DebugAcceleratorsEnabled()) {
    AppendAcceleratorData(accelerators,
                          base::make_span(ash::kDebugAcceleratorData,
                                          ash::kDebugAcceleratorDataLength));
  }

  // Developer accelerators.
  if (ash::debug::DeveloperAcceleratorsEnabled()) {
    AppendAcceleratorData(
        accelerators, base::make_span(ash::kDeveloperAcceleratorData,
                                      ash::kDeveloperAcceleratorDataLength));
  }
  return accelerators;
}

}  // namespace

namespace ash {

AshAcceleratorConfiguration::AshAcceleratorConfiguration()
    : AcceleratorConfiguration(ash::mojom::AcceleratorSource::kAsh) {
  if (Shell::HasInstance()) {
    Shell::Get()->session_controller()->AddObserver(this);
  }
}
AshAcceleratorConfiguration::~AshAcceleratorConfiguration() {
  if (Shell::HasInstance()) {
    Shell::Get()->session_controller()->RemoveObserver(this);
  }
}

// TODO(jimmyxgong): Implement all functions below as these are only stubs.

// static:
void AshAcceleratorConfiguration::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  if (!::features::IsShortcutCustomizationEnabled()) {
    return;
  }

  registry->RegisterDictionaryPref(prefs::kShortcutCustomizationOverrides);
}

void AshAcceleratorConfiguration::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // A pref service may not be available in tests.
  if (!::features::IsShortcutCustomizationEnabled() || !pref_service ||
      pref_service != GetActiveUserPrefService()) {
    return;
  }

  // Store a copy of the pref overrides.
  accelerator_overrides_ =
      pref_service->GetDict(prefs::kShortcutCustomizationOverrides).Clone();
  // Reset to default first.
  ResetAllAccelerators();
  ApplyPrefOverrides();
}

const std::vector<ui::Accelerator>&
AshAcceleratorConfiguration::GetAcceleratorsForAction(
    AcceleratorActionId action_id) {
  const auto accelerator_iter = id_to_accelerators_.find(action_id);
  CHECK(accelerator_iter != id_to_accelerators_.end());

  return accelerator_iter->second;
}

bool AshAcceleratorConfiguration::IsMutable() const {
  return Shell::Get()->accelerator_prefs()->IsCustomizationAllowed();
}

bool AshAcceleratorConfiguration::IsDeprecated(
    const ui::Accelerator& accelerator) const {
  return deprecated_accelerators_to_id_.Find(accelerator);
}

const AcceleratorAction* AshAcceleratorConfiguration::FindAcceleratorAction(
    const ui::Accelerator& accelerator) const {
  // If the accelerator is deprecated, return the action ID first.
  const AcceleratorAction* deprecated_action_id =
      deprecated_accelerators_to_id_.Find(accelerator);
  if (deprecated_action_id) {
    return deprecated_action_id;
  }

  return accelerator_to_id_.Find(accelerator);
}

AcceleratorConfigResult AshAcceleratorConfiguration::AddUserAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());
  const AcceleratorConfigResult result =
      DoAddAccelerator(action_id, accelerator, /*save_override=*/true);

  if (result == AcceleratorConfigResult::kSuccess) {
    UpdateAndNotifyAccelerators();
  }

  VLOG(1) << "AddAccelerator called for ActionID: " << action_id
          << ", Accelerator: " << accelerator.GetShortcutText()
          << " returned: " << static_cast<int>(result);

  return result;
}

AcceleratorConfigResult AshAcceleratorConfiguration::RemoveAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());
  AcceleratorConfigResult result =
      DoRemoveAccelerator(action_id, accelerator, /*save_override=*/true);

  if (result == AcceleratorConfigResult::kSuccess) {
    UpdateAndNotifyAccelerators();
  }

  VLOG(1) << "RemovedAccelerator called for ActionID: " << action_id
          << ", Accelerator: " << accelerator.GetShortcutText()
          << " returned: " << static_cast<int>(result);
  return result;
}

AcceleratorConfigResult AshAcceleratorConfiguration::ReplaceAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& old_accelerator,
    const ui::Accelerator& new_accelerator) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());

  const AcceleratorConfigResult result =
      DoReplaceAccelerator(action_id, old_accelerator, new_accelerator);
  if (result == AcceleratorConfigResult::kSuccess) {
    UpdateAndNotifyAccelerators();
  }

  VLOG(1) << "ReplaceAccelerator caleld for ActionID: " << action_id
          << ", old accelerator: " << old_accelerator.GetShortcutText()
          << ", new accelerator: " << new_accelerator.GetShortcutText()
          << " returned: " << static_cast<int>(result);
  return result;
}

AcceleratorConfigResult AshAcceleratorConfiguration::RestoreDefault(
    AcceleratorActionId action_id) {
  const auto& current_accelerators = id_to_accelerators_.find(action_id);
  if (current_accelerators == id_to_accelerators_.end()) {
    VLOG(1) << "ResetAction called for ActionID: " << action_id
            << " returned with error: " << AcceleratorConfigResult::kNotFound;
    return AcceleratorConfigResult::kNotFound;
  }

  auto& accelerators_for_id = current_accelerators->second;
  // Clear reverse mapping first.
  for (const auto& accelerator : accelerators_for_id) {
    // There should never be a mismatch between the two maps, `Get()` does an
    // implicit CHECK too.
    auto& found_id = accelerator_to_id_.Get(accelerator);
    if (found_id != action_id) {
      VLOG(1) << "ResetAction called for ActionID: " << action_id
              << " returned with error: " << AcceleratorConfigResult::kNotFound;
      return AcceleratorConfigResult::kNotFound;
    }

    accelerator_to_id_.Erase(accelerator);
  }

  // Clear lookup map.
  accelerators_for_id.clear();

  // Restore the system default accelerator(s) for this action only if it the
  // default is not used by another accelerator.
  // Users will have to manually re-add the default accelerator if there exists
  // a conflict.
  const auto& defaults = default_id_to_accelerators_cache_.find(action_id);
  CHECK(defaults != default_id_to_accelerators_cache_.end());

  AcceleratorConfigResult result = AcceleratorConfigResult::kSuccess;
  // Iterate through the default and only add back the default if they're not
  // in use.
  for (const auto& default_accelerator : defaults->second) {
    if (!accelerator_to_id_.Find(default_accelerator)) {
      accelerators_for_id.push_back(default_accelerator);
      accelerator_to_id_.InsertNew(
          {default_accelerator, static_cast<AcceleratorAction>(action_id)});
    } else {
      // The default accelerator cannot be re-added since it conflicts with
      // another accelerator.
      result = AcceleratorConfigResult::kRestoreSuccessWithConflicts;
    }
  }

  // Update the pref overrides.
  std::string id = base::NumberToString(action_id);
  const auto* found_override = accelerator_overrides_.Find(id);
  if (found_override) {
    accelerator_overrides_.Remove(id);
  }

  UpdateAndNotifyAccelerators();

  VLOG(1) << "ResetAction called for ActionID: " << action_id << " returned "
          << static_cast<uint32_t>(result);
  return result;
}

AcceleratorConfigResult AshAcceleratorConfiguration::RestoreAllDefaults() {
  ResetAllAccelerators();

  // Clear the prefs to be back to default.
  accelerator_overrides_.clear();

  UpdateAndNotifyAccelerators();

  return AcceleratorConfigResult::kSuccess;
}

void AshAcceleratorConfiguration::Initialize() {
  Initialize(GetDefaultAccelerators());
  InitializeDeprecatedAccelerators();
}

void AshAcceleratorConfiguration::Initialize(
    base::span<const AcceleratorData> accelerators) {
  accelerators_.clear();
  deprecated_accelerators_to_id_.Clear();
  id_to_accelerators_.clear();
  accelerator_to_id_.Clear();
  default_accelerators_to_id_cache_.Clear();
  default_id_to_accelerators_cache_.clear();

  // Cache these accelerators as the default.
  SetLookupMaps(accelerators, default_id_to_accelerators_cache_,
                default_accelerators_to_id_cache_);

  // TODO(jimmyxgong): Before adding the accelerators to the mappings, apply
  // pref remaps.
  AddAccelerators(accelerators);
}

void AshAcceleratorConfiguration::InitializeDeprecatedAccelerators() {
  base::span<const DeprecatedAcceleratorData> deprecated_accelerator_data(
      kDeprecatedAcceleratorsData, kDeprecatedAcceleratorsDataLength);
  base::span<const AcceleratorData> deprecated_accelerators(
      kDeprecatedAccelerators, kDeprecatedAcceleratorsLength);

  InitializeDeprecatedAccelerators(std::move(deprecated_accelerator_data),
                                   std::move(deprecated_accelerators));
}

void AshAcceleratorConfiguration::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AshAcceleratorConfiguration::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// This function must only be called after Initialize().
void AshAcceleratorConfiguration::InitializeDeprecatedAccelerators(
    base::span<const DeprecatedAcceleratorData> deprecated_data,
    base::span<const AcceleratorData> deprecated_accelerators) {
  for (const auto& data : deprecated_data) {
    actions_with_deprecations_[data.action] = &data;
  }

  for (const auto& data : deprecated_accelerators) {
    deprecated_accelerators_to_id_.InsertNew(
        {{data.keycode, data.modifiers},
         static_cast<AcceleratorAction>(data.action)});
  }

  // Cache a copy of the default deprecated accelerators.
  default_actions_with_deprecations_cache_ = actions_with_deprecations_;
  default_deprecated_accelerators_to_id_cache_ = deprecated_accelerators_to_id_;
  UpdateAndNotifyAccelerators();
}

void AshAcceleratorConfiguration::AddAccelerators(
    base::span<const AcceleratorData> accelerators) {
  SetLookupMaps(accelerators, id_to_accelerators_, accelerator_to_id_);
  UpdateAndNotifyAccelerators();
}

AcceleratorConfigResult AshAcceleratorConfiguration::DoRemoveAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator,
    bool save_override) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());

  // If the accelerator is deprecated, remove it.
  const AcceleratorAction* deprecated_action_id =
      deprecated_accelerators_to_id_.Find(accelerator);
  if (deprecated_action_id && *deprecated_action_id == action_id) {
    deprecated_accelerators_to_id_.Erase(accelerator);
    // Check if there are any other accelerators associated with `action_id`.
    // If not, remove it from `actions_with_deprecations_`.
    bool has_more_deprecated_accelerators = false;
    for (const auto& deprecated_iter : deprecated_accelerators_to_id_) {
      if (deprecated_iter.second == action_id) {
        has_more_deprecated_accelerators = true;
        break;
      }
    }

    if (!has_more_deprecated_accelerators) {
      actions_with_deprecations_.erase(action_id);
    }
    return AcceleratorConfigResult::kSuccess;
  }

  const AcceleratorAction* found_id = accelerator_to_id_.Find(accelerator);
  auto found_accelerators_iter = id_to_accelerators_.find(action_id);
  if (found_accelerators_iter == id_to_accelerators_.end() || !found_id) {
    return AcceleratorConfigResult::kNotFound;
  }

  CHECK(*found_id == action_id);

  // Remove accelerator from lookup map.
  base::Erase(found_accelerators_iter->second, accelerator);

  // Remove accelerator from reverse lookup map.
  accelerator_to_id_.Erase(accelerator);

  // Store the final state of `action_id`.
  if (save_override) {
    UpdateOverrides(action_id, accelerator,
                    AcceleratorModificationAction::kRemove);
  }

  return AcceleratorConfigResult::kSuccess;
}

AcceleratorConfigResult AshAcceleratorConfiguration::DoAddAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator,
    bool save_override) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());

  const auto& accelerators_iter = id_to_accelerators_.find(action_id);
  if (accelerators_iter == id_to_accelerators_.end()) {
    return AcceleratorConfigResult::kNotFound;
  }

  // Check if `accelerator` is already used, in-use or deprecated. If so
  // remove/disable it.
  const auto* conflict_action_id = FindAcceleratorAction(accelerator);
  if (conflict_action_id) {
    // If the conflicting accelerator is NOT the default for culprit action id,
    // then we should update the override accordingly. Otherwise, we do not
    // save the override as it will be handled implicitly when applying the
    // prefs.
    bool save_remove_override = true;
    absl::optional<AcceleratorAction> conflict_accelerator_default_id =
        GetIdForDefaultAccelerator(accelerator);
    if (conflict_accelerator_default_id.has_value()) {
      save_remove_override =
          conflict_accelerator_default_id.value() != *conflict_action_id;
    }
    const AcceleratorConfigResult remove_result = DoRemoveAccelerator(
        *conflict_action_id, accelerator, save_remove_override);
    if (remove_result != AcceleratorConfigResult::kSuccess) {
      return remove_result;
    }
  }

  // Add the accelerator.
  auto& accelerators = accelerators_iter->second;
  accelerators.push_back(accelerator);
  accelerator_to_id_.InsertNew(
      {accelerator, static_cast<AcceleratorAction>(action_id)});

  if (save_override) {
    // Update pref overrides.
    UpdateOverrides(action_id, accelerator,
                    AcceleratorModificationAction::kAdd);
  }

  return AcceleratorConfigResult::kSuccess;
}

mojom::AcceleratorConfigResult
AshAcceleratorConfiguration::DoReplaceAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& old_accelerator,
    const ui::Accelerator& new_accelerator) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());

  // Check that `old_accelerator` belongs to `action_id`.
  const AcceleratorAction* found_id = accelerator_to_id_.Find(old_accelerator);
  if (!found_id || *found_id != action_id) {
    return AcceleratorConfigResult::kNotFound;
  }

  // First remove the old accelerator.
  const AcceleratorConfigResult remove_result =
      DoRemoveAccelerator(action_id, old_accelerator, /*save_override=*/true);
  if (remove_result != AcceleratorConfigResult::kSuccess) {
    return remove_result;
  }

  // Now add the new accelerator.
  return DoAddAccelerator(action_id, new_accelerator, /*save_override=*/true);
}

const DeprecatedAcceleratorData*
AshAcceleratorConfiguration::GetDeprecatedAcceleratorData(
    AcceleratorActionId action) {
  auto it = actions_with_deprecations_.find(action);
  if (it == actions_with_deprecations_.end()) {
    return nullptr;
  }
  return it->second;
}

void AshAcceleratorConfiguration::NotifyAcceleratorsUpdated() {
  if (!Shell::Get()->accelerator_prefs()->IsCustomizationAllowed()) {
    return;
  }

  for (auto& observer : observer_list_) {
    observer.OnAcceleratorsUpdated();
  }
}

absl::optional<AcceleratorAction>
AshAcceleratorConfiguration::GetIdForDefaultAccelerator(
    ui::Accelerator accelerator) {
  AcceleratorAction* found_id =
      default_accelerators_to_id_cache_.Find(accelerator);
  return found_id ? absl::optional<AcceleratorAction>(*found_id)
                  : absl::nullopt;
}

std::vector<ui::Accelerator>
AshAcceleratorConfiguration::GetDefaultAcceleratorsForId(
    AcceleratorActionId id) {
  const auto iter = default_id_to_accelerators_cache_.find(id);

  if (iter == default_id_to_accelerators_cache_.end()) {
    VLOG(1) << "No default accelerators were found for id: " << id;
    return std::vector<ui::Accelerator>();
  }

  return iter->second;
}

bool AshAcceleratorConfiguration::IsValid(uint32_t id) const {
  return id_to_accelerators_.contains(id) &&
         default_id_to_accelerators_cache_.contains(id);
}

void AshAcceleratorConfiguration::UpdateAndNotifyAccelerators() {
  // Re-populate `accelerators_` which contains all currently available
  // accelerators and deprecated accelerators, if present.
  accelerators_.clear();
  accelerators_.reserve(accelerator_to_id_.size() +
                        deprecated_accelerators_to_id_.size());
  for (const auto& [accel, action_id] : accelerator_to_id_) {
    accelerators_.push_back(accel);
  }

  for (const auto& [accel, action_id] : deprecated_accelerators_to_id_) {
    accelerators_.push_back(accel);
  }

  UpdateAccelerators(id_to_accelerators_);
  NotifyAcceleratorsUpdated();
  if (Shell::Get()->accelerator_prefs()->IsCustomizationAllowed()) {
    SaveOverridePrefChanges();
  }
}

void AshAcceleratorConfiguration::SaveOverridePrefChanges() {
  auto* pref_service = GetActiveUserPrefService();
  if (pref_service) {
    pref_service->SetDict(prefs::kShortcutCustomizationOverrides,
                          accelerator_overrides_.Clone());
  }
}

void AshAcceleratorConfiguration::ApplyPrefOverrides() {
  // Stores all actions with prefs to be removed, this gets populated if there
  // are malformed prefs which results in an empty pref after removal.
  std::vector<uint32_t> actions_to_be_removed;

  for (auto entry : accelerator_overrides_) {
    int action_id;
    base::StringToInt(entry.first, &action_id);
    if (!IsValid(action_id)) {
      actions_to_be_removed.push_back(action_id);
      continue;
    }

    base::Value::List& override_list = entry.second.GetList();
    CHECK(!override_list.empty());

    auto override_list_iter = override_list.begin();
    while (override_list_iter != override_list.end()) {
      base::Value::Dict& override_dict = override_list_iter->GetDict();
      AcceleratorModificationData override_data =
          ValueToAcceleratorModificationData(override_dict);
      if (override_data.action == AcceleratorModificationAction::kRemove) {
        // Race condition:
        // If the user has disabled the default accelerator but then adds
        // it to another action, we do not attempt to remove here.
        const auto* found_id =
            accelerator_to_id_.Find(override_data.accelerator);

        // If the pref has an accelerator that is invalid, do not attempt to
        // apply the pref and remove it.
        if (!found_id) {
          override_list_iter = override_list.erase(override_list_iter);
          // If removing the pref results in an empty pref, remove it and move
          // onto the next override pref.
          if (override_list.empty()) {
            actions_to_be_removed.push_back(action_id);
            break;
          }
          continue;
        }
        if (*found_id == action_id) {
          DoRemoveAccelerator(action_id, override_data.accelerator,
                              /*save_override=*/false);
        }
      }

      if (override_data.action == AcceleratorModificationAction::kAdd) {
        DoAddAccelerator(action_id, override_data.accelerator,
                         /*save_override=*/false);
      }
      ++override_list_iter;
    }
  }

  // Remove all empty override prefs.
  for (uint32_t action : actions_to_be_removed) {
    accelerator_overrides_.Remove(base::NumberToString(action));
  }

  // Check if the overridden accelerators are valid, if not then restore all
  // defaults.
  if (!AreAcceleratorsValid()) {
    RestoreAllDefaults();
  }

  UpdateAndNotifyAccelerators();
}

void AshAcceleratorConfiguration::UpdateOverrides(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator,
    AcceleratorModificationAction action) {
  const std::string action_id_key = base::NumberToString(action_id);
  base::Value* action_entry = accelerator_overrides_.Find(action_id_key);

  if (!action_entry) {
    base::Value::List accelerator_override_list;
    // No existing overrides, add the override entry and return.
    accelerator_override_list.Append(
        AcceleratorModificationDataToValue(accelerator, action));
    accelerator_overrides_.Set(action_id_key,
                               std::move(accelerator_override_list));
    return;
  }

  base::Value::List& override_list = action_entry->GetList();
  CHECK(!override_list.empty());
  // Iterate through the override list, check if the accelerator already exist
  // for `action_id`.
  for (auto override_iter = override_list.begin();
       override_iter != override_list.end(); ++override_iter) {
    const AcceleratorModificationData accelerator_data =
        ValueToAcceleratorModificationData(override_iter->GetDict());
    if (accelerator == accelerator_data.accelerator) {
      // It's not possible to perform the same action to the same accelerator.
      CHECK(accelerator_data.action != action);
      // The accelerator already has already been modified and the new
      // action differs from the previous modification action. This can happen
      // if the user has added a new custom accelerator and then removes it.
      // Or if the user disables a default accelerator and re-enables it.
      override_iter = override_list.erase(override_iter);
      if (override_list.empty()) {
        // If the override list is empty, no changes are made to `action_id`,
        // remove its override entry.
        accelerator_overrides_.Remove(action_id_key);
      }
      return;
    }
  }

  // The accelerator was not present in existing overrides, append this
  // accelerator.
  override_list.Append(AcceleratorModificationDataToValue(accelerator, action));
}

bool AshAcceleratorConfiguration::AreAcceleratorsValid() {
  // Iterate through the lookup map and verify that both lookup and reverse
  // lookup maps are in sync.
  // TODO(jimmyxgong): Consider resetting the override pref here if invalid
  // pref is found.
  for (const auto& [action_id, accelerators] : id_to_accelerators_) {
    // Perform reverse lookup of each of the accelerators and check that they
    // match `action_id`.
    for (const auto& accelerator : accelerators) {
      const AcceleratorAction* found_id = accelerator_to_id_.Find(accelerator);
      if (!found_id || *found_id != action_id) {
        LOG(ERROR) << "Shortcut override prefs are out of sync. Lookup map"
                   << " has an extra accelerator: "
                   << accelerator.GetShortcutText()
                   << "Reverting to default accelerators.";
        RestoreAllDefaults();
        return false;
      }
    }
  }

  // Now iterate through the reverse lookup. This is to check that all
  // accelerators in the reverse lookup are valid.
  for (const auto& [accelerator, action_id] : accelerator_to_id_) {
    const auto& id_to_accelerator_iter = id_to_accelerators_.find(action_id);
    if (id_to_accelerator_iter == id_to_accelerators_.end()) {
      LOG(ERROR) << "Shortcut overide prefs are out of sync, reverse lookup "
                 << " has an extra action id: " << action_id
                 << " Reverting to default accelerators.";
      RestoreAllDefaults();
      return false;
    }
    if (base::ranges::find(id_to_accelerator_iter->second, accelerator) ==
        id_to_accelerator_iter->second.end()) {
      LOG(ERROR) << "Shortcut overide prefs are out of sync, reverse lookup "
                 << "has an extra accelerator: "
                 << accelerator.GetShortcutText() << " for id: " << action_id
                 << " Reverting to default accelerators.";
      RestoreAllDefaults();
      return false;
    }
  }

  return true;
}

void AshAcceleratorConfiguration::ResetAllAccelerators() {
  accelerators_.clear();
  id_to_accelerators_.clear();
  accelerator_to_id_.Clear();
  deprecated_accelerators_to_id_.Clear();
  actions_with_deprecations_.clear();

  id_to_accelerators_ = default_id_to_accelerators_cache_;
  accelerator_to_id_ = default_accelerators_to_id_cache_;

  deprecated_accelerators_to_id_ = default_deprecated_accelerators_to_id_cache_;
  actions_with_deprecations_ = default_actions_with_deprecations_cache_;
}

}  // namespace ash
