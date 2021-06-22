// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace holding_space_util {

// Returns the maximum image size required for a holding space item of `type`.
ASH_PUBLIC_EXPORT gfx::Size GetMaxImageSizeForType(HoldingSpaceItem::Type type);

}  // namespace holding_space_util
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
