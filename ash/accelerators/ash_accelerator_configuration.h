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
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/accelerators/accelerator_map.h"

namespace ash {

// Implementor of AcceleratorConfiguration for Ash accelerators.
// This class exist as a way to provide access to view and modify Ash
// accelerators.
class ASH_EXPORT AshAcceleratorConfiguration : public AcceleratorConfiguration {
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

  // AcceleratorConfiguration::
  const std::vector<ui::Accelerator>& GetAcceleratorsForAction(
      AcceleratorActionId action_id) override;
  // Whether the source is mutable and shortcuts can be changed. If this returns
  // false then any of the Add/Remove/Replace class will DCHECK. The two Restore
  // methods will be no-ops.
  bool IsMutable() const override;
  // Return true if the accelerator is deprecated.
  bool IsDeprecated(const ui::Accelerator& accelerator) const override;
  AcceleratorConfigResult AddUserAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator) override;
  // TODO(jimmyxgong): Implement disabling accelerators after pref storage is
  // implemented.
  AcceleratorConfigResult RemoveAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator) override;
  AcceleratorConfigResult ReplaceAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& old_acc,
      const ui::Accelerator& new_acc) override;
  AcceleratorConfigResult RestoreDefault(
      AcceleratorActionId action_id) override;
  AcceleratorConfigResult RestoreAllDefaults() override;

  void Initialize();
  void Initialize(base::span<const AcceleratorData> accelerators);
  void InitializeDeprecatedAccelerators(
      base::span<const DeprecatedAcceleratorData> deprecated_datas,
      base::span<const AcceleratorData> deprecated_accelerators);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  AcceleratorAction* FindAcceleratorAction(const ui::Accelerator& accelerator) {
    return accelerator_to_id_.Find(accelerator);
  }

  const AcceleratorAction* FindAcceleratorAction(
      const ui::Accelerator& accelerator) const {
    return accelerator_to_id_.Find(accelerator);
  }

  AcceleratorAction& GetAcceleratorAction(const ui::Accelerator& accelerator) {
    return accelerator_to_id_.Get(accelerator);
  }

  const AcceleratorAction& GetAcceleratorAction(
      const ui::Accelerator& accelerator) const {
    return accelerator_to_id_.Get(accelerator);
  }

  const std::vector<ui::Accelerator>& GetAllAccelerators() {
    return accelerators_;
  }

  void SetUsePositionalLookup(bool use_positional_lookup) {
    accelerator_to_id_.set_use_positional_lookup(use_positional_lookup);
  }

  // Returns a nullptr if `action` is not a deprecated action, otherwise
  // returns the deprecated data.
  const DeprecatedAcceleratorData* GetDeprecatedAcceleratorData(
      AcceleratorActionId action);

  // Returns the ID of the action if `accelerator` is a default accelerator.
  // If there is no ID found, returns absl::nullopt.
  absl::optional<AcceleratorAction> GetIdForDefaultAccelerator(
      ui::Accelerator accelerator);

  // Returns the default accelerators of a given accelerator ID.
  std::vector<ui::Accelerator> GetDefaultAcceleratorsForId(
      AcceleratorActionId id);

 private:
  // A map for looking up actions from accelerators.
  using AcceleratorActionMap = ui::AcceleratorMap<AcceleratorAction>;

  void InitializeDeprecatedAccelerators();

  void AddAccelerators(base::span<const AcceleratorData> accelerators);

  // Remove the accelerator, does not notify observers.
  AcceleratorConfigResult DoRemoveAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator);

  void NotifyAcceleratorsUpdated();

  void UpdateAndNotifyAccelerators();

  std::vector<ui::Accelerator> accelerators_;

  base::flat_set<ui::Accelerator> deprecated_accelerators_;

  // A map of accelerator ID's that are deprecated.
  std::map<AcceleratorActionId, const DeprecatedAcceleratorData*>
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

  // List of all observer clients.
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ASH_ACCELERATOR_CONFIGURATION_H_
