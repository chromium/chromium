// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer.h"

#include "cc/animation/animation_host.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace cc {
namespace {

class NinePatchLayerTest : public testing::Test {
 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::kMain);
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  }

  FakeLayerTreeHostClient fake_client_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
};

TEST_F(NinePatchLayerTest, SetLayerProperties) {
  scoped_refptr<NinePatchLayer> test_layer = NinePatchLayer::Create();
  ASSERT_TRUE(test_layer.get());
  test_layer->SetIsDrawable(true);
  test_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  test_layer->Update();

  EXPECT_FALSE(test_layer->draws_content());

  bool is_opaque = false;
  std::unique_ptr<ScopedUIResource> resource =
      ScopedUIResource::Create(layer_tree_host_->GetUIResourceManager(),
                               UIResourceBitmap(gfx::Size(10, 10), is_opaque));
  gfx::Rect aperture(5, 5, 1, 1);
  bool fill_center = true;
  test_layer->SetAperture(aperture);
  test_layer->SetUIResourceId(resource->id());
  test_layer->SetFillCenter(fill_center);
  test_layer->Update();

  EXPECT_TRUE(test_layer->draws_content());
}

}  // namespace
}  // namespace cc
