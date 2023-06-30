// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_BATTERY_IMAGE_SOURCE_H_
#define ASH_SYSTEM_POWER_BATTERY_IMAGE_SOURCE_H_

#include "ash/system/power/power_status.h"
#include "ui/gfx/image/canvas_image_source.h"

namespace ash {

// ImageSource for the battery icon, used in system tray and network detailed
// page.
class ASH_EXPORT BatteryImageSource : public gfx::CanvasImageSource {
 public:
  BatteryImageSource(const PowerStatus::BatteryImageInfo& info,
                     int height,
                     const BatteryColors& colors);

  BatteryImageSource(BatteryImageSource&) = delete;
  BatteryImageSource operator=(BatteryImageSource&) = delete;
  ~BatteryImageSource() override;

  // gfx::ImageSkiaSource implementation.
  void Draw(gfx::Canvas* canvas) override;

  bool HasRepresentationAtAllScales() const override;

 private:
  PowerStatus::BatteryImageInfo info_;
  BatteryColors resolved_colors_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_BATTERY_IMAGE_SOURCE_H_
