// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_LAYER_TREE_IMPL_H_
#define CC_SLIM_LAYER_TREE_IMPL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/slim/damage_data.h"
#include "cc/slim/frame_sink_impl_client.h"
#include "cc/slim/layer_tree.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace cc {
class UIResourceManager;
}  // namespace cc

namespace viz {
class ClientResourceProvider;
class CompositorRenderPass;
}  // namespace viz

namespace cc::slim {

class FrameSinkImpl;
class TestLayerTreeImpl;
class SurfaceLayer;
struct FrameData;

// Slim implementation of LayerTree.
class COMPONENT_EXPORT(CC_SLIM) LayerTreeImpl : public LayerTree,
                                                public FrameSinkImplClient {
 public:
  ~LayerTreeImpl() override;

  // LayerTree.
  cc::UIResourceManager* GetUIResourceManager() override;
  void SetViewportRectAndScale(
      const gfx::Rect& device_viewport_rect,
      float device_scale_factor,
      const viz::LocalSurfaceId& local_surface_id) override;
  void set_background_color(SkColor4f color) override;
  void SetVisible(bool visible) override;
  bool IsVisible() const override;
  void RequestPresentationTimeForNextFrame(
      PresentationCallback callback) override;
  void RequestSuccessfulPresentationTimeForNextFrame(
      SuccessfulCallback callback) override;
  void set_display_transform_hint(gfx::OverlayTransform hint) override;
  void RequestCopyOfOutput(
      std::unique_ptr<viz::CopyOutputRequest> request) override;
  base::OnceClosure DeferBeginFrame() override;
  void SetNeedsAnimate() override;
  void MaybeCompositeNow() override;
  const scoped_refptr<Layer>& root() const override;
  void SetRoot(scoped_refptr<Layer> root) override;
  void SetFrameSink(std::unique_ptr<FrameSink> sink) override;
  void ReleaseLayerTreeFrameSink() override;
  std::unique_ptr<ScopedKeepSurfaceAlive> CreateScopedKeepSurfaceAlive(
      const viz::SurfaceId& surface_id) override;
  const SurfaceRangesAndCounts& GetSurfaceRangesForTesting() const override;
  void SetNeedsRedrawForTesting() override;

  // FrameSinkImplClient.
  bool BeginFrame(const viz::BeginFrameArgs& args,
                  viz::CompositorFrame& out_frame,
                  base::flat_set<viz::ResourceId>& out_resource_ids,
                  viz::HitTestRegionList& out_hit_test_region_list) override;
  void DidReceiveCompositorFrameAck() override;
  void DidSubmitCompositorFrame() override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override;
  void DidLoseLayerTreeFrameSink() override;

  // Internal methods called by Layers.
  void NotifyTreeChanged();
  viz::ClientResourceProvider* GetClientResourceProvider();
  viz::ResourceId GetVizResourceId(cc::UIResourceId id);
  bool IsUIResourceOpaque(int resource_id);
  gfx::Size GetUIResourceSize(int resource_id);
  void AddSurfaceRange(const viz::SurfaceRange& range);
  void RemoveSurfaceRange(const viz::SurfaceRange& range);
  void RegisterOffsetTag(const viz::OffsetTag& tag, SurfaceLayer* owner);
  void UnregisterOffsetTag(const viz::OffsetTag& tag, SurfaceLayer* owner);

 private:
  friend class LayerTree;
  friend class TestLayerTreeImpl;

  struct PresentationCallbackInfo {
    PresentationCallbackInfo(
        uint32_t frame_token,
        std::vector<PresentationCallback> presentation_callbacks,
        std::vector<SuccessfulCallback> success_callbacks);
    ~PresentationCallbackInfo();
    PresentationCallbackInfo(PresentationCallbackInfo&&);
    PresentationCallbackInfo& operator=(PresentationCallbackInfo&&);

    PresentationCallbackInfo(const PresentationCallbackInfo&) = delete;
    PresentationCallbackInfo& operator=(const PresentationCallbackInfo&) =
        delete;

    uint32_t frame_token = 0u;
    std::vector<PresentationCallback> presentation_callbacks;
    std::vector<SuccessfulCallback> success_callbacks;
  };

  LayerTreeImpl(LayerTreeClient* client,
                uint32_t num_unneeded_begin_frame_before_stop,
                int min_occlusion_tracking_dimension);

  // Request a new frame sink from the client if a new frame sink is needed and
  // there isn't already a pending request.
  void MaybeRequestFrameSink();
  // Matches `DeferBeginFrame` that reduces number of outstanding requests to
  // defer (ie stop) BeginFrames to the client.
  void ReleaseDeferBeginFrame();
  void UpdateNeedsBeginFrame();
  void SetClientNeedsOneBeginFrame();
  // Call this whenever there are tree or layer changes that needs to be
  // submitted in a CompositorFrame.
  void SetNeedsDraw();
  bool NeedsDraw() const;
  bool NeedsBeginFrames() const;
  void GenerateCompositorFrame(
      const viz::BeginFrameArgs& args,
      viz::CompositorFrame& out_frame,
      base::flat_set<viz::ResourceId>& out_resource_ids,
      viz::HitTestRegionList& out_hit_test_region_list);
  void Draw(Layer& layer,
            viz::CompositorRenderPass& render_pass,
            FrameData& data,
            const gfx::Transform& parent_transform_to_root,
            const gfx::Transform& parent_transform_to_target,
            const gfx::RectF* parent_clip_in_target,
            const gfx::RectF& clip_in_parent,
            float opacity);
  void DrawChildrenAndAppendQuads(Layer& layer,
                                  viz::CompositorRenderPass& render_pass,
                                  FrameData& data,
                                  const gfx::Transform& transform_to_root,
                                  const gfx::Transform& transform_to_target,
                                  const gfx::RectF* clip_in_target,
                                  const gfx::RectF& clip_in_layer,
                                  float opacity);
  // Updates the `FrameData::occlusion_in_target` field with the visible_rect.
  // Return if layer's AppendQuads should happen. May reduce `visible_rect` if
  // it's partially occluded.
  bool UpdateOcclusionRect(Layer& layer,
                           FrameData& data,
                           const gfx::Transform& transform_to_target,
                           float opacity,
                           const gfx::RectF& visible_rectf_in_target,
                           gfx::RectF& visible_rect);
  // Compute and update `damage_rect` and `has_damage_from_contributing_content`
  // of `render_pass`. `data.render_pass_damage` should be the newly computed
  // damage data of the frame being produced. Damage data from previous frame is
  // retrieved from `damage_from_previous_frame_`. `data.render_pass_damage` is
  // moved into `data.current_frame_data` and then cleared, to avoid copying
  // data.
  void ProcessDamageForRenderPass(viz::CompositorRenderPass& render_pass,
                                  FrameData& data);

  const raw_ptr<LayerTreeClient> client_;
  const uint32_t num_unneeded_begin_frame_before_stop_;
  const int min_occlusion_tracking_dimension_;
  std::unique_ptr<FrameSinkImpl> frame_sink_;

  cc::UIResourceManager ui_resource_manager_;

  viz::ChildLocalSurfaceIdAllocator local_surface_id_allocator_;

  bool frame_sink_request_pending_ = false;
  // Indicates there is an `UpdateNeedsBeginFrame` call pending in the current
  // task lower in the stack frame. This is to prevent unnecessary back and
  // forth flips.
  bool update_needs_begin_frame_pending_ = false;
  // Set when client requests a begin frame viz `SetNeedsAnimate`.
  bool client_needs_one_begin_frame_ = false;
  // Set to indicate there are layer or tree changes that's not yet submitted
  // in a CompositorFrame.
  bool needs_draw_ = false;
  bool visible_ = false;
  uint32_t num_defer_begin_frame_ = 0u;
  // Number of begin frames with no draw. Stop requesting begin frames after
  // this reaches `num_unneeded_begin_frame_before_stop_`.
  // TODO(boliu): Move this logic to DelayedScheduler.
  uint32_t num_begin_frames_with_no_draw_ =
      num_unneeded_begin_frame_before_stop_;

  gfx::Rect device_viewport_rect_;
  float device_scale_factor_ = 1.0f;
  SkColor4f background_color_ = SkColors::kWhite;
  SurfaceRangesAndCounts referenced_surfaces_;

  // Tracks OffsetTags and which SurfaceLayer they were registered with.
  base::flat_map<viz::OffsetTag, raw_ptr<SurfaceLayer>> registered_offset_tags_;
  viz::FrameTokenGenerator next_frame_token_;
  gfx::OverlayTransform display_transform_hint_ = gfx::OVERLAY_TRANSFORM_NONE;

  std::vector<std::unique_ptr<viz::CopyOutputRequest>>
      copy_requests_for_next_frame_;
  // These are added to `pending_presentation_callbacks_` in the next frame.
  std::vector<PresentationCallback> presentation_callback_for_next_frame_;
  std::vector<SuccessfulCallback> success_callback_for_next_frame_;

  FrameDamageData damage_from_previous_frame_;

  base::circular_deque<PresentationCallbackInfo>
      pending_presentation_callbacks_;

  // Destroy Layers before other fields that might be accessed by Layers.
  scoped_refptr<Layer> root_;

  base::WeakPtrFactory<LayerTreeImpl> weak_factory_{this};
};

}  // namespace cc::slim

#endif  // CC_SLIM_LAYER_TREE_IMPL_H_
