// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_paint_worklet_input.h"

namespace cc {

TestPaintWorkletInput::TestPaintWorkletInput(const gfx::SizeF& size)
    : container_size_(size) {}

TestPaintWorkletInput::~TestPaintWorkletInput() = default;

gfx::SizeF TestPaintWorkletInput::GetSize() const {
  return container_size_;
}

int TestPaintWorkletInput::WorkletId() const {
  return 1u;
}

const std::vector<PaintWorkletInput::PropertyKey>&
TestPaintWorkletInput::GetPropertyKeys() const {
  return property_keys_;
}

}  // namespace cc
