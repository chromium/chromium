// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_TYPES_H_
#define CC_TEST_TEST_TYPES_H_

#include <ostream>

namespace cc {

enum class TestRendererType {
  kGL,
  kSkiaGL,
  kSkiaVk,
  // SkiaRenderer with the Dawn backend will be used; on Linux this will
  // initialize Vulkan, and on Windows this will initialize D3D12.
  kSkiaDawn,
  kSoftware,
};

enum class TestRasterType {
  kBitmap,
  kGpu,
  kOop,
  kOneCopy,
  kZeroCopy,
};

struct RasterTestConfig {
  TestRendererType renderer_type;
  TestRasterType raster_type;
};

void PrintTo(TestRendererType type, std::ostream* os);

// Joins the |renderer_type| and |raster_type| labels using an underscore
// character, resulting in e.g. "Test/SkiaGL_OOP". Underscores shouldn't be used
// in test suite names due to a risk of name collision, but this doesn't apply
// to parameterization labels.
void PrintTo(const RasterTestConfig& config, std::ostream* os);

}  // namespace cc

#endif  // CC_TEST_TEST_TYPES_H_
