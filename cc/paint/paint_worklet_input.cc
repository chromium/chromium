// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_worklet_input.h"

namespace cc {

PaintWorkletInput::PropertyKey::PropertyKey(
    const std::string& custom_property_name,
    ElementId element_id)
    : custom_property_name(custom_property_name), element_id(element_id) {}

PaintWorkletInput::PropertyKey::PropertyKey(
    NativePropertyType native_property_type,
    ElementId element_id)
    : native_property_type(native_property_type), element_id(element_id) {}

PaintWorkletInput::PropertyKey::PropertyKey(const PropertyKey& other) = default;

PaintWorkletInput::PropertyKey::PropertyKey(PropertyKey&& other) = default;

PaintWorkletInput::PropertyKey& PaintWorkletInput::PropertyKey::operator=(
    const PropertyKey& other) = default;

PaintWorkletInput::PropertyKey& PaintWorkletInput::PropertyKey::operator=(
    PropertyKey&& other) = default;

PaintWorkletInput::PropertyKey::~PropertyKey() = default;

bool PaintWorkletInput::PropertyKey::operator==(
    const PropertyKey& other) const {
  return custom_property_name == other.custom_property_name &&
         native_property_type == other.native_property_type &&
         element_id == other.element_id;
}

bool PaintWorkletInput::PropertyKey::operator!=(
    const PropertyKey& other) const {
  return !(*this == other);
}

bool PaintWorkletInput::PropertyKey::operator<(const PropertyKey& other) const {
  if (custom_property_name.has_value() &&
      !other.custom_property_name.has_value())
    return true;
  if (!custom_property_name.has_value() &&
      other.custom_property_name.has_value())
    return false;
  if (custom_property_name.has_value() &&
      other.custom_property_name.has_value()) {
    if (custom_property_name.value() == other.custom_property_name.value())
      return element_id < other.element_id;
    return custom_property_name.value() < other.custom_property_name.value();
  }
  if (native_property_type.value() == other.native_property_type.value())
    return element_id < other.element_id;
  return native_property_type.value() < other.native_property_type.value();
}

PaintWorkletInput::PropertyValue::PropertyValue() = default;

PaintWorkletInput::PropertyValue::PropertyValue(float value)
    : float_value(value) {}

PaintWorkletInput::PropertyValue::PropertyValue(SkColor4f value)
    : color_value(value) {}

bool PaintWorkletInput::PropertyValue::has_value() const {
  DCHECK(float_value.has_value() != color_value.has_value() ||
         (!float_value.has_value() && !color_value.has_value()));
  return float_value.has_value() || color_value.has_value();
}

void PaintWorkletInput::PropertyValue::reset() {
  float_value.reset();
  color_value.reset();
}

bool PaintWorkletInput::NeedsLayer() const {
  return false;
}

bool PaintWorkletInput::ValueChangeShouldCauseRepaint(
    const PropertyValue& val1,
    const PropertyValue& val2) const {
  return val1.color_value != val2.color_value ||
         val1.float_value != val2.float_value;
}

bool PaintWorkletInput::IsPaintWorkletInput() const {
  return true;
}

}  // namespace cc
