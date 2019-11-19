// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_IMPL_TEST_BASE_H_
#define CC_TEST_LAYER_TREE_IMPL_TEST_BASE_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "cc/animation/animation_timeline.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/quads/render_pass.h"

namespace cc {

class LayerImpl;
class LayerTreeFrameSink;
class RenderSurfaceImpl;

class LayerTreeImplTestBase {
 public:
  LayerTreeImplTestBase();
  explicit LayerTreeImplTestBase(
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink);
  explicit LayerTreeImplTestBase(const LayerTreeSettings& settings);
  LayerTreeImplTestBase(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink);
  ~LayerTreeImplTestBase();

  template <typename T, typename... Args>
  T* AddLayer(Args&&... args) {
    return AddLayerInternal<T>(host_impl()->active_tree(),
                               std::forward<Args>(args)...);
  }

  LayerImpl* EnsureRootLayerInPendingTree();

  template <typename T, typename... Args>
  T* AddLayerInPendingTree(Args&&... args) {
    return AddLayerInternal<T>(host_impl()->pending_tree(),
                               std::forward<Args>(args)...);
  }

  void CalcDrawProps(const gfx::Size& viewport_size);
  void AppendQuadsWithOcclusion(LayerImpl* layer_impl,
                                const gfx::Rect& occluded);
  void AppendQuadsForPassWithOcclusion(LayerImpl* layer_impl,
                                       viz::RenderPass* given_render_pass,
                                       const gfx::Rect& occluded);
  void AppendSurfaceQuadsWithOcclusion(RenderSurfaceImpl* surface_impl,
                                       const gfx::Rect& occluded);

  LayerTreeFrameSink* layer_tree_frame_sink() const {
    return host_impl()->layer_tree_frame_sink();
  }
  viz::ClientResourceProvider* resource_provider() const {
    return host_impl()->resource_provider();
  }
  LayerImpl* root_layer() const {
    return host_impl()->active_tree()->root_layer();
  }
  FakeLayerTreeHost* host() { return host_.get(); }
  FakeLayerTreeHostImpl* host_impl() const { return host_->host_impl(); }
  TaskRunnerProvider* task_runner_provider() const {
    return host_impl()->task_runner_provider();
  }
  const viz::QuadList& quad_list() const { return render_pass_->quad_list; }
  scoped_refptr<AnimationTimeline> timeline() { return timeline_; }
  scoped_refptr<AnimationTimeline> timeline_impl() { return timeline_impl_; }

  void SetElementIdsForTesting() {
    host_impl()->active_tree()->SetElementIdsForTesting();
  }

  LayerImpl* InnerViewportScrollLayer() {
    return host_impl()->active_tree()->InnerViewportScrollLayerForTesting();
  }
  LayerImpl* OuterViewportScrollLayer() {
    return host_impl()->active_tree()->OuterViewportScrollLayerForTesting();
  }

  // These functions sets device scale factor and update device viewport rect
  // before calling the global UpdateDrawProperties() with
  // update_layer_impl_list_.
  void UpdateActiveTreeDrawProperties(float device_scale_factor = 1.0f);
  void UpdatePendingTreeDrawProperties(float device_scale_factor = 1.0f);

  bool UpdateLayerImplListContains(int id) const {
    for (const auto* layer : update_layer_impl_list_) {
      if (layer->id() == id)
        return true;
    }
    return false;
  }

  const LayerImplList& update_layer_impl_list() const {
    return update_layer_impl_list_;
  }

 private:
  template <typename T, typename... Args>
  T* AddLayerInternal(LayerTreeImpl* tree, Args&&... args) {
    std::unique_ptr<T> layer =
        T::Create(tree, layer_impl_id_++, std::forward<Args>(args)...);
    T* ptr = layer.get();
    tree->AddLayer(std::move(layer));
    return ptr;
  }

  FakeLayerTreeHostClient client_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> host_;
  std::unique_ptr<viz::RenderPass> render_pass_;
  scoped_refptr<AnimationTimeline> timeline_;
  scoped_refptr<AnimationTimeline> timeline_impl_;
  int layer_impl_id_;
  LayerImplList update_layer_impl_list_;
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_IMPL_TEST_BASE_H_
