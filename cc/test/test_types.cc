// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_types.h"

#include "cc/base/region.h"
#include "cc/input/main_thread_scrolling_reason.h"

namespace cc {

namespace {

// Provides a test raster suffix appropriate for |type|.
const char* RasterTypeTestSuffix(TestRasterType type) {
  switch (type) {
    case TestRasterType::kBitmap:
      return "Bitmap";
    case TestRasterType::kGpu:
      return "GPU";
    case TestRasterType::kOneCopy:
      return "OneCopy";
    case TestRasterType::kZeroCopy:
      return "ZeroCopy";
  }
}

}  // namespace

void PrintTo(const RasterTestConfig& config, std::ostream* os) {
  PrintTo(config.renderer_type, os);
  *os << '_' << RasterTypeTestSuffix(config.raster_type);
}

void PrintTo(const Region& region, std::ostream* os) {
  *os << region.ToString();
}

void PrintTo(MainThreadRepaintReasons reasons, std::ostream* os) {
  *os << MainThreadScrollingReason::AsText(reasons);
}

void PrintTo(MainThreadHitTestReasons reasons, std::ostream* os) {
  *os << MainThreadScrollingReason::AsText(reasons);
}

void PrintTo(MainThreadScrollingOtherReasons reasons, std::ostream* os) {
  *os << MainThreadScrollingReason::AsText(reasons);
}

}  // namespace cc
