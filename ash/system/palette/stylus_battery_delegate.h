// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_STYLUS_BATTERY_DELEGATE_H_
#define ASH_SYSTEM_PALETTE_STYLUS_BATTERY_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class ASH_EXPORT StylusBatteryDelegate {
 public:
  StylusBatteryDelegate();
  StylusBatteryDelegate(const StylusBatteryDelegate& other) = delete;
  StylusBatteryDelegate& operator=(const StylusBatteryDelegate& other) = delete;

  SkColor GetColorForBatteryLevel() const;
  int GetLabelIdForBatteryLevel() const;
  gfx::ImageSkia GetBatteryImage() const;

  base::Optional<uint8_t> battery_level() const { return battery_level_; }

 private:
  base::Optional<uint8_t> battery_level_;

  // Peripheral battery observer to be added here.
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_STYLUS_BATTERY_DELEGATE_H_
