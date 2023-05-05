// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/layer_tree_cc_wrapper.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "cc/animation/animation_host.h"
#include "cc/base/switches.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/slim/frame_sink.h"
#include "cc/slim/frame_sink_cc_wrapper.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree_client.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/swap_promise.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"

namespace cc::slim {

namespace {

class TopControlsSwapPromise : public cc::SwapPromise {
 public:
  explicit TopControlsSwapPromise(float height) : height_(height) {}
  ~TopControlsSwapPromise() override = default;

  // cc::SwapPromise:
  void DidActivate() override {}
  void WillSwap(viz::CompositorFrameMetadata* metadata) override {
    DCHECK_GT(metadata->frame_token, 0u);
    metadata->top_controls_visible_height.emplace(height_);
  }
  void DidSwap() override {}
  cc::SwapPromise::DidNotSwapAction DidNotSwap(DidNotSwapReason reason,
                                               base::TimeTicks) override {
    return DidNotSwapAction::KEEP_ACTIVE;
  }
  int64_t GetTraceId() const override { return 0; }

 private:
  const float height_;
};

class LayerTreeCcWrapperScopedKeepSurfaceAlive
    : public LayerTree::ScopedKeepSurfaceAlive {
 public:
  LayerTreeCcWrapperScopedKeepSurfaceAlive(
      base::WeakPtr<LayerTreeCcWrapper> layer_tree,
      const viz::SurfaceId& surface_id)
      : layer_tree_(std::move(layer_tree)), range_(surface_id, surface_id) {
    layer_tree_->AddSurfaceRange(range_);
  }

  ~LayerTreeCcWrapperScopedKeepSurfaceAlive() override {
    if (layer_tree_) {
      layer_tree_->RemoveSurfaceRange(range_);
    }
  }

 private:
  const base::WeakPtr<LayerTreeCcWrapper> layer_tree_;
  const viz::SurfaceRange range_;
};

}  // namespace

LayerTreeCcWrapper::LayerTreeCcWrapper(InitParams init_params)
    : client_(init_params.client) {
  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeSettings settings;
  settings.use_zero_copy = true;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));
  settings.initial_debug_state.show_fps_counter =
      command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
  if (command_line->HasSwitch(cc::switches::kUIShowCompositedLayerBorders)) {
    settings.initial_debug_state.show_debug_borders.set();
  }
  settings.single_thread_proxy_scheduler = true;
  settings.use_painted_device_scale_factor = true;

  cc::LayerTreeHost::InitParams cc_init_params;
  cc_init_params.client = this;
  cc_init_params.task_graph_runner = init_params.cc_task_graph_runner;
  cc_init_params.main_task_runner = std::move(init_params.task_runner);
  cc_init_params.settings = &settings;
  cc_init_params.mutator_host = animation_host_.get();

  host_ =
      cc::LayerTreeHost::CreateSingleThreaded(this, std::move(cc_init_params));
}

LayerTreeCcWrapper::~LayerTreeCcWrapper() {
  SetRoot(nullptr);
}

cc::UIResourceManager* LayerTreeCcWrapper::GetUIResourceManager() {
  return host_->GetUIResourceManager();
}

void LayerTreeCcWrapper::SetViewportRectAndScale(
    const gfx::Rect& device_viewport_rect,
    float device_scale_factor,
    const viz::LocalSurfaceId& local_surface_id) {
  host_->SetViewportRectAndScale(device_viewport_rect, device_scale_factor,
                                 local_surface_id);
}

void LayerTreeCcWrapper::set_background_color(SkColor4f color) {
  host_->set_background_color(color);
}

void LayerTreeCcWrapper::SetVisible(bool visible) {
  host_->SetVisible(visible);
}

bool LayerTreeCcWrapper::IsVisible() const {
  return host_->IsVisible();
}

void LayerTreeCcWrapper::RequestPresentationTimeForNextFrame(
    PresentationCallback callback) {
  host_->RequestPresentationTimeForNextFrame(std::move(callback));
}

void LayerTreeCcWrapper::RequestSuccessfulPresentationTimeForNextFrame(
    SuccessfulCallback callback) {
  host_->RequestSuccessfulPresentationTimeForNextFrame(std::move(callback));
}

void LayerTreeCcWrapper::set_display_transform_hint(
    gfx::OverlayTransform hint) {
  host_->set_display_transform_hint(hint);
}

void LayerTreeCcWrapper::RequestCopyOfOutput(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  if (host_->root_layer()) {
    host_->root_layer()->RequestCopyOfOutput(std::move(request));
  }
}

base::OnceClosure LayerTreeCcWrapper::DeferBeginFrame() {
  return base::DoNothingWithBoundArgs(host_->DeferMainFrameUpdate());
}

void LayerTreeCcWrapper::UpdateTopControlsVisibleHeight(float height) {
  auto swap_promise = std::make_unique<TopControlsSwapPromise>(height);
  host_->QueueSwapPromise(std::move(swap_promise));
}

void LayerTreeCcWrapper::SetNeedsAnimate() {
  host_->SetNeedsAnimate();
}

void LayerTreeCcWrapper::SetNeedsRedraw() {
  host_->SetNeedsRedrawRect(host_->device_viewport_rect());
}

const scoped_refptr<Layer>& LayerTreeCcWrapper::root() const {
  return root_;
}

void LayerTreeCcWrapper::SetRoot(scoped_refptr<Layer> root) {
  if (root_) {
    root_->SetLayerTree(nullptr);
  }
  root_ = std::move(root);
  if (root_) {
    root_->SetLayerTree(this);
    DCHECK(root_->cc_layer_);
    host_->SetRootLayer(root_->cc_layer_);
  } else {
    host_->SetRootLayer(nullptr);
  }
}

void LayerTreeCcWrapper::SetFrameSink(std::unique_ptr<FrameSink> sink) {
  host_->SetLayerTreeFrameSink(
      std::move(static_cast<FrameSinkCcWrapper*>(sink.get())->cc_frame_sink_));
}

void LayerTreeCcWrapper::ReleaseLayerTreeFrameSink() {
  host_->ReleaseLayerTreeFrameSink();
}

std::unique_ptr<LayerTree::ScopedKeepSurfaceAlive>
LayerTreeCcWrapper::CreateScopedKeepSurfaceAlive(
    const viz::SurfaceId& surface_id) {
  return std::make_unique<LayerTreeCcWrapperScopedKeepSurfaceAlive>(
      weak_factory_.GetWeakPtr(), surface_id);
}

const LayerTree::SurfaceRangesAndCounts&
LayerTreeCcWrapper::GetSurfaceRangesForTesting() const {
  const auto* const_host = host_.get();
  return const_host->pending_commit_state()->surface_ranges;
}

void LayerTreeCcWrapper::BeginMainFrame(const viz::BeginFrameArgs& args) {
  client_->BeginFrame(args);
}

void LayerTreeCcWrapper::RequestNewLayerTreeFrameSink() {
  client_->RequestNewFrameSink();
}

void LayerTreeCcWrapper::DidInitializeLayerTreeFrameSink() {
  client_->DidInitializeLayerTreeFrameSink();
}

void LayerTreeCcWrapper::DidFailToInitializeLayerTreeFrameSink() {
  client_->DidFailToInitializeLayerTreeFrameSink();
}

void LayerTreeCcWrapper::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAck();
}

void LayerTreeCcWrapper::DidSubmitCompositorFrame() {
  client_->DidSubmitCompositorFrame();
}

void LayerTreeCcWrapper::DidLoseLayerTreeFrameSink() {
  client_->DidLoseLayerTreeFrameSink();
}

void LayerTreeCcWrapper::AddSurfaceRange(
    const viz::SurfaceRange& surface_range) {
  host_->AddSurfaceRange(surface_range);
}

void LayerTreeCcWrapper::RemoveSurfaceRange(
    const viz::SurfaceRange& surface_range) {
  host_->RemoveSurfaceRange(surface_range);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
LayerTreeCcWrapper::GetBeginMainFrameMetrics() {
  return nullptr;
}

std::unique_ptr<cc::WebVitalMetrics> LayerTreeCcWrapper::GetWebVitalMetrics() {
  return nullptr;
}

}  // namespace cc::slim
