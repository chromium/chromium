// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/scoped_gpu_raster.h"

#include <memory>

#include "components/viz/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class ScopedGpuRasterTest : public testing::Test {
 public:
  ScopedGpuRasterTest() = default;
};

// Releasing ScopedGpuRaster should restore GL_UNPACK_ALIGNMENT == 4.
TEST(ScopedGpuRasterTest, RestoresUnpackAlignment) {
  scoped_refptr<viz::TestContextProvider> provider =
      viz::TestContextProvider::Create();
  ASSERT_EQ(provider->BindToCurrentSequence(), gpu::ContextResult::kSuccess);
  gpu::gles2::GLES2Interface* gl = provider->ContextGL();
  GLint unpack_alignment = 0;
  gl->GetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
  EXPECT_EQ(4, unpack_alignment);

  {
    std::unique_ptr<ScopedGpuRaster> scoped_gpu_raster(
        new ScopedGpuRaster(provider.get()));
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->GetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
    EXPECT_EQ(1, unpack_alignment);
  }

  gl->GetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
  EXPECT_EQ(4, unpack_alignment);
}

}  // namespace
}  // namespace cc
