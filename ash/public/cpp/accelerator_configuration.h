// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATOR_CONFIGURATION_H_
#define ASH_PUBLIC_CPP_ACCELERATOR_CONFIGURATION_H_

#include <map>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/mojom/accelerator_configuration.mojom.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/functional/callback.h"
#include "base/types/optional_ref.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

using AcceleratorActionId = uint32_t;
using ActionIdToAcceleratorsMap =
    std::map<AcceleratorActionId, std::vector<ui::Accelerator>>;

// The public-facing interface for shortcut providers, this should be
// implemented by sources, e.g. Browser, Ash, that want their shortcuts to be
// exposed to separate clients.
class ASH_PUBLIC_EXPORT AcceleratorConfiguration {
 public:
  using AcceleratorsUpdatedCallback =
      base::RepeatingCallback<void(ash::mojom::AcceleratorSource,
                                   const ActionIdToAcceleratorsMap&)>;

  explicit AcceleratorConfiguration(ash::mojom::AcceleratorSource source);
  virtual ~AcceleratorConfiguration();

  // Callback will fire immediately once after updating.
  void AddAcceleratorsUpdatedCallback(AcceleratorsUpdatedCallback callback);

  void RemoveAcceleratorsUpdatedCallback(AcceleratorsUpdatedCallback callback);

  // Get the accelerators for a single action.
  virtual base::optional_ref<const std::vector<ui::Accelerator>>
  GetAcceleratorsForAction(AcceleratorActionId action_id) = 0;

  // Whether this source of shortcuts can be modified. If this returns false
  // then any of the Add/Remove/Replace class will DCHECK. The two Restore
  // methods will be no-ops.
  virtual bool IsMutable() const = 0;

  // Return true if the accelerator is deprecated.
  virtual bool IsDeprecated(const ui::Accelerator& accelerator) const = 0;

  // Return true if the accelerator data does not allow users to modify.
  virtual bool IsAcceleratorLocked(
      const ui::Accelerator& accelerator) const = 0;

  // Add a new user defined accelerator.
  virtual mojom::AcceleratorConfigResult AddUserAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator) = 0;

  // Remove a shortcut. This will delete a user-defined shortcut, or
  // mark a default one disabled.
  virtual mojom::AcceleratorConfigResult RemoveAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator) = 0;

  // Atomic version of Remove then Add.
  virtual mojom::AcceleratorConfigResult ReplaceAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& old_acc,
      const ui::Accelerator& new_acc) = 0;

  // Restore the defaults for the given action.
  virtual mojom::AcceleratorConfigResult RestoreDefault(
      AcceleratorActionId action_id) = 0;

  // Restore all defaults.
  virtual mojom::AcceleratorConfigResult RestoreAllDefaults() = 0;

 protected:
  // Updates the local cache and notifies observers of the updated accelerators.
  void UpdateAccelerators(const ActionIdToAcceleratorsMap& accelerators);

 private:
  void NotifyAcceleratorsUpdated();

  // The source of the accelerators. Derived classes are responsible for only
  // one source.
  const ash::mojom::AcceleratorSource source_;

  // Container of all invoked callbacks when the accelerators are updated. Call
  // AddAcceleratorsUpdatedCallback or RemoveAcceleratorsUpdatedCallback to
  // add/remove callbacks to the container.
  std::vector<AcceleratorsUpdatedCallback> callbacks_;

  // Keep a cache of the accelerator map, it's possible that adding a new
  // observer is done after initializing the accelerator mapping. This lets
  // new observers to get the immediate cached mapping.
  ActionIdToAcceleratorsMap accelerator_mapping_cache_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATOR_CONFIGURATION_H_
