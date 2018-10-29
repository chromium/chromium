// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/color_helpers.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "ui/gfx/color_utils.h"

std::string OptionalSkColorToString(const base::Optional<SkColor>& color) {
  if (!color)
    return std::string();
  return color_utils::SkColorToRgbaString(*color);
}

int64_t OptionalSkColorToJavaColor(const base::Optional<SkColor>& skcolor) {
  if (!skcolor)
    return kInvalidJavaColor;
  return static_cast<int32_t>(*skcolor);
}

base::Optional<SkColor> JavaColorToOptionalSkColor(int64_t java_color) {
  if (java_color == kInvalidJavaColor)
    return base::nullopt;
  DCHECK(base::IsValueInRangeForNumericType<int32_t>(java_color));
  return static_cast<SkColor>(java_color);
}
