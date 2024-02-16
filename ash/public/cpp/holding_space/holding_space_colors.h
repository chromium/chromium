// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_COLORS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_COLORS_H_

#include <variant>

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"

namespace ash {

// Contains the colors for dark/light modes respectively.
class ASH_PUBLIC_EXPORT HoldingSpaceColors {
 public:
  HoldingSpaceColors(SkColor dark_mode, SkColor light_mode);
  HoldingSpaceColors(const HoldingSpaceColors&);
  HoldingSpaceColors& operator=(const HoldingSpaceColors&);
  ~HoldingSpaceColors();

  bool operator==(const HoldingSpaceColors&) const;

  SkColor dark_mode() const { return dark_mode_; }
  SkColor light_mode() const { return light_mode_; }

 private:
  SkColor dark_mode_;
  SkColor light_mode_;
};

using HoldingSpaceColorVariant = std::variant<ui::ColorId, HoldingSpaceColors>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_COLORS_H_
