// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_
#define CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "cc/test/layer_tree_pixel_test.h"

namespace cc {

class LayerTreeHostPixelResourceTest : public LayerTreePixelTest {
 public:
  explicit LayerTreeHostPixelResourceTest(RasterTestConfig test_config);

  viz::RendererType renderer_type() const { return test_config_.renderer_type; }

  const char* GetRendererSuffix() const;

  std::unique_ptr<RasterBufferProvider> CreateRasterBufferProvider(
      LayerTreeHostImpl* host_impl) override;

  void RunPixelResourceTest(scoped_refptr<Layer> content_root,
                            base::FilePath file_name);
  void RunPixelResourceTest(scoped_refptr<Layer> content_root,
                            const SkBitmap& expected_bitmap);

  void RunPixelResourceTestWithLayerList(base::FilePath file_name);

 protected:
  const RasterTestConfig test_config_;
};

class ParameterizedPixelResourceTest
    : public LayerTreeHostPixelResourceTest,
      public ::testing::WithParamInterface<RasterTestConfig> {
 public:
  ParameterizedPixelResourceTest();
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_
