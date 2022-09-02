// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ASH_ACCELERATOR_CONFIGURATION_H_
#define ASH_ACCELERATORS_ASH_ACCELERATOR_CONFIGURATION_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/mojom/accelerator_info.mojom.h"

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

 private:
  std::vector<AcceleratorInfo> accelerator_infos_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ASH_ACCELERATOR_CONFIGURATION_H_
