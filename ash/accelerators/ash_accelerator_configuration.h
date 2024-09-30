// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ASH_ACCELERATOR_CONFIGURATION_H_
#define ASH_ACCELERATORS_ASH_ACCELERATOR_CONFIGURATION_H_

#include <map>
#include <vector>

#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/mojom/accelerator_configuration.mojom-shared.h"
#include "ash/public/mojom/accelerator_configuration.mojom.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/accelerators/accelerator_map.h"

namespace {
// Represents the state of the accelerator modification in the prefs.
// `kAdd` - User adds a custom accelerator ontop of the default accelerators.
// `kRemove` - User removes a default accelerator.
// Removing a user-added accelerator will not result in a new action, rather it
// will remove the pref override entry with `kAdd`.
enum class AcceleratorModificationAction {
  kAdd = 0,
  kRemove = 1,
};

// Represents the underlying data of a modified accelerator in the pref
// storage.
struct AcceleratorModificationData {
  ui::Accelerator accelerator;
  AcceleratorModificationAction action;
};
}  // namespace

namespace ash {

// Implementor of AcceleratorConfiguration for Ash accelerators.
// This class exist as a way to provide access to view and modify Ash
// accelerators.
class ASH_EXPORT AshAcceleratorConfiguration : public AcceleratorConfiguration,
                                               public SessionObserver {
 public:
  // Observer to notify clients of when accelerators are updated.
  // Clients can receive a list of accelerators via
  // `AshAcceleratorConfiguration::GetAllAccelerators()`.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnAcceleratorsUpdated() = 0;
  };

  AshAcceleratorConfiguration();
  AshAcceleratorConfiguration(const AshAcceleratorConfiguration&) = delete;
  AshAcceleratorConfiguration& operator=(const AshAcceleratorConfiguration&) =
      delete;
  ~AshAcceleratorConfiguration() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Whether the source is mutable and shortcuts can be changed. If this returns
  // false then any of the Add/Remove/Replace class will DCHECK. The two Restore
  // methods will be no-ops.
  bool IsMutable() const override;
  // Return true if the accelerator is deprecated.
  bool IsDeprecated(const ui::Accelerator& accelerator) const override;
  // Return true if the accelerator data does not allow users to modify.
  bool IsAcceleratorLocked(const ui::Accelerator& accelerator) const override;
  mojom::AcceleratorConfigResult AddUserAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator) override;
  // TODO(jimmyxgong): Implement disabling accelerators after pref storage is
  // implemented.
  mojom::AcceleratorConfigResult RemoveAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator) override;
  mojom::AcceleratorConfigResult ReplaceAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& old_accelerator,
      const ui::Accelerator& new_accelerator) override;
  mojom::AcceleratorConfigResult RestoreDefault(
      AcceleratorActionId action_id) override;
  mojom::AcceleratorConfigResult RestoreAllDefaults() override;

  // SessionObserver::
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void Initialize();
  void Initialize(base::span<const AcceleratorData> accelerators);
  void InitializeDeprecatedAccelerators(
      base::span<const DeprecatedAcceleratorData> deprecated_datas,
      base::span<const AcceleratorData> deprecated_accelerators);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer);

  const AcceleratorAction* FindAcceleratorAction(
      const ui::Accelerator& accelerator) const;

  const std::vector<ui::Accelerator>& GetAllAccelerators() {
    return accelerators_;
  }

  void SetUsePositionalLookup(bool use_positional_lookup);

  // Returns a nullptr if `action` is not a deprecated action, otherwise
  // returns the deprecated data.
  const DeprecatedAcceleratorData* GetDeprecatedAcceleratorData(
      AcceleratorActionId action);

  // Returns the ID of the action if `accelerator` is a default accelerator.
  // If there is no ID found, returns std::nullopt.
  std::optional<AcceleratorAction> GetIdForDefaultAccelerator(
      ui::Accelerator accelerator);

  // Returns the default accelerators of a given accelerator ID.
  std::vector<ui::Accelerator> GetDefaultAcceleratorsForId(
      AcceleratorActionId id);

  // Returns true if the `id` is a valid ash accelerator ID.
  bool IsValid(uint32_t id) const;

 private:
  friend class AshAcceleratorConfigurationTest;

  // A map for looking up actions from accelerators.
  using AcceleratorActionMap = ui::AcceleratorMap<AcceleratorAction>;

  // AcceleratorConfiguration::
  base::optional_ref<const std::vector<ui::Accelerator>>
  GetAcceleratorsForAction(AcceleratorActionId action_id) override;

  void InitializeDeprecatedAccelerators();

  void AddAccelerators(base::span<const AcceleratorData> accelerators);

  void ApplyPrefOverrides();
  void SaveOverridePrefChanges();
  void UpdateOverrides(AcceleratorActionId action_id,
                       const ui::Accelerator& accelerator,
                       AcceleratorModificationAction action);

  // Remove the accelerator, does not notify observers.
  mojom::AcceleratorConfigResult DoRemoveAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator,
      bool save_override);

  // Adds the accelerator, does not notify observers.
  mojom::AcceleratorConfigResult DoAddAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator,
      bool save_override);

  // Replace the accelerator, does not notify observers.
  mojom::AcceleratorConfigResult DoReplaceAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& old_accelerator,
      const ui::Accelerator& new_accelerator);

  void NotifyAcceleratorsUpdated();

  void UpdateAndNotifyAccelerators();

  // Checks that the accelerators are in a valid state, if not reset back to
  // the default state and clear the override prefs.
  bool AreAcceleratorsValid();

  // Resets all accelerator mappings to the the system default.
  void ResetAllAccelerators();

  // Returns the total number of customizations for all accelerators.
  int GetTotalNumberOfModifications();

  // A local copy of the pref overrides, allows modifying the overrides before
  // updating the override pref.
  base::Value::Dict accelerator_overrides_;

  std::vector<ui::Accelerator> accelerators_;

  AcceleratorActionMap deprecated_accelerators_to_id_;

  // A map of accelerator ID's that are deprecated.
  std::map<AcceleratorActionId,
           raw_ptr<const DeprecatedAcceleratorData, CtnExperimental>>
      actions_with_deprecations_;

  // One accelerator action ID can potentially have multiple accelerators
  // associated with it.
  ActionIdToAcceleratorsMap id_to_accelerators_;
  // A map from accelerators to the AcceleratorAction values, which are used in
  // the implementation.
  AcceleratorActionMap accelerator_to_id_;

  // The following are caches for system default accelerators.
  // These should not be modified after the initial instantiation. Provides a
  // reference to the defaults in the event of customization or resets.
  // These are effectively const and should not be modified after the initial
  // data is set.
  ActionIdToAcceleratorsMap default_id_to_accelerators_cache_;
  AcceleratorActionMap default_accelerators_to_id_cache_;
  std::map<AcceleratorActionId,
           raw_ptr<const DeprecatedAcceleratorData, CtnExperimental>>
      default_actions_with_deprecations_cache_;
  AcceleratorActionMap default_deprecated_accelerators_to_id_cache_;

  // List of all observer clients.
  base::ObserverList<Observer> observer_list_;

  // Set of locked accelerators key combinations from an action while the
  // action itself may not be locked.
  base::flat_set<ui::Accelerator> locked_accelerator_set_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ASH_ACCELERATOR_CONFIGURATION_H_
