// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/ui_resource_layer.h"

#include "cc/animation/animation_host.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
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

class TestUIResourceLayer : public UIResourceLayer {
 public:
  static scoped_refptr<TestUIResourceLayer> Create() {
    return base::WrapRefCounted(new TestUIResourceLayer());
  }

  using UIResourceLayer::resource_id;
  using UIResourceLayer::HasDrawableContent;

 protected:
  TestUIResourceLayer() : UIResourceLayer() { SetIsDrawable(true); }
  ~TestUIResourceLayer() override = default;
};

class UIResourceLayerTest : public testing::Test {
 protected:
  void TearDown() override {
    Mock::VerifyAndClearExpectations(layer_tree_host());
  }

  FakeLayerTreeHost* layer_tree_host() { return layer_impl_test_.host(); }

 private:
  LayerTreeImplTestBase layer_impl_test_;
};

TEST_F(UIResourceLayerTest, SetBitmap) {
  scoped_refptr<UIResourceLayer> test_layer = TestUIResourceLayer::Create();
  ASSERT_TRUE(test_layer.get());
  test_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host()->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host());

  test_layer->Update();

  EXPECT_FALSE(test_layer->draws_content());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.setImmutable();

  test_layer->SetBitmap(bitmap);
  test_layer->Update();

  EXPECT_TRUE(test_layer->draws_content());
}

TEST_F(UIResourceLayerTest, SetUIResourceId) {
  scoped_refptr<TestUIResourceLayer> test_layer = TestUIResourceLayer::Create();
  ASSERT_TRUE(test_layer.get());
  test_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host()->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host());

  test_layer->Update();

  EXPECT_FALSE(test_layer->draws_content());

  bool is_opaque = false;
  std::unique_ptr<ScopedUIResource> resource =
      ScopedUIResource::Create(layer_tree_host()->GetUIResourceManager(),
                               UIResourceBitmap(gfx::Size(10, 10), is_opaque));
  test_layer->SetUIResourceId(resource->id());
  test_layer->Update();

  EXPECT_TRUE(test_layer->draws_content());

  // ID is preserved even when you set ID first and attach it to the tree.
  layer_tree_host()->SetRootLayer(nullptr);
  std::unique_ptr<ScopedUIResource> shared_resource =
      ScopedUIResource::Create(layer_tree_host()->GetUIResourceManager(),
                               UIResourceBitmap(gfx::Size(5, 5), is_opaque));
  test_layer->SetUIResourceId(shared_resource->id());
  layer_tree_host()->SetRootLayer(test_layer);
  EXPECT_EQ(shared_resource->id(), test_layer->resource_id());
  EXPECT_TRUE(test_layer->draws_content());
}

TEST_F(UIResourceLayerTest, BitmapClearedOnSetUIResourceId) {
  scoped_refptr<TestUIResourceLayer> test_layer = TestUIResourceLayer::Create();
  ASSERT_TRUE(test_layer.get());
  test_layer->SetBounds(gfx::Size(100, 100));
  EXPECT_FALSE(test_layer->HasDrawableContent());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.setImmutable();
  ASSERT_FALSE(bitmap.isNull());
  ASSERT_TRUE(bitmap.pixelRef()->unique());

  // Without a layer tree, the only additional reference is in UIResourceLayer.
  test_layer->SetBitmap(bitmap);
  ASSERT_FALSE(bitmap.pixelRef()->unique());
  // Also, there's no drawable content due to the lack of a LTH.
  EXPECT_FALSE(test_layer->HasDrawableContent());

  test_layer->SetUIResourceId(0);
  EXPECT_TRUE(bitmap.pixelRef()->unique());
  EXPECT_FALSE(test_layer->HasDrawableContent());

  // Add to a layer tree; now the UIResourceManager holds onto a ref
  // indefinitely.
  {
    LayerTreeImplTestBase impl;
    impl.host()->SetRootLayer(test_layer);

    test_layer->SetBitmap(bitmap);
    EXPECT_FALSE(bitmap.pixelRef()->unique());
    EXPECT_TRUE(test_layer->HasDrawableContent());
    test_layer->SetUIResourceId(0);
    EXPECT_FALSE(bitmap.pixelRef()->unique());
    EXPECT_FALSE(test_layer->HasDrawableContent());
  }

  // After the layer tree/resource manager are destroyed, refs are back to 1.
  test_layer->SetUIResourceId(0);
  EXPECT_TRUE(bitmap.pixelRef()->unique());
  EXPECT_FALSE(test_layer->HasDrawableContent());
}

TEST_F(UIResourceLayerTest, SharedBitmap) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.setImmutable();
  // The SkPixelRef is what's important, not the SkBitmap itself.
  SkBitmap bitmap_copy = bitmap;

  scoped_refptr<TestUIResourceLayer> layer1 = TestUIResourceLayer::Create();
  layer_tree_host()->SetRootLayer(layer1);
  layer1->SetBitmap(bitmap);
  bitmap.reset();
  layer1->Update();
  EXPECT_TRUE(layer1->draws_content());
  const auto resource_id = layer1->resource_id();

  // Second layer, same LTH. Resource is shared (has same ID).
  scoped_refptr<TestUIResourceLayer> layer2 = TestUIResourceLayer::Create();
  layer_tree_host()->SetRootLayer(layer2);
  layer2->SetBitmap(bitmap_copy);
  layer2->Update();
  EXPECT_TRUE(layer2->draws_content());
  EXPECT_EQ(resource_id, layer2->resource_id());

  // Change bitmap, different resource id.
  SkBitmap other_bitmap;
  other_bitmap.allocN32Pixels(12, 12);
  other_bitmap.setImmutable();
  layer2->SetBitmap(other_bitmap);
  EXPECT_NE(resource_id, layer2->resource_id());

  // Switch layer to different LTH. ID is in a new namespace (LTH), so it may
  // still be the same. We can make sure it's using the same shared bitmap by
  // verifying that whatever ID it has, it changes away from and back to when we
  // change the shared bitmap to something else then back to the original.
  LayerTreeImplTestBase impl;
  impl.host()->SetRootLayer(layer1);
  layer1->Update();
  EXPECT_TRUE(layer1->draws_content());
  const auto other_lth_resource_id = layer1->resource_id();
  layer1->SetBitmap(other_bitmap);
  EXPECT_NE(other_lth_resource_id, layer1->resource_id());
  layer1->SetBitmap(bitmap_copy);
  EXPECT_EQ(other_lth_resource_id, layer1->resource_id());
}

TEST_F(UIResourceLayerTest, SharedBitmapCacheSizeLimit) {
  scoped_refptr<TestUIResourceLayer> layer = TestUIResourceLayer::Create();
  layer_tree_host()->SetRootLayer(layer);

  // Number of bitmaps that are created then get their references dropped.
  constexpr size_t kDroppedResources = 100u;

  // Populate the shared bitmap cache.
  auto* manager = layer_tree_host()->GetUIResourceManager();
  while (manager->owned_shared_resources_size_for_test() < kDroppedResources) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    bitmap.setImmutable();
    layer->SetBitmap(bitmap);
  }

  // No eviction because bitmaps are references by UIResourcesRequests.
  EXPECT_EQ(manager->owned_shared_resources_size_for_test(), kDroppedResources);

  // Pretend UIResourcesRequests are processed to drop bitmap references.
  manager->TakeUIResourcesRequests();

  // Create one more shared bitmap resource and the eviction happens.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.setImmutable();
  layer->SetBitmap(bitmap);

  // The cache should trimmed down.
  EXPECT_EQ(manager->owned_shared_resources_size_for_test(), 1u);
}

}  // namespace
}  // namespace cc
