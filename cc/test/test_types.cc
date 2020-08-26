// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_types.h"

namespace cc {

namespace {

// Provides a test renderer suffix appropriate for |type|.
const char* RendererTypeTestSuffix(TestRendererType type) {
  switch (type) {
    case TestRendererType::kGL:
      return "GL";
    case TestRendererType::kSkiaGL:
      return "SkiaGL";
    case TestRendererType::kSkiaVk:
      return "SkiaVulkan";
    case TestRendererType::kSkiaDawn:
      return "SkiaDawn";
    case TestRendererType::kSoftware:
      return "Software";
  }
}

// Provides a test raster suffix appropriate for |type|.
const char* RasterTypeTestSuffix(TestRasterType type) {
  switch (type) {
    case TestRasterType::kBitmap:
      return "Bitmap";
    case TestRasterType::kGpu:
      return "GPU";
    case TestRasterType::kOop:
      return "OOP";
    case TestRasterType::kOneCopy:
      return "OneCopy";
    case TestRasterType::kZeroCopy:
      return "ZeroCopy";
  }
}

}  // namespace

void PrintTo(TestRendererType type, std::ostream* os) {
  *os << RendererTypeTestSuffix(type);
}

void PrintTo(const RasterTestConfig& config, std::ostream* os) {
  PrintTo(config.renderer_type, os);
  *os << '_' << RasterTypeTestSuffix(config.raster_type);
}

}  // namespace cc
