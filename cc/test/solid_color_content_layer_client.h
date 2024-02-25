// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SOLID_COLOR_CONTENT_LAYER_CLIENT_H_
#define CC_TEST_SOLID_COLOR_CONTENT_LAYER_CLIENT_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "cc/layers/content_layer_client.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class SolidColorContentLayerClient : public ContentLayerClient {
 public:
  explicit SolidColorContentLayerClient(SkColor color, gfx::Size size)
      : color_(color), size_(size), border_size_(0), border_color_(0) {}
  explicit SolidColorContentLayerClient(SkColor color,
                                        gfx::Size size,
                                        int border_size,
                                        SkColor border_color)
      : color_(color),
        size_(size),
        border_size_(border_size),
        border_color_(border_color) {}

  // ContentLayerClient implementation.
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList() override;
  bool FillsBoundsCompletely() const override;

 private:
  SkColor color_;
  gfx::Size size_;
  int border_size_;
  SkColor border_color_;
};

}  // namespace cc

#endif  // CC_TEST_SOLID_COLOR_CONTENT_LAYER_CLIENT_H_
