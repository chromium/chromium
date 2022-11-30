// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_TYPES_H_
#define CC_TEST_TEST_TYPES_H_

#include <ostream>
#include <string>

#include "base/strings/string_util.h"
#include "components/viz/test/test_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

// Joins all elements of a testing::tuple using an underscore. Use as the fourth
// parameter of INSTANTIATE_TEST_SUITE_P() instead of
// testing::PrintToStringParamName() to generate a valid parameter label. Each
// element of the tuple must be printable, and each combination of tuple values
// must produce a unique string.
// Underscores shouldn't be used in test suite names due to a risk of name
// collision, but this doesn't apply to parameterization labels.
struct PrintTupleToStringParamName {
  template <class ParamType>
  std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
    return base::JoinString(
        testing::internal::UniversalTersePrintTupleFieldsToStrings(info.param),
        "_");
  }
};

enum class TestRasterType {
  kBitmap,
  kGpu,
  kOneCopy,
  kZeroCopy,
};

struct RasterTestConfig {
  viz::RendererType renderer_type;
  TestRasterType raster_type;
};

// Joins the |renderer_type| and |raster_type| labels using an underscore
// character, resulting in e.g. "Test/SkiaGL_OOP".
void PrintTo(const RasterTestConfig& config, std::ostream* os);

}  // namespace cc

#endif  // CC_TEST_TEST_TYPES_H_
