// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_worklet_input.h"

#include <utility>

namespace cc {

PaintWorkletInput::PropertyValue::PropertyValue() = default;

PaintWorkletInput::PropertyValue::PropertyValue(float value)
    : float_value(value) {}

PaintWorkletInput::PropertyValue::PropertyValue(SkColor value)
    : color_value(value) {}

PaintWorkletInput::PropertyValue::PropertyValue(const PropertyValue& other) =
    default;

PaintWorkletInput::PropertyValue::~PropertyValue() = default;

bool PaintWorkletInput::PropertyValue::has_value() const {
  DCHECK(float_value.has_value() != color_value.has_value() ||
         (!float_value.has_value() && !color_value.has_value()));
  return float_value.has_value() || color_value.has_value();
}

void PaintWorkletInput::PropertyValue::reset() {
  float_value.reset();
  color_value.reset();
}

}  // namespace cc
