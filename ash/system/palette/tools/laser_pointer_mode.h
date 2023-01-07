// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TOOLS_LASER_POINTER_MODE_H_
#define ASH_SYSTEM_PALETTE_TOOLS_LASER_POINTER_MODE_H_

#include "ash/ash_export.h"
#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/system/palette/common_palette_tool.h"
#include "base/scoped_observation.h"

namespace ash {

// Controller for the laser pointer functionality.
class ASH_EXPORT LaserPointerMode : public CommonPaletteTool,
                                    public LaserPointerObserver {
 public:
  explicit LaserPointerMode(Delegate* delegate);

  LaserPointerMode(const LaserPointerMode&) = delete;
  LaserPointerMode& operator=(const LaserPointerMode&) = delete;

  ~LaserPointerMode() override;

 private:
  // LaserPointerObserver:
  void OnLaserPointerStateChanged(bool enabled) override;

  // PaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  void OnDisable() override;
  const gfx::VectorIcon& GetActiveTrayIcon() const override;
  views::View* CreateView() override;

  // CommonPaletteTool:
  const gfx::VectorIcon& GetPaletteIcon() const override;

  base::ScopedObservation<LaserPointerController, LaserPointerObserver>
      laser_pointer_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_LASER_POINTER_MODE_H_
