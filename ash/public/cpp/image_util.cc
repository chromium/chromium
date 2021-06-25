// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/image_util.h"

#include <memory>

#include "ui/gfx/image/canvas_image_source.h"

namespace ash {
namespace image_util {
namespace {

// EmptyImageSkiaSource --------------------------------------------------------

// An `gfx::ImageSkiaSource` which draws nothing to its `canvas`.
class EmptyImageSkiaSource : public gfx::CanvasImageSource {
 public:
  explicit EmptyImageSkiaSource(const gfx::Size& size)
      : gfx::CanvasImageSource(size) {}

  EmptyImageSkiaSource(const EmptyImageSkiaSource&) = delete;
  EmptyImageSkiaSource& operator=(const EmptyImageSkiaSource&) = delete;
  ~EmptyImageSkiaSource() override = default;

 private:
  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {}  // Draw nothing.
};

}  // namespace

// Utilities -------------------------------------------------------------------

gfx::ImageSkia CreateEmptyImage(const gfx::Size& size) {
  return gfx::ImageSkia(std::make_unique<EmptyImageSkiaSource>(size), size);
}

}  // namespace image_util
}  // namespace ash
