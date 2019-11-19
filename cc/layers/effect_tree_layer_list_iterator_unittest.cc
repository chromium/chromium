// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/effect_tree_layer_list_iterator.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<TestLayerImpl> Create(LayerTreeImpl* tree, int id) {
    return base::WrapUnique(new TestLayerImpl(tree, id));
  }
  ~TestLayerImpl() override = default;

  int count_;

 private:
  explicit TestLayerImpl(LayerTreeImpl* tree, int id)
      : LayerImpl(tree, id), count_(-1) {
    SetBounds(gfx::Size(100, 100));
    SetDrawsContent(true);
  }
};

#define EXPECT_COUNT(layer, target, contrib, itself)                      \
  if (GetRenderSurface(layer)) {                                          \
    EXPECT_EQ(target, target_surface_count_[layer->effect_tree_index()]); \
    EXPECT_EQ(contrib,                                                    \
              contributing_surface_count_[layer->effect_tree_index()]);   \
  }                                                                       \
  EXPECT_EQ(itself, layer->count_);

class EffectTreeLayerListIteratorTest : public LayerTreeImplTestBase,
                                        public testing::Test {
 public:
  void SetUp() override {
    // This test suite needs the root layer to be TestLayerImpl.
    LayerTreeImpl* active_tree = host_impl()->active_tree();
    active_tree->DetachLayers();
    active_tree->property_trees()->clear();
    active_tree->SetRootLayerForTesting(TestLayerImpl::Create(active_tree, 1));
    root_layer()->SetBounds(gfx::Size(1, 1));
    SetupRootProperties(root_layer());
  }

  void IterateFrontToBack() {
    ResetCounts();
    int count = 0;
    for (EffectTreeLayerListIterator it(host_impl()->active_tree());
         it.state() != EffectTreeLayerListIterator::State::END; ++it, ++count) {
      switch (it.state()) {
        case EffectTreeLayerListIterator::State::LAYER:
          static_cast<TestLayerImpl*>(it.current_layer())->count_ = count;
          break;
        case EffectTreeLayerListIterator::State::TARGET_SURFACE:
          target_surface_count_[it.target_render_surface()->EffectTreeIndex()] =
              count;
          break;
        case EffectTreeLayerListIterator::State::CONTRIBUTING_SURFACE:
          contributing_surface_count_[it.current_render_surface()
                                          ->EffectTreeIndex()] = count;
          break;
        default:
          NOTREACHED();
      }
    }
  }

  void ResetCounts() {
    for (LayerImpl* layer : *host_impl()->active_tree()) {
      static_cast<TestLayerImpl*>(layer)->count_ = -1;
    }

    target_surface_count_ = std::vector<int>(
        host_impl()->active_tree()->property_trees()->effect_tree.size(), -1);
    contributing_surface_count_ = std::vector<int>(
        host_impl()->active_tree()->property_trees()->effect_tree.size(), -1);
  }

 protected:
  // Tracks when each render surface is visited as a target surface or
  // contributing surface. Indexed by effect node id.
  std::vector<int> target_surface_count_;
  std::vector<int> contributing_surface_count_;
};

TEST_F(EffectTreeLayerListIteratorTest, TreeWithNoDrawnLayers) {
  auto* root = static_cast<TestLayerImpl*>(root_layer());
  root->SetDrawsContent(false);

  UpdateActiveTreeDrawProperties();

  IterateFrontToBack();
  EXPECT_COUNT(root, 0, -1, -1);
}

TEST_F(EffectTreeLayerListIteratorTest, SimpleTree) {
  auto* root = static_cast<TestLayerImpl*>(root_layer());
  auto* first = AddLayer<TestLayerImpl>();
  CopyProperties(root, first);
  auto* second = AddLayer<TestLayerImpl>();
  CopyProperties(root, second);
  auto* third = AddLayer<TestLayerImpl>();
  CopyProperties(root, third);
  auto* fourth = AddLayer<TestLayerImpl>();
  CopyProperties(root, fourth);

  UpdateActiveTreeDrawProperties();

  IterateFrontToBack();
  EXPECT_COUNT(root, 5, -1, 4);
  EXPECT_COUNT(first, 5, -1, 3);
  EXPECT_COUNT(second, 5, -1, 2);
  EXPECT_COUNT(third, 5, -1, 1);
  EXPECT_COUNT(fourth, 5, -1, 0);
}

TEST_F(EffectTreeLayerListIteratorTest, ComplexTreeMultiSurface) {
  auto* root = static_cast<TestLayerImpl*>(root_layer());
  auto* root1 = AddLayer<TestLayerImpl>();
  CopyProperties(root, root1);

  auto* root2 = AddLayer<TestLayerImpl>();
  root2->SetDrawsContent(false);
  CopyProperties(root, root2);
  CreateEffectNode(root2).render_surface_reason = RenderSurfaceReason::kTest;

  auto* root21 = AddLayer<TestLayerImpl>();
  CopyProperties(root2, root21);

  auto* root22 = AddLayer<TestLayerImpl>();
  CopyProperties(root2, root22);
  CreateEffectNode(root22).render_surface_reason = RenderSurfaceReason::kTest;
  auto* root221 = AddLayer<TestLayerImpl>();
  CopyProperties(root22, root221);

  auto* root23 = AddLayer<TestLayerImpl>();
  CopyProperties(root2, root23);
  CreateEffectNode(root23).render_surface_reason = RenderSurfaceReason::kTest;
  auto* root231 = AddLayer<TestLayerImpl>();
  CopyProperties(root23, root231);

  auto* root3 = AddLayer<TestLayerImpl>();
  CopyProperties(root, root3);

  UpdateActiveTreeDrawProperties();

  IterateFrontToBack();
  EXPECT_COUNT(root, 14, -1, 13);
  EXPECT_COUNT(root1, 14, -1, 12);
  EXPECT_COUNT(root2, 10, 11, -1);
  EXPECT_COUNT(root21, 10, 11, 9);
  EXPECT_COUNT(root22, 7, 8, 6);
  EXPECT_COUNT(root221, 7, 8, 5);
  EXPECT_COUNT(root23, 3, 4, 2);
  EXPECT_COUNT(root231, 3, 4, 1);
  EXPECT_COUNT(root3, 14, -1, 0);
}

}  // namespace
}  // namespace cc
