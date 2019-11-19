// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_
#define CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_

#include "base/memory/ref_counted.h"
#include "cc/test/layer_tree_pixel_test.h"

namespace cc {

enum RasterType {
  SOFTWARE,
  GPU,
  ONE_COPY,
  ZERO_COPY,
};

struct PixelResourceTestCase {
  LayerTreeTest::RendererType renderer_type;
  RasterType raster_type;
};

class LayerTreeHostPixelResourceTest : public LayerTreePixelTest {
 public:
  explicit LayerTreeHostPixelResourceTest(PixelResourceTestCase test_case);
  LayerTreeHostPixelResourceTest();

  RendererType renderer_type() const { return test_case_.renderer_type; }

  RasterType raster_type() const { return test_case_.raster_type; }

  const char* GetRendererSuffix() const;

  void InitializeSettings(LayerTreeSettings* settings) override;

  std::unique_ptr<RasterBufferProvider> CreateRasterBufferProvider(
      LayerTreeHostImpl* host_impl) override;

  void RunPixelResourceTest(scoped_refptr<Layer> content_root,
                            base::FilePath file_name);
  void RunPixelResourceTest(scoped_refptr<Layer> content_root,
                            const SkBitmap& expected_bitmap);

  void RunPixelResourceTestWithLayerList(base::FilePath file_name);

 protected:
  PixelResourceTestCase test_case_;
  bool initialized_ = false;

  void InitializeFromTestCase(PixelResourceTestCase test_case);
};

class ParameterizedPixelResourceTest
    : public LayerTreeHostPixelResourceTest,
      public ::testing::WithParamInterface<PixelResourceTestCase> {
 public:
  ParameterizedPixelResourceTest();
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_
