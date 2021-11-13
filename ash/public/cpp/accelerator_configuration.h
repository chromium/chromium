// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATOR_CONFIGURATION_H_
#define ASH_PUBLIC_CPP_ACCELERATOR_CONFIGURATION_H_

#include <map>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "base/callback.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

// Represents the type of accelerator.
enum class AcceleratorType {
  kDefault,     // System default
  kUser,        // User added accelerator
  kDeprecated,  // Deprecated accelerator
  kDeveloper,   // Accelerator used for developer mode
  kDebug,       // Used only for debugging
};

// Represents the current state of an accelerator.
enum class AcceleratorState {
  kEnabled,             // Accelerator available
  kDisabledByConflict,  // Accelerator is disabled due to a conflict
  kDisabledByUser,      // User disabled the shortcut e.g. disabling a default
};

// Error codes associated with mutating accelerators.
enum class AcceleratorConfigResult {
  kSuccess,            // Success
  kActionLocked,       // Failure, locked actions cannot be modified
  kAcceleratorLocked,  // Failure, locked accelerators cannot be modified
  kConflict,           // Transient failure, conflict with existing accelerator
  kNotFound,           // Failure, accelerator not found
  kDuplicate,          // Failure, adding a duplicate accelerator to the same
                       // action
};

struct ASH_PUBLIC_EXPORT AcceleratorInfo {
  AcceleratorInfo(AcceleratorType type,
                  ui::Accelerator accelerator,
                  bool locked)
      : type(type), accelerator(accelerator), locked(locked) {}
  AcceleratorType type;
  ui::Accelerator accelerator;
  // Whether the accelerator can be modified.
  bool locked = true;
  // Accelerators are enabled by default.
  AcceleratorState state = AcceleratorState::kEnabled;
};

using AcceleratorAction = uint32_t;

// The public-facing interface for shortcut providers, this should be
// implemented by sources, e.g. Browser, Ash, that want their shortcuts to be
// exposed to separate clients.
class ASH_PUBLIC_EXPORT AcceleratorConfiguration {
 public:
  using AcceleratorsUpdatedCallback = base::RepeatingCallback<void(
      ash::mojom::AcceleratorSource,
      std::multimap<AcceleratorAction, AcceleratorInfo>)>;

  explicit AcceleratorConfiguration(ash::mojom::AcceleratorSource source);
  virtual ~AcceleratorConfiguration();

  // Callback will fire immediately once after updating.
  void AddAcceleratorsUpdatedCallback(AcceleratorsUpdatedCallback callback);

  void RemoveAcceleratorsUpdatedCallback(AcceleratorsUpdatedCallback callback);

  // Get the accelerators for a single action.
  virtual const std::vector<AcceleratorInfo>& GetConfigForAction(
      AcceleratorAction actionId) = 0;

  // Whether this source of shortcuts can be modified. If this returns false
  // then any of the Add/Remove/Replace class will DCHECK. The two Restore
  // methods will be no-ops.
  virtual bool IsMutable() const = 0;

  // Add a new user defined accelerator.
  virtual AcceleratorConfigResult AddUserAccelerator(
      AcceleratorAction action,
      const ui::Accelerator& accelerator) = 0;

  // Remove a shortcut. This will delete a user-defined shortcut, or
  // mark a default one disabled.
  virtual AcceleratorConfigResult RemoveAccelerator(
      AcceleratorAction action,
      const ui::Accelerator& accelerator) = 0;

  // Atomic version of Remove then Add.
  virtual AcceleratorConfigResult ReplaceAccelerator(
      AcceleratorAction action,
      const ui::Accelerator& old_acc,
      const ui::Accelerator& new_acc) = 0;

  // Restore the defaults for the given action.
  virtual AcceleratorConfigResult RestoreDefault(AcceleratorAction action) = 0;

  // Restore all defaults.
  virtual AcceleratorConfigResult RestoreAllDefaults() = 0;

 protected:
  void NotifyAcceleratorsUpdated(
      const std::multimap<AcceleratorAction, AcceleratorInfo>& accelerators);

 private:
  // The source of the accelerators. Derived classes are responsible for only
  // one source.
  const ash::mojom::AcceleratorSource source_;

  // Container of all invoked callbacks when the accelerators are updated. Call
  // AddAcceleratorsUpdatedCallback or RemoveAcceleratorsUpdatedCallback to
  // add/remove callbacks to the container.
  std::vector<AcceleratorsUpdatedCallback> callbacks_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATOR_CONFIGURATION_H_
