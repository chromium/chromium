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
#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/accelerators/accelerator_map.h"

namespace ash {

// Implementor of AcceleratorConfiguration for Ash accelerators.
// This class exist as a way to provide access to view and modify Ash
// accelerators.
class ASH_EXPORT AshAcceleratorConfiguration : public AcceleratorConfiguration {
 public:
  AshAcceleratorConfiguration();
  AshAcceleratorConfiguration(const AshAcceleratorConfiguration&) = delete;
  AshAcceleratorConfiguration& operator=(const AshAcceleratorConfiguration&) =
      delete;
  ~AshAcceleratorConfiguration() override;

  // AcceleratorConfiguration::
  const std::vector<mojom::AcceleratorLayoutInfoPtr>&
  GetAcceleratorLayoutInfos() override;
  const std::vector<AcceleratorInfo>& GetConfigForAction(
      AcceleratorActionId action_id) override;
  bool IsMutable() const override;
  AcceleratorConfigResult AddUserAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator) override;
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

  const std::vector<AcceleratorInfo>& GetAllAcceleratorInfos() {
    return accelerator_infos_;
  }

  void SetUsePositionalLookup(bool use_positional_lookup) {
    accelerator_to_id_.set_use_positional_lookup(use_positional_lookup);
  }

  // Returns a nullptr if `action` is not a deprecated action, otherwise
  // returns the deprecated data.
  const DeprecatedAcceleratorData* GetDeprecatedAcceleratorData(
      AcceleratorActionId action);

  // Return true if the accelerator is deprecated.
  bool IsDeprecated(const ui::Accelerator& accelerator);

 private:
  // A map for looking up actions from accelerators.
  using AcceleratorActionMap = ui::AcceleratorMap<AcceleratorAction>;

  void InitializeDeprecatedAccelerators();

  void AddLayoutInfo(const AcceleratorData& data);

  void AddAccelerators(base::span<const AcceleratorData> accelerators,
                       mojom::AcceleratorType type);

  std::vector<AcceleratorInfo> accelerator_infos_;

  // A map of accelerator ID's that are deprecated.
  std::map<AcceleratorActionId, const DeprecatedAcceleratorData*>
      actions_with_deprecations_;

  // One accelerator action ID can potentially have multiple accelerators
  // associated with it.
  std::map<AcceleratorActionId, std::vector<AcceleratorInfo>>
      id_to_accelerator_infos_;
  // A map from accelerators to the AcceleratorAction values, which are used in
  // the implementation.
  AcceleratorActionMap accelerator_to_id_;
  std::vector<mojom::AcceleratorLayoutInfoPtr> layout_infos_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ASH_ACCELERATOR_CONFIGURATION_H_
