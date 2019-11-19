// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "cc/animation/animation_host.h"
#include "cc/layers/mirror_layer.h"
#include "cc/layers/mirror_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/tree_synchronizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MirrorLayerTest : public testing::Test {
 public:
  MirrorLayerTest() : host_impl_(&task_runner_provider_, &task_graph_runner_) {}

  // Synchronizes |layer_tree_host_| and |host_impl_| and pushes surface ids.
  void SynchronizeTrees() {
    TreeSynchronizer::PushLayerProperties(layer_tree_host_.get(),
                                          host_impl_.pending_tree());
  }

 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
    layer_tree_host_->SetViewportRectAndScale(gfx::Rect(10, 10), 1.f,
                                              viz::LocalSurfaceIdAllocation());
    host_impl_.CreatePendingTree();
  }

  void TearDown() override {
    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
  }

  FakeLayerTreeHostClient fake_client_;
  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  FakeLayerTreeHostImpl host_impl_;
};

// This test verifies that MirrorLayer properties are pushed across to
// MirrorLayerImpl.
TEST_F(MirrorLayerTest, PushProperties) {
  auto root = Layer::Create();
  layer_tree_host_->SetRootLayer(root);

  auto mirrored = Layer::Create();
  root->AddChild(mirrored);
  auto mirror = MirrorLayer::Create(mirrored);
  root->AddChild(mirror);

  EXPECT_EQ(1, mirrored->mirror_count());
  EXPECT_EQ(mirrored.get(), mirror->mirrored_layer());

  auto root_impl = LayerImpl::Create(host_impl_.pending_tree(), root->id());
  auto mirrored_impl =
      LayerImpl::Create(host_impl_.pending_tree(), mirrored->id());
  auto mirror_impl =
      MirrorLayerImpl::Create(host_impl_.pending_tree(), mirror->id());

  // Verify that impl layers have default property values.
  EXPECT_EQ(0, mirrored_impl->mirror_count());
  EXPECT_EQ(0, mirror_impl->mirrored_layer_id());

  SynchronizeTrees();

  // Verify that property values are pushed to impl layers.
  EXPECT_EQ(1, mirrored_impl->mirror_count());
  EXPECT_EQ(mirrored_impl->id(), mirror_impl->mirrored_layer_id());
}

// This test verifies adding/removing mirror layers updates mirror count
// properly and sets appropriate bits on the layer tree host.
TEST_F(MirrorLayerTest, MirrorCount) {
  auto mirrored = Layer::Create();
  mirrored->SetLayerTreeHost(layer_tree_host_.get());

  layer_tree_host_->property_trees()->needs_rebuild = false;
  layer_tree_host_->ClearLayersThatShouldPushProperties();
  EXPECT_EQ(0, mirrored->mirror_count());

  // Creating the first mirror layer should trigger property trees rebuild.
  auto mirror1 = MirrorLayer::Create(mirrored);
  EXPECT_EQ(1, mirrored->mirror_count());
  EXPECT_EQ(mirrored.get(), mirror1->mirrored_layer());
  EXPECT_TRUE(layer_tree_host_->property_trees()->needs_rebuild);
  EXPECT_TRUE(base::Contains(layer_tree_host_->LayersThatShouldPushProperties(),
                             mirrored.get()));

  layer_tree_host_->property_trees()->needs_rebuild = false;
  layer_tree_host_->ClearLayersThatShouldPushProperties();

  // Creating a second mirror layer should not trigger property trees rebuild.
  auto mirror2 = MirrorLayer::Create(mirrored);
  EXPECT_EQ(2, mirrored->mirror_count());
  EXPECT_EQ(mirrored.get(), mirror2->mirrored_layer());
  EXPECT_FALSE(layer_tree_host_->property_trees()->needs_rebuild);
  EXPECT_TRUE(base::Contains(layer_tree_host_->LayersThatShouldPushProperties(),
                             mirrored.get()));

  layer_tree_host_->property_trees()->needs_rebuild = false;
  layer_tree_host_->ClearLayersThatShouldPushProperties();

  // Destroying one of the mirror layers should not trigger property trees
  // rebuild.
  mirror1->RemoveFromParent();
  mirror1 = nullptr;
  EXPECT_EQ(1, mirrored->mirror_count());
  EXPECT_FALSE(layer_tree_host_->property_trees()->needs_rebuild);
  EXPECT_EQ(1u, layer_tree_host_->LayersThatShouldPushProperties().size());

  layer_tree_host_->property_trees()->needs_rebuild = false;
  layer_tree_host_->ClearLayersThatShouldPushProperties();

  // Destroying the only remaining mirror layer should trigger property trees
  // rebuild.
  mirror2->RemoveFromParent();
  mirror2 = nullptr;
  EXPECT_EQ(0, mirrored->mirror_count());
  EXPECT_TRUE(layer_tree_host_->property_trees()->needs_rebuild);
  EXPECT_TRUE(base::Contains(layer_tree_host_->LayersThatShouldPushProperties(),
                             mirrored.get()));

  mirrored->SetLayerTreeHost(nullptr);
}

}  // namespace
}  // namespace cc
