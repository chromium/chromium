// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_COLOR_MAP_H_
#define CC_PAINT_SKOTTIE_COLOR_MAP_H_

#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

using SkottieColorMap = base::flat_map<SkottieResourceIdHash, SkColor>;

CC_PAINT_EXPORT inline SkottieColorMap::value_type SkottieMapColor(
    std::string_view name,
    SkColor color) {
  return std::make_pair(HashSkottieResourceId(name), color);
}

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_COLOR_MAP_H_
