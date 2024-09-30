// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_BROWSER_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_BROWSER_VIEW_RENDERER_H_

#include <stddef.h>

#include <map>
#include <optional>
#include <set>

#include "android_webview/browser/gfx/begin_frame_source_webview.h"
#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/compositor_frame_producer.h"
#include "android_webview/browser/gfx/parent_compositor_draw_constraints.h"
#include "android_webview/browser/gfx/root_frame_sink_proxy.h"
#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

class SkCanvas;
class SkPicture;

namespace content {
class WebContents;
}

namespace android_webview {

class BrowserViewRendererClient;
class ChildFrame;
class CompositorFrameConsumer;
class RootFrameSinkProxy;

// Interface for all the WebView-specific content rendering operations.
// Provides software and hardware rendering and the Capture Picture API.
//
// Lifetime: WebView
class BrowserViewRenderer : public content::SynchronousCompositorClient,
                            public CompositorFrameProducer,
                            public RootFrameSinkProxyClient {
 public:
  static void CalculateTileMemoryPolicy();
  static BrowserViewRenderer* FromWebContents(
      content::WebContents* web_contents);

  BrowserViewRenderer(
      BrowserViewRendererClient* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  BrowserViewRenderer(const BrowserViewRenderer&) = delete;
  BrowserViewRenderer& operator=(const BrowserViewRenderer&) = delete;

  ~BrowserViewRenderer() override;

  void RegisterWithWebContents(content::WebContents* web_contents);

  // The BrowserViewRenderer client is responsible for ensuring that
  // the current compositor frame consumer has been set correctly via
  // this method.  The consumer is added to the set of registered
  // consumers if it is not already registered.
  void SetCurrentCompositorFrameConsumer(
      CompositorFrameConsumer* compositor_frame_consumer);

  // Called before either OnDrawHardware or OnDrawSoftware to set the view
  // state of this frame. |scroll| is the view's current scroll offset.
  // |global_visible_rect| is the intersection of the view size and the window
  // in window coordinates.
  void PrepareToDraw(const gfx::Point& scroll,
                     const gfx::Rect& global_visible_rect);

  // Main handlers for view drawing. A false return value indicates no new
  // frame is produced.
  bool OnDrawHardware();
  bool OnDrawSoftware(SkCanvas* canvas);

  float GetVelocityInPixelsPerSecond();

  bool NeedToDrawBackgroundColor();

  // CapturePicture API methods.
  sk_sp<SkPicture> CapturePicture(int width, int height);
  void EnableOnNewPicture(bool enabled);

  void ClearView();

  void SetOffscreenPreRaster(bool enabled);

  // View update notifications.
  void SetIsPaused(bool paused);
  void SetViewVisibility(bool visible);
  void SetWindowVisibility(bool visible);
  void OnSizeChanged(int width, int height);
  void OnAttachedToWindow(int width, int height);
  void OnDetachedFromWindow();
  void ZoomBy(float delta);
  void OnComputeScroll(base::TimeTicks animation_time);

  // Sets the scale for logical<->physical pixel conversions.
  void SetDipScale(float dip_scale);
  float dip_scale() const { return dip_scale_; }
  float page_scale_factor() const { return page_scale_factor_; }

  // Set the root layer scroll offset to |new_value|. The |new_value| here is in
  // physical pixel.
  void ScrollTo(const gfx::Point& new_value);

  // Set root layer scroll offset on the next scroll state update.
  void RestoreScrollAfterTransition(const gfx::Point& new_value);

  // Android views hierarchy gluing.
  bool IsVisible() const;
  gfx::Rect GetScreenRect() const;
  bool view_visible() const { return view_visible_; }
  bool window_visible() const { return window_visible_; }
  bool attached_to_window() const { return attached_to_window_; }
  bool was_attached() const { return was_attached_; }
  gfx::Size size() const { return size_; }

  bool IsClientVisible() const;
  void TrimMemory();

  // SynchronousCompositorClient overrides.
  void DidInitializeCompositor(content::SynchronousCompositor* compositor,
                               const viz::FrameSinkId& frame_sink_id) override;
  void DidDestroyCompositor(content::SynchronousCompositor* compositor,
                            const viz::FrameSinkId& frame_sink_id) override;
  void PostInvalidate(content::SynchronousCompositor* compositor) override;
  void DidUpdateContent(content::SynchronousCompositor* compositor) override;
  void OnInputEvent();

  // |total_scroll_offset|, |total_max_scroll_offset|, and |scrollable_size| are
  // in DIP scale when --use-zoom-for-dsf is disabled. Otherwise, they are in
  // physical pixel scale.
  void UpdateRootLayerState(content::SynchronousCompositor* compositor,
                            const gfx::PointF& total_scroll_offset,
                            const gfx::PointF& total_max_scroll_offset,
                            const gfx::SizeF& scrollable_size,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) override;

  void DidOverscroll(content::SynchronousCompositor* compositor,
                     const gfx::Vector2dF& accumulated_overscroll,
                     const gfx::Vector2dF& latest_overscroll_delta,
                     const gfx::Vector2dF& current_fling_velocity) override;
  ui::TouchHandleDrawable* CreateDrawable() override;
  void CopyOutput(
      content::SynchronousCompositor* compositor,
      std::unique_ptr<viz::CopyOutputRequest> copy_request) override;

  void AddBeginFrameCompletionCallback(base::OnceClosure callback) override;

  void SetThreadIds(const std::vector<int32_t>& thread_ids) override;

  // CompositorFrameProducer overrides
  base::WeakPtr<CompositorFrameProducer> GetWeakPtr() override;
  void RemoveCompositorFrameConsumer(
      CompositorFrameConsumer* consumer) override;
  void ReturnUsedResources(std::vector<viz::ReturnedResource> resources,
                           const viz::FrameSinkId& frame_sink_id,
                           uint32_t layer_tree_frame_sink_id) override;
  void OnParentDrawDataUpdated(
      CompositorFrameConsumer* compositor_frame_consumer) override;
  void OnViewTreeForceDarkStateChanged(
      bool view_tree_force_dark_state) override;
  void ChildSurfaceWasEvicted() override;

  void SetActiveFrameSinkId(const viz::FrameSinkId& frame_sink_id);

  // RootFrameSinkProxy overrides
  void Invalidate() override;
  void ReturnResourcesFromViz(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      std::vector<viz::ReturnedResource> resources) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      uint32_t sequence_id) override;

  // Visible for testing.
  content::SynchronousCompositor* GetActiveCompositorForTesting() const {
    return compositor_;
  }

  bool window_visible_for_tests() const { return window_visible_; }

 private:
  void SetActiveCompositor(content::SynchronousCompositor* compositor);
  void SetTotalRootLayerScrollOffset(const gfx::PointF& new_value_dip);
  bool CanOnDraw();
  bool CompositeSW(SkCanvas* canvas, bool software_canvas);
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
  RootLayerStateAsValue(const gfx::PointF& total_scroll_offset_dip,
                        const gfx::SizeF& scrollable_size_dip);

  void ReturnUncommittedFrames(ChildFrameQueue frame);
  void ReturnUnusedResource(std::unique_ptr<ChildFrame> frame);
  void ReturnResourceFromParent(
      CompositorFrameConsumer* compositor_frame_consumer);
  void ReleaseHardware();
  bool DoUpdateParentDrawData();
  void UpdateBeginFrameSource();
  void UpdateForegroundForGpuResources();

  gfx::Point max_scroll_offset() const;

  // Return the tile rect in view space.
  gfx::Rect ComputeTileRectAndUpdateMemoryPolicy();

  content::SynchronousCompositor* FindCompositor(
      const viz::FrameSinkId& frame_sink_id) const;
  // For debug tracing or logging. Return the string representation of this
  // view renderer's state.
  std::string ToString() const;

  void SetBrowserIOThreadId(base::PlatformThreadId thread_id);

  const raw_ptr<BrowserViewRendererClient> client_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  raw_ptr<CompositorFrameConsumer> current_compositor_frame_consumer_;
  std::unique_ptr<RootFrameSinkProxy> root_frame_sink_proxy_;

  // The current compositor that's owned by the current RVH.
  raw_ptr<content::SynchronousCompositor> compositor_;
  // The id of the most recent RVH according to RVHChanged.
  viz::FrameSinkId frame_sink_id_;
  // A map from compositor's per-WebView unique ID to the compositor's raw
  // pointer. A raw pointer here is fine because the entry will be erased when
  // a compositor is destroyed.
  std::map<viz::FrameSinkId,
           raw_ptr<content::SynchronousCompositor, CtnExperimental>>
      compositor_map_;

  bool is_paused_;
  bool view_visible_;
  bool window_visible_;  // Only applicable if |attached_to_window_| is true.
  bool attached_to_window_;
  bool was_attached_;  // Whether the view was attached to window at least once.
  bool hardware_enabled_;
  float dip_scale_;
  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
  bool on_new_picture_enable_;
  bool clear_view_;

  bool foreground_for_gpu_resources_ = false;

  // Used for metrics, indicates if we called invalidate since last draw.
  bool did_invalidate_since_last_draw_ = false;

  // Approximates whether render thread functor has a frame to draw. It is safe
  // for Java side to stop blitting the background color once this is true.
  bool has_rendered_frame_ = false;

  bool offscreen_pre_raster_;

  CopyOutputRequestQueue copy_requests_;

  gfx::Point last_on_draw_scroll_offset_;
  gfx::Rect last_on_draw_global_visible_rect_;

  gfx::Size size_;

  gfx::SizeF scrollable_size_dip_;

  // When zoom-for-dsf enabled |max_scroll_offset_unscaled_| and
  // |scroll_offset_unscaled_| is in physical pixel; otherwise, they are in dip
  gfx::PointF scroll_offset_unscaled_;
  gfx::PointF max_scroll_offset_unscaled_;

  // Used to prevent rounding errors from accumulating enough to generate
  // visible skew (especially noticeable when scrolling up and down in the same
  // spot over a period of time).
  gfx::Vector2dF overscroll_rounding_error_;

  // The scroll to apply after the next scroll state update.
  std::optional<gfx::Point> scroll_on_scroll_state_update_;

  ParentCompositorDrawConstraints external_draw_constraints_;

  std::unique_ptr<BeginFrameSourceWebView> begin_frame_source_;

  std::vector<int32_t> renderer_thread_ids_;
  base::PlatformThreadId browser_io_thread_id_ = base::kInvalidThreadId;

  base::WeakPtrFactory<BrowserViewRenderer> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_BROWSER_VIEW_RENDERER_H_
