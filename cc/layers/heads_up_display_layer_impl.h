// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_HEADS_UP_DISPLAY_LAYER_IMPL_H_
#define CC_LAYERS_HEADS_UP_DISPLAY_LAYER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/metrics/web_vital_metrics.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/resource_pool.h"
#include "cc/trees/debug_rect_history.h"
#include "cc/trees/layer_tree_impl.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkTypeface;
struct SkRect;

namespace viz {
class ClientResourceProvider;
}

namespace cc {
class DroppedFrameCounter;
class LayerTreeFrameSink;
class PaintCanvas;
class PaintFlags;

enum class TextAlign { kLeft, kCenter, kRight };

class CC_EXPORT HeadsUpDisplayLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<HeadsUpDisplayLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id) {
    return base::WrapUnique(new HeadsUpDisplayLayerImpl(tree_impl, id));
  }
  HeadsUpDisplayLayerImpl(const HeadsUpDisplayLayerImpl&) = delete;
  ~HeadsUpDisplayLayerImpl() override;

  HeadsUpDisplayLayerImpl& operator=(const HeadsUpDisplayLayerImpl&) = delete;

  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  void UpdateHudTexture(DrawMode draw_mode,
                        LayerTreeFrameSink* frame_sink,
                        viz::ClientResourceProvider* resource_provider,
                        bool gpu_raster,
                        const viz::CompositorRenderPassList& list);

  void ReleaseResources() override;

  gfx::Rect GetEnclosingVisibleRectInTargetSpace() const override;

  bool IsAnimatingHUDContents() const {
    return paint_rects_fade_step_ > 0 || layout_shift_rects_fade_step_ > 0;
  }

  void SetHUDTypeface(sk_sp<SkTypeface> typeface);
  void SetLayoutShiftRects(const std::vector<gfx::Rect>& rects);
  void ClearLayoutShiftRects();
  const std::vector<gfx::Rect>& LayoutShiftRects() const;
  void SetWebVitalMetrics(std::unique_ptr<WebVitalMetrics> web_vital_metrics);

  // This evicts hud quad appended during render pass preparation.
  void EvictHudQuad(const viz::CompositorRenderPassList& list);

  // LayerImpl overrides.
  void PushPropertiesTo(LayerImpl* layer) override;

 private:
  HeadsUpDisplayLayerImpl(LayerTreeImpl* tree_impl, int id);

  const char* LayerTypeAsString() const override;

  void AsValueInto(base::trace_event::TracedValue* dict) const override;

  void UpdateHudContents();
  void DrawHudContents(PaintCanvas* canvas);
  void DrawText(PaintCanvas* canvas,
                const PaintFlags& flags,
                const std::string& text,
                TextAlign align,
                int size,
                int x,
                int y) const;
  void DrawText(PaintCanvas* canvas,
                const PaintFlags& flags,
                const std::string& text,
                TextAlign align,
                int size,
                const SkPoint& pos) const;
  void DrawGraphBackground(PaintCanvas* canvas,
                           PaintFlags* flags,
                           const SkRect& bounds) const;
  void DrawGraphLines(PaintCanvas* canvas,
                      PaintFlags* flags,
                      const SkRect& bounds) const;
  // Draw a separator line at top of bounds.
  void DrawSeparatorLine(PaintCanvas* canvas,
                         PaintFlags* flags,
                         const SkRect& bounds) const;

  SkRect DrawFrameThroughputDisplay(
      PaintCanvas* canvas,
      const DroppedFrameCounter* dropped_frame_counter,
      int right,
      int top) const;
  SkRect DrawMemoryDisplay(PaintCanvas* canvas,
                           int top,
                           int right,
                           int width) const;
  SkRect DrawGpuRasterizationStatus(PaintCanvas* canvas,
                                    int right,
                                    int top,
                                    int width) const;
  void DrawDebugRect(PaintCanvas* canvas,
                     PaintFlags* flags,
                     const DebugRect& rect,
                     SkColor4f stroke_color,
                     SkColor4f fill_color,
                     float stroke_width,
                     const std::string& label_text) const;
  void DrawDebugRects(PaintCanvas* canvas,
                      DebugRectHistory* debug_rect_history);

  // This function draws a single web vital metric. If the metrics doesn't have
  // a valid value, the value is set to -1. This function returns the height
  // of the current draw so it can be used to calculate the top of the next
  // draw.
  int DrawSingleMetric(PaintCanvas* canvas,
                       int left,
                       int right,
                       int top,
                       std::string name,
                       const WebVitalMetrics::MetricsInfo& info,
                       bool has_value,
                       double value) const;
  SkRect DrawWebVitalMetrics(PaintCanvas* canvas,
                             int left,
                             int top,
                             int width) const;

  // This function draws a single smoothness related metric.
  int DrawSinglePercentageMetric(PaintCanvas* canvas,
                                 int left,
                                 int right,
                                 int top,
                                 std::string name,
                                 double value) const;
  SkRect DrawSmoothnessMetrics(PaintCanvas* canvas,
                               int left,
                               int top,
                               int width) const;

  int bounds_width_in_dips() const {
    // bounds() is specified in layout coordinates, which is painted dsf away
    // from DIPs.
    return bounds().width() / layer_tree_impl()->painted_device_scale_factor();
  }

  ResourcePool::InUsePoolResource in_flight_resource_;
  std::unique_ptr<ResourcePool> pool_;
  raw_ptr<viz::DrawQuad> current_quad_ = nullptr;
  // Used for software raster when it will be uploaded to a texture.
  sk_sp<SkSurface> staging_surface_;

  sk_sp<SkTypeface> typeface_;
  std::vector<gfx::Rect> layout_shift_rects_;

  float internal_contents_scale_ = 1.0f;
  gfx::Size internal_content_bounds_;

  uint32_t throughput_value_ = 0.0f;
  // Obtained from the current BeginFrameArgs.
  absl::optional<base::TimeDelta> frame_interval_;
  MemoryHistory::Entry memory_entry_;
  int paint_rects_fade_step_ = 0;
  int layout_shift_rects_fade_step_ = 0;
  std::vector<DebugRect> paint_rects_;
  std::vector<DebugRect> layout_shift_debug_rects_;

  std::unique_ptr<WebVitalMetrics> web_vital_metrics_;

  base::TimeTicks time_of_last_graph_update_;
};

}  // namespace cc

#endif  // CC_LAYERS_HEADS_UP_DISPLAY_LAYER_IMPL_H_
