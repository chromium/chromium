// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SOLID_COLOR_ANALYZER_H_
#define CC_PAINT_SOLID_COLOR_ANALYZER_H_

#include <optional>
#include <vector>

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {
class PaintOpBuffer;

class CC_PAINT_EXPORT SolidColorAnalyzer {
 public:
  SolidColorAnalyzer() = delete;

  static std::optional<SkColor4f> DetermineIfSolidColor(
      const PaintOpBuffer& buffer,
      const gfx::Rect& rect,
      int max_ops_to_analyze,
      const std::vector<size_t>* offsets = nullptr);
};

}  // namespace cc

#endif  // CC_PAINT_SOLID_COLOR_ANALYZER_H_
