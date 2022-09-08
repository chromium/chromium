// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_TRANSFORM_PROPERTY_VALUE_H_
#define CC_PAINT_SKOTTIE_TRANSFORM_PROPERTY_VALUE_H_

#include "base/containers/flat_map.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc {

// Contains a subset of the fields in skottie::TransformPropertyValue that the
// caller may be interested in. Other fields may be added to this class as
// the need arises.
struct CC_PAINT_EXPORT SkottieTransformPropertyValue {
  bool operator==(const SkottieTransformPropertyValue& other) const;
  bool operator!=(const SkottieTransformPropertyValue& other) const;

  gfx::PointF position;
};

// Node name in the Lottie file (hashed) to corresponding
// SkottieTransformPropertyValue.
using SkottieTransformPropertyValueMap =
    base::flat_map<SkottieResourceIdHash, SkottieTransformPropertyValue>;

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_TRANSFORM_PROPERTY_VALUE_H_
