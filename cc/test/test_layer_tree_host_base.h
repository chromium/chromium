// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_LAYER_TREE_HOST_BASE_H_
#define CC_TEST_TEST_LAYER_TREE_HOST_BASE_H_

#include <memory>

#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/tiles/tile_priority.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class TestLayerTreeHostBase : public testing::Test {
 protected:
  TestLayerTreeHostBase();
  ~TestLayerTreeHostBase() override;

  void SetUp() override;

  virtual LayerTreeSettings CreateSettings();
  virtual std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink();
  virtual std::unique_ptr<FakeLayerTreeHostImpl> CreateHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner);
  virtual std::unique_ptr<TaskGraphRunner> CreateTaskGraphRunner();
  virtual void InitializeFrameSink();

  void ResetLayerTreeFrameSink(
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink);
  std::unique_ptr<FakeLayerTreeHostImpl> TakeHostImpl();

  void SetupDefaultTrees(const gfx::Size& layer_bounds);
  void SetupTrees(scoped_refptr<RasterSource> pending_raster_source,
                  scoped_refptr<RasterSource> active_raster_source);
  void SetupPendingTree(scoped_refptr<RasterSource> raster_source = nullptr);
  void SetupPendingTree(scoped_refptr<RasterSource> raster_source,
                        const gfx::Size& tile_size,
                        const Region& invalidation);
  void ActivateTree();
  void PerformImplSideInvalidation();

  FakeLayerTreeHostImpl* host_impl() const { return host_impl_.get(); }
  TaskGraphRunner* task_graph_runner() const {
    return task_graph_runner_.get();
  }
  LayerTreeFrameSink* layer_tree_frame_sink() const {
    return layer_tree_frame_sink_.get();
  }
  FakePictureLayerImpl* pending_layer() const { return pending_layer_; }
  FakePictureLayerImpl* active_layer() const { return active_layer_; }
  FakePictureLayerImpl* old_pending_layer() const { return old_pending_layer_; }

  // Clear all layer trees and property trees for repeated testing.
  void ResetTrees();

  template <typename T, typename... Args>
  T* AddLayer(LayerTreeImpl* layer_tree_impl, Args&&... args) {
    std::unique_ptr<T> layer = T::Create(layer_tree_impl, next_layer_id_++,
                                         std::forward<Args>(args)...);
    T* result = layer.get();
    layer_tree_impl->AddLayer(std::move(layer));
    return result;
  }

  int root_id() const { return root_id_; }

 private:
  void SetInitialTreePriority();

  FakeImplTaskRunnerProvider task_runner_provider_;
  std::unique_ptr<TaskGraphRunner> task_graph_runner_;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink_;
  std::unique_ptr<FakeLayerTreeHostImpl> host_impl_;

  FakePictureLayerImpl* pending_layer_;
  FakePictureLayerImpl* active_layer_;
  FakePictureLayerImpl* old_pending_layer_;
  const int root_id_;
  int next_layer_id_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_LAYER_TREE_HOST_BASE_H_
