// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_PAINT_WORKLET_INPUT_H_
#define CC_TEST_TEST_PAINT_WORKLET_INPUT_H_

#include <vector>

#include "cc/paint/paint_worklet_input.h"

namespace cc {

class TestPaintWorkletInput : public PaintWorkletInput {
 public:
  explicit TestPaintWorkletInput(const gfx::SizeF& size);
  explicit TestPaintWorkletInput(const PaintWorkletInput::PropertyKey& key,
                                 const gfx::SizeF& size);

  gfx::SizeF GetSize() const override;
  int WorkletId() const override;
  const std::vector<PaintWorkletInput::PropertyKey>& GetPropertyKeys()
      const override;

  bool NeedsLayer() const override;
  bool IsCSSPaintWorkletInput() const override;

 protected:
  ~TestPaintWorkletInput() override;

 private:
  gfx::SizeF container_size_;
  std::vector<PaintWorkletInput::PropertyKey> property_keys_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_PAINT_WORKLET_INPUT_H_
