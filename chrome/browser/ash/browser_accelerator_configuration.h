// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_ACCELERATOR_CONFIGURATION_H_
#define CHROME_BROWSER_ASH_BROWSER_ACCELERATOR_CONFIGURATION_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ui/base/accelerators/accelerator.h"

#include <vector>

namespace ash {

// Implementor of AcceleratorConfiguration for LaCrOS/Chrome accelerators.
// The delegate exists here so that it is able to fetch browser accelerators
// from chrome/browser/ui/views and route it back to services in ash/.
// This class maintains a Mojo connection with a service in
// chrome/browser/ui/views in order to fetch LaCrOS specific shortcuts.
class ASH_EXPORT BrowserAcceleratorConfiguration
    : public AcceleratorConfiguration {
 public:
  BrowserAcceleratorConfiguration();
  BrowserAcceleratorConfiguration(const BrowserAcceleratorConfiguration&) =
      delete;
  BrowserAcceleratorConfiguration& operator=(
      const BrowserAcceleratorConfiguration&) = delete;
  ~BrowserAcceleratorConfiguration() override;

  // AcceleratorConfiguration:
  const std::vector<ui::Accelerator>& GetAcceleratorsForAction(
      AcceleratorActionId action_id) override;
  bool IsMutable() const override;
  bool IsDeprecated(const ui::Accelerator& accelerator) const override;
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
  std::vector<ui::Accelerator> accelerators_;
  std::vector<mojom::AcceleratorLayoutInfoPtr> layout_infos_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_ACCELERATOR_CONFIGURATION_H_
