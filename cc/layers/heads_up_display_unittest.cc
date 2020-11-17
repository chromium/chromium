// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer.h"

#include "cc/layers/layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {
namespace {

class HeadsUpDisplayTest : public LayerTreeTest {
 protected:
  void InitializeSettings(LayerTreeSettings* settings) override {
    // Enable the HUD without requiring text.
    settings->initial_debug_state.show_property_changed_rects = true;
  }
};

class HudWithRootLayerChange : public HeadsUpDisplayTest {
 public:
  void BeginTest() override {
    root_layer1_ = Layer::Create();
    root_layer2_ = Layer::Create();
    root_layer1_->SetBounds(gfx::Size(30, 30));
    root_layer2_->SetBounds(gfx::Size(30, 30));

    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
    ++num_commits_;

    ASSERT_TRUE(layer_tree_host()->hud_layer());

    switch (num_commits_) {
      case 1:
        // Change directly to a new root layer.
        layer_tree_host()->SetRootLayer(root_layer1_);
        break;
      case 2:
        EXPECT_EQ(root_layer1_.get(), layer_tree_host()->hud_layer()->parent());
        // Unset the root layer.
        layer_tree_host()->SetRootLayer(nullptr);
        break;
      case 3:
        EXPECT_EQ(nullptr, layer_tree_host()->hud_layer()->parent());
        // Change back to the previous root layer.
        layer_tree_host()->SetRootLayer(root_layer1_);
        break;
      case 4:
        EXPECT_EQ(root_layer1_.get(), layer_tree_host()->hud_layer()->parent());
        // Unset the root layer.
        layer_tree_host()->SetRootLayer(nullptr);
        break;
      case 5:
        EXPECT_EQ(nullptr, layer_tree_host()->hud_layer()->parent());
        // Change to a new root layer from a null root.
        layer_tree_host()->SetRootLayer(root_layer2_);
        break;
      case 6:
        EXPECT_EQ(root_layer2_.get(), layer_tree_host()->hud_layer()->parent());
        // Change directly back to the last root layer.
        layer_tree_host()->SetRootLayer(root_layer1_);
        break;
      case 7:
        EXPECT_EQ(root_layer1_.get(), layer_tree_host()->hud_layer()->parent());
        EndTest();
        break;
    }
  }

 private:
  scoped_refptr<Layer> root_layer1_;
  scoped_refptr<Layer> root_layer2_;
  int num_commits_ = 0;
};

SINGLE_AND_MULTI_THREAD_TEST_F(HudWithRootLayerChange);

class HeadsUpDisplaySizeWithFPS : public LayerTreeTest {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->initial_debug_state.show_fps_counter = true;
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    ASSERT_TRUE(layer_tree_host()->hud_layer());
    EXPECT_EQ(gfx::Size(256, 256), layer_tree_host()->hud_layer()->bounds());
    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(HeadsUpDisplaySizeWithFPS);

}  // namespace
}  // namespace cc
