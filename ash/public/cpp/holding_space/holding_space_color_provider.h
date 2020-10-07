// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_COLOR_PROVIDER_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_COLOR_PROVIDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// The interface for the singleton which provides colors to holding space.
class ASH_PUBLIC_EXPORT HoldingSpaceColorProvider {
 public:
  virtual ~HoldingSpaceColorProvider();

  // Returns the singleton instance.
  static HoldingSpaceColorProvider* Get();

  // Returns the background color for the bubble.
  virtual SkColor GetBackgroundColor() const = 0;

  // Returns the color to be used for file icons.
  virtual SkColor GetFileIconColor() const = 0;

 protected:
  HoldingSpaceColorProvider();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_COLOR_PROVIDER_H_
