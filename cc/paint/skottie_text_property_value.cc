// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_text_property_value.h"

#include <utility>

#include "base/hash/hash.h"

namespace cc {

SkottieTextPropertyValue::SkottieTextPropertyValue(std::string text,
                                                   gfx::RectF box)
    : box_(std::move(box)) {
  SetText(std::move(text));
}

SkottieTextPropertyValue::SkottieTextPropertyValue(
    const SkottieTextPropertyValue& other) = default;

SkottieTextPropertyValue& SkottieTextPropertyValue::operator=(
    const SkottieTextPropertyValue& other) = default;

SkottieTextPropertyValue::~SkottieTextPropertyValue() = default;

bool SkottieTextPropertyValue::operator==(
    const SkottieTextPropertyValue& other) const {
  return text_hash_ == other.text_hash_ && box_ == other.box_;
}

bool SkottieTextPropertyValue::operator!=(
    const SkottieTextPropertyValue& other) const {
  return !(*this == other);
}

void SkottieTextPropertyValue::SetText(std::string text) {
  size_t incoming_text_hash = base::FastHash(text);
  if (incoming_text_hash == text_hash_)
    return;
  text_hash_ = incoming_text_hash;
  text_ = base::MakeRefCounted<base::RefCountedString>(std::move(text));
}

}  // namespace cc
