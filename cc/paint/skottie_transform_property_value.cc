// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_transform_property_value.h"

namespace cc {

bool SkottieTransformPropertyValue::operator==(
    const SkottieTransformPropertyValue& other) const {
  return position == other.position;
}

bool SkottieTransformPropertyValue::operator!=(
    const SkottieTransformPropertyValue& other) const {
  return !(*this == other);
}

}  // namespace cc
