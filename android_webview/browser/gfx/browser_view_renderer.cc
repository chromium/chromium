// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/browser_view_renderer.h"

#include <memory>
#include <utility>

#include "android_webview/browser/gfx/browser_view_renderer_client.h"
#include "android_webview/browser/gfx/compositor_frame_consumer.h"
#include "android_webview/browser/gfx/root_frame_sink.h"
#include "android_webview/browser/gfx/root_frame_sink_proxy.h"
#include "android_webview/common/aw_features.h"
#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace android_webview {

namespace {

const double kEpsilon = 1e-8;

// Used to calculate memory allocation. Determined experimentally.
const size_t kMemoryMultiplier = 20;
const size_t kBytesPerPixel = 4;
const size_t kMemoryAllocationStep = 5 * 1024 * 1024;
uint64_t g_memory_override_in_bytes = 0u;

const void* const kBrowserViewRendererUserDataKey =
    &kBrowserViewRendererUserDataKey;

class BrowserViewRendererUserData : public base::SupportsUserData::Data {
 public:
  explicit BrowserViewRendererUserData(BrowserViewRenderer* ptr) : bvr_(ptr) {}

  static BrowserViewRenderer* GetBrowserViewRenderer(
      content::WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    BrowserViewRendererUserData* data =
        static_cast<BrowserViewRendererUserData*>(
            web_contents->GetUserData(kBrowserViewRendererUserDataKey));
    return data ? data->bvr_.get() : NULL;
  }

 private:
  raw_ptr<BrowserViewRenderer> bvr_;
};

}  // namespace

// static
void BrowserViewRenderer::CalculateTileMemoryPolicy() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  // If the value was overridden on the command line, use the specified value.
  bool client_hard_limit_bytes_overridden =
      cl->HasSwitch(switches::kForceGpuMemAvailableMb);
  if (client_hard_limit_bytes_overridden) {
    base::StringToUint64(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceGpuMemAvailableMb),
        &g_memory_override_in_bytes);
    g_memory_override_in_bytes *= 1024 * 1024;
  }
}

// static
BrowserViewRenderer* BrowserViewRenderer::FromWebContents(
    content::WebContents* web_contents) {
  return BrowserViewRendererUserData::GetBrowserViewRenderer(web_contents);
}

BrowserViewRenderer::BrowserViewRenderer(
    BrowserViewRendererClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : client_(client),
      ui_task_runner_(ui_task_runner),
      current_compositor_frame_consumer_(nullptr),
      compositor_(nullptr),
      is_paused_(false),
      view_visible_(false),
      window_visible_(false),
      attached_to_window_(false),
      was_attached_(false),
      hardware_enabled_(false),
      dip_scale_(0.f),
      page_scale_factor_(1.f),
      min_page_scale_factor_(0.f),
      max_page_scale_factor_(0.f),
      on_new_picture_enable_(false),
      clear_view_(false),
      offscreen_pre_raster_(false) {
  begin_frame_source_ = std::make_unique<BeginFrameSourceWebView>();
  root_frame_sink_proxy_ = std::make_unique<RootFrameSinkProxy>(
      ui_task_runner_, this, begin_frame_source_.get());
  UpdateBeginFrameSource();

  base::OnceCallback<base::PlatformThreadId()> compute_current_thread_id =
      base::BindOnce([]() { return base::PlatformThread::CurrentId(); });
  io_task_runner->PostTask(
      FROM_HERE, std::move(compute_current_thread_id)
                     .Then(base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &BrowserViewRenderer::SetBrowserIOThreadId,
                         weak_ptr_factory_.GetWeakPtr()))));
}

BrowserViewRenderer::~BrowserViewRenderer() {
  DCHECK(compositor_map_.empty());
  DCHECK(!current_compositor_frame_consumer_);
  if (foreground_for_gpu_resources_) {
    // Cannot leave a dangling foreground compositor. Just detach from
    // destructor.
    OnDetachedFromWindow();
  }

  // We need to destroy |root_frame_sink_proxy_| before |begin_frame_source_|;
  root_frame_sink_proxy_.reset();
}

base::WeakPtr<CompositorFrameProducer> BrowserViewRenderer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BrowserViewRenderer::SetCurrentCompositorFrameConsumer(
    CompositorFrameConsumer* compositor_frame_consumer) {
  if (compositor_frame_consumer == current_compositor_frame_consumer_) {
    return;
  }
  current_compositor_frame_consumer_ = compositor_frame_consumer;
  if (current_compositor_frame_consumer_) {
    // Previous renderer will evict CompositorFrame, compositor needs to submit
    // next frames with new local surface id.
    if (compositor_)
      compositor_->WasEvicted();

    RootFrameSinkGetter root_sink_getter;
    if (root_frame_sink_proxy_)
      root_sink_getter = root_frame_sink_proxy_->GetRootFrameSinkCallback();
    current_compositor_frame_consumer_->SetCompositorFrameProducer(
        this, std::move(root_sink_getter));
    OnParentDrawDataUpdated(current_compositor_frame_consumer_);
  }
}

void BrowserViewRenderer::RegisterWithWebContents(
    content::WebContents* web_contents) {
  web_contents->SetUserData(
      kBrowserViewRendererUserDataKey,
      std::make_unique<BrowserViewRendererUserData>(this));
}

void BrowserViewRenderer::TrimMemory() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::TrimMemory");

  // Trimming memory might destroy HardwareRenderer which will evict
  // CompositorFrame, compositor needs to submit next frames with new local
  // surface id.
  if (compositor_)
    compositor_->WasEvicted();

  // Just set the memory limit to 0 and drop all tiles. This will be reset to
  // normal levels in the next DrawGL call.
  if (!offscreen_pre_raster_)
    ReleaseHardware();
}

gfx::Rect BrowserViewRenderer::ComputeTileRectAndUpdateMemoryPolicy() {
  if (!compositor_) {
    return gfx::Rect();
  }

  if (!hardware_enabled_) {
    compositor_->SetMemoryPolicy(0u);
    return gfx::Rect();
  }

  gfx::Transform transform_for_tile_priority =
      external_draw_constraints_.transform;

  gfx::Rect viewport_rect_for_tile_priority_in_view_space;
  gfx::Transform screen_to_view;
  if (transform_for_tile_priority.GetInverse(&screen_to_view)) {
    // Convert from screen space to view space.
    viewport_rect_for_tile_priority_in_view_space =
        cc::MathUtil::ProjectEnclosingClippedRect(
            screen_to_view,
            gfx::Rect(external_draw_constraints_.viewport_size));
  }
  viewport_rect_for_tile_priority_in_view_space.Intersect(gfx::Rect(size_));

  size_t bytes_limit = 0u;
  if (g_memory_override_in_bytes) {
    bytes_limit = static_cast<size_t>(g_memory_override_in_bytes);
  } else {
    // Note we are using |last_on_draw_global_visible_rect_| rather than
    // |external_draw_constraints_.viewport_size|. This is to reduce budget
    // for a webview that's much smaller than the surface it's rendering.
    gfx::Rect interest_rect;
    if (offscreen_pre_raster_) {
      interest_rect = gfx::Rect(size_);
    } else {
      // Re-compute screen-space rect for computing tile budget, since tile is
      // rastered in screen space.
      gfx::Rect viewport_rect_for_tile_priority_in_screen_space =
          cc::MathUtil::ProjectEnclosingClippedRect(
              transform_for_tile_priority,
              viewport_rect_for_tile_priority_in_view_space);
      // Intersect by viewport size again, in case axis-aligning operations made
      // the rect bigger than necessary.
      viewport_rect_for_tile_priority_in_screen_space.Intersect(
          gfx::Rect(external_draw_constraints_.viewport_size));
      interest_rect = viewport_rect_for_tile_priority_in_screen_space.IsEmpty()
                          ? last_on_draw_global_visible_rect_
                          : viewport_rect_for_tile_priority_in_screen_space;
    }

    size_t width = interest_rect.width();
    size_t height = interest_rect.height();
    bytes_limit = kMemoryMultiplier * kBytesPerPixel * width * height;
    // Round up to a multiple of kMemoryAllocationStep.
    bytes_limit =
        (bytes_limit / kMemoryAllocationStep + 1) * kMemoryAllocationStep;
  }

  compositor_->SetMemoryPolicy(bytes_limit);
  return viewport_rect_for_tile_priority_in_view_space;
}

content::SynchronousCompositor* BrowserViewRenderer::FindCompositor(
    const viz::FrameSinkId& frame_sink_id) const {
  const auto& compositor_iterator = compositor_map_.find(frame_sink_id);
  if (compositor_iterator == compositor_map_.end())
    return nullptr;

  return compositor_iterator->second;
}

void BrowserViewRenderer::PrepareToDraw(const gfx::Point& scroll,
                                        const gfx::Rect& global_visible_rect) {
  last_on_draw_scroll_offset_ = scroll;
  last_on_draw_global_visible_rect_ = global_visible_rect;
}

bool BrowserViewRenderer::CanOnDraw() {
  if (!compositor_) {
    TRACE_EVENT_INSTANT0("android_webview", "EarlyOut_NoCompositor",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (clear_view_) {
    TRACE_EVENT_INSTANT0("android_webview", "EarlyOut_ClearView",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  return true;
}

bool BrowserViewRenderer::OnDrawHardware() {
  DCHECK(current_compositor_frame_consumer_);
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::OnDrawHardware");

  const bool did_invalidate = did_invalidate_since_last_draw_;
  did_invalidate_since_last_draw_ = false;

  if (!CanOnDraw()) {
    return false;
  }

  current_compositor_frame_consumer_->SetScrollOffsetOnUI(
      last_on_draw_scroll_offset_);
  hardware_enabled_ = true;

  DoUpdateParentDrawData();
  // Do not override (ie leave empty) for offscreen raster.
  gfx::Size viewport_size_for_tile_priority =
      offscreen_pre_raster_ ? gfx::Size()
                            : external_draw_constraints_.viewport_size;
  gfx::Rect viewport_rect_for_tile_priority_in_view_space =
      ComputeTileRectAndUpdateMemoryPolicy();

  // Explanation for the various viewports and transforms. There are:
  // * "default" viewport" (and identity transform) that's normally used by
  //   compositor. This is |size_| in this file.
  // * "draw" viewport and transform. Compositor applies them at the root at
  //   draw time. This is contained in SkCanvas for a software draw
  // * "tile" viewport and transform. These are set in hardware draw to
  //   correctly prioritize and raster tiles.
  // The draw viewport was added to support software draw's ability to change
  // the viewport and transform at draw time to anything the embedding app
  // desires. However the tile system was not expecting its viewport to jump
  // around, and only move incrementally due to user input. This required adding
  // the tile viewport and transform. Tile and default are separate to reduce
  // memory in the case when only a small portion of webview (ie the default
  // viewport) is actually visible.
  // We intersect the tile viewport with the default viewport above so that the
  // tile viewport can only shrink and not grow from the default viewport. This
  // is because webview can also be small in relation to the surface size, so
  // and growing the tile viewport can cause more tiles to be rastered than
  // necessary.

  scoped_refptr<content::SynchronousCompositor::FrameFuture> future =
      compositor_->DemandDrawHwAsync(
          size_, viewport_rect_for_tile_priority_in_view_space,
          external_draw_constraints_.transform);
  CopyOutputRequestQueue requests;
  copy_requests_.swap(requests);
  for (auto& copy_request_ptr : requests) {
    if (!copy_request_ptr->has_result_task_runner())
      copy_request_ptr->set_result_task_runner(ui_task_runner_);
  }
  std::unique_ptr<ChildFrame> child_frame = std::make_unique<ChildFrame>(
      std::move(future), frame_sink_id_, viewport_size_for_tile_priority,
      external_draw_constraints_.transform, offscreen_pre_raster_, dip_scale_,
      std::move(requests), did_invalidate,
      begin_frame_source_->LastDispatchedBeginFrameArgs(), renderer_thread_ids_,
      browser_io_thread_id_);

  ReturnUnusedResource(
      current_compositor_frame_consumer_->SetFrameOnUI(std::move(child_frame)));
  return true;
}

void BrowserViewRenderer::OnParentDrawDataUpdated(
    CompositorFrameConsumer* compositor_frame_consumer) {
  DCHECK(compositor_frame_consumer);
  if (compositor_frame_consumer != current_compositor_frame_consumer_)
    return;
  if (!DoUpdateParentDrawData())
    return;
  PostInvalidate(compositor_);
  ComputeTileRectAndUpdateMemoryPolicy();
}

bool BrowserViewRenderer::DoUpdateParentDrawData() {
  ParentCompositorDrawConstraints new_constraints;
  viz::FrameTimingDetailsMap new_timing_details;
  viz::FrameSinkId id;
  uint32_t frame_token = 0u;
  base::TimeDelta preferred_frame_interval;
  current_compositor_frame_consumer_->TakeParentDrawDataOnUI(
      &new_constraints, &id, &new_timing_details, &frame_token,
      &preferred_frame_interval);

  content::SynchronousCompositor* compositor = FindCompositor(id);
  if (compositor) {
    compositor->DidPresentCompositorFrames(std::move(new_timing_details),
                                           frame_token);
  }

  client_->SetPreferredFrameInterval(preferred_frame_interval);

  if (external_draw_constraints_ == new_constraints)
    return false;
  external_draw_constraints_ = new_constraints;
  return true;
}

void BrowserViewRenderer::OnViewTreeForceDarkStateChanged(
    bool view_tree_force_dark_state) {
  client_->OnViewTreeForceDarkStateChanged(view_tree_force_dark_state);
}

void BrowserViewRenderer::ChildSurfaceWasEvicted() {
  if (compositor_)
    compositor_->WasEvicted();
}

void BrowserViewRenderer::RemoveCompositorFrameConsumer(
    CompositorFrameConsumer* consumer) {
  ReturnUncommittedFrames(consumer->PassUncommittedFrameOnUI());
  if (current_compositor_frame_consumer_ == consumer)
    SetCurrentCompositorFrameConsumer(nullptr);
}

void BrowserViewRenderer::ReturnUncommittedFrames(
    ChildFrameQueue child_frames) {
  for (auto& child_frame : child_frames)
    ReturnUnusedResource(std::move(child_frame));
}

void BrowserViewRenderer::ReturnUnusedResource(
    std::unique_ptr<ChildFrame> child_frame) {
  if (!child_frame.get() || !child_frame->frame.get())
    return;

  std::vector<viz::ReturnedResource> resources =
      viz::TransferableResource::ReturnResources(
          child_frame->frame->resource_list);
  content::SynchronousCompositor* compositor =
      FindCompositor(child_frame->frame_sink_id);
  if (compositor && !resources.empty())
    compositor->ReturnResources(child_frame->layer_tree_frame_sink_id,
                                std::move(resources));
}

void BrowserViewRenderer::ReturnUsedResources(
    std::vector<viz::ReturnedResource> resources,
    const viz::FrameSinkId& frame_sink_id,
    uint32_t layer_tree_frame_sink_id) {
  content::SynchronousCompositor* compositor = FindCompositor(frame_sink_id);
  if (compositor && !resources.empty())
    compositor->ReturnResources(layer_tree_frame_sink_id, std::move(resources));
  has_rendered_frame_ = true;
}

bool BrowserViewRenderer::OnDrawSoftware(SkCanvas* canvas) {
  did_invalidate_since_last_draw_ = false;
  return CanOnDraw() && CompositeSW(canvas, /*software_canvas=*/true);
}

float BrowserViewRenderer::GetVelocityInPixelsPerSecond() {
  if (!compositor_) {
    return 0.f;
  }
  return compositor_->GetVelocityInPixelsPerSecond();
}

bool BrowserViewRenderer::NeedToDrawBackgroundColor() {
  return !has_rendered_frame_;
}

sk_sp<SkPicture> BrowserViewRenderer::CapturePicture(int width,
                                                     int height) {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::CapturePicture");

  // Return empty Picture objects for empty SkPictures.
  if (width <= 0 || height <= 0) {
    SkPictureRecorder emptyRecorder;
    emptyRecorder.beginRecording(0, 0);
    return emptyRecorder.finishRecordingAsPicture();
  }

  SkPictureRecorder recorder;
  SkCanvas* rec_canvas = recorder.beginRecording(width, height);
  if (compositor_) {
    {
      // Reset scroll back to the origin, will go back to the old
      // value when scroll_reset is out of scope.
      base::AutoReset<gfx::PointF> scroll_reset(&scroll_offset_unscaled_,
                                                gfx::PointF());
      compositor_->DidChangeRootLayerScrollOffset(scroll_offset_unscaled_);
      CompositeSW(rec_canvas, /*software_canvas=*/false);
    }
    compositor_->DidChangeRootLayerScrollOffset(scroll_offset_unscaled_);
  }
  return recorder.finishRecordingAsPicture();
}

void BrowserViewRenderer::EnableOnNewPicture(bool enabled) {
  on_new_picture_enable_ = enabled;
}

void BrowserViewRenderer::ClearView() {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::ClearView",
                       TRACE_EVENT_SCOPE_THREAD);
  if (clear_view_)
    return;

  clear_view_ = true;
  // Always invalidate ignoring the compositor to actually clear the webview.
  PostInvalidate(compositor_);
}

void BrowserViewRenderer::SetOffscreenPreRaster(bool enable) {
  if (offscreen_pre_raster_ != enable) {
    offscreen_pre_raster_ = enable;
    ComputeTileRectAndUpdateMemoryPolicy();
  }
}

void BrowserViewRenderer::SetIsPaused(bool paused) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetIsPaused",
                       TRACE_EVENT_SCOPE_THREAD,
                       "paused",
                       paused);
  is_paused_ = paused;
  UpdateBeginFrameSource();
}

void BrowserViewRenderer::SetViewVisibility(bool view_visible) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetViewVisibility",
                       TRACE_EVENT_SCOPE_THREAD,
                       "view_visible",
                       view_visible);
  view_visible_ = view_visible;
}

void BrowserViewRenderer::SetWindowVisibility(bool window_visible) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetWindowVisibility",
                       TRACE_EVENT_SCOPE_THREAD,
                       "window_visible",
                       window_visible);
  window_visible_ = window_visible;
  UpdateBeginFrameSource();
  UpdateForegroundForGpuResources();
}

void BrowserViewRenderer::OnSizeChanged(int width, int height) {
  TRACE_EVENT_INSTANT2("android_webview",
                       "BrowserViewRenderer::OnSizeChanged",
                       TRACE_EVENT_SCOPE_THREAD,
                       "width",
                       width,
                       "height",
                       height);
  size_.SetSize(width, height);
  if (offscreen_pre_raster_)
    ComputeTileRectAndUpdateMemoryPolicy();
}

void BrowserViewRenderer::OnAttachedToWindow(int width, int height) {
  TRACE_EVENT2("android_webview",
               "BrowserViewRenderer::OnAttachedToWindow",
               "width",
               width,
               "height",
               height);
  attached_to_window_ = true;
  was_attached_ = true;

  size_.SetSize(width, height);
  if (offscreen_pre_raster_)
    ComputeTileRectAndUpdateMemoryPolicy();
  UpdateBeginFrameSource();
  UpdateForegroundForGpuResources();
}

void BrowserViewRenderer::OnDetachedFromWindow() {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::OnDetachedFromWindow");
  attached_to_window_ = false;
  ReleaseHardware();
  UpdateBeginFrameSource();
  UpdateForegroundForGpuResources();
}

void BrowserViewRenderer::ZoomBy(float delta) {
  if (!compositor_)
    return;
  compositor_->SynchronouslyZoomBy(
      delta, gfx::Point(size_.width() / 2, size_.height() / 2));
}

void BrowserViewRenderer::OnComputeScroll(base::TimeTicks animation_time) {
  if (!compositor_)
    return;
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::OnComputeScroll");
  compositor_->OnComputeScroll(animation_time);
}

void BrowserViewRenderer::ReleaseHardware() {
  if (current_compositor_frame_consumer_) {
    ReturnUncommittedFrames(
        current_compositor_frame_consumer_->PassUncommittedFrameOnUI());
  }
  hardware_enabled_ = false;
  has_rendered_frame_ = false;
  ComputeTileRectAndUpdateMemoryPolicy();
}

bool BrowserViewRenderer::IsVisible() const {
  // Ignore |window_visible_| if |attached_to_window_| is false.
  return view_visible_ && (!attached_to_window_ || window_visible_);
}

bool BrowserViewRenderer::IsClientVisible() const {
  // When WebView is not paused, we declare it visible even before it is
  // attached to window to allow for background operations. If it ever gets
  // attached though, the WebView is visible as long as it is attached
  // to a window and the window is visible.
  return is_paused_
             ? false
             : !was_attached_ || (attached_to_window_ && window_visible_);
}

void BrowserViewRenderer::UpdateBeginFrameSource() {
  if (IsClientVisible()) {
    begin_frame_source_->SetParentSource(
        RootBeginFrameSourceWebView::GetInstance());
  } else {
    begin_frame_source_->SetParentSource(nullptr);
  }
}

void BrowserViewRenderer::UpdateForegroundForGpuResources() {
  bool foreground = attached_to_window_ && window_visible_;
  if (foreground != foreground_for_gpu_resources_) {
    foreground_for_gpu_resources_ = foreground;
    if (!compositor_) {
      return;
    }
    if (foreground_for_gpu_resources_) {
      compositor_->OnCompositorVisible();
    } else {
      compositor_->OnCompositorHidden();
    }
  }
}

gfx::Rect BrowserViewRenderer::GetScreenRect() const {
  return gfx::Rect(client_->GetLocationOnScreen(), size_);
}

void BrowserViewRenderer::DidInitializeCompositor(
    content::SynchronousCompositor* compositor,
    const viz::FrameSinkId& frame_sink_id) {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::DidInitializeCompositor",
                       TRACE_EVENT_SCOPE_THREAD);
  DCHECK(compositor);
  // This assumes that a RenderViewHost has at most 1 synchronous compositor
  // througout its lifetime.
  DCHECK(compositor_map_.count(frame_sink_id) == 0);
  compositor_map_[frame_sink_id] = compositor;
  if (root_frame_sink_proxy_)
    root_frame_sink_proxy_->AddChildFrameSinkId(frame_sink_id);

  compositor->SetBeginFrameSource(begin_frame_source_.get());

  // At this point, the RVHChanged event for the new RVH that contains the
  // |compositor| might have been fired already, in which case just set the
  // current compositor with the new compositor.
  if (frame_sink_id == frame_sink_id_)
    SetActiveCompositor(compositor);
}

void BrowserViewRenderer::DidDestroyCompositor(
    content::SynchronousCompositor* compositor,
    const viz::FrameSinkId& frame_sink_id) {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::DidDestroyCompositor",
                       TRACE_EVENT_SCOPE_THREAD);
  DCHECK(compositor_map_.count(frame_sink_id));
  if (compositor_ == compositor) {
    if (compositor_ && foreground_for_gpu_resources_) {
      compositor_->OnCompositorHidden();
    }
    compositor_ = nullptr;
    copy_requests_.clear();
  }

  if (root_frame_sink_proxy_)
    root_frame_sink_proxy_->RemoveChildFrameSinkId(frame_sink_id);
  compositor_map_.erase(frame_sink_id);
}

void BrowserViewRenderer::SetActiveFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
  SetActiveCompositor(FindCompositor(frame_sink_id));
}

void BrowserViewRenderer::SetActiveCompositor(
    content::SynchronousCompositor* compositor) {
  if (compositor_ == compositor)
    return;

  content::SynchronousCompositor* existing_compositor = compositor_;
  if (existing_compositor) {
    existing_compositor->SetMemoryPolicy(0u);
  }
  compositor_ = compositor;
  copy_requests_.clear();
  if (compositor_) {
    ComputeTileRectAndUpdateMemoryPolicy();
    compositor_->DidBecomeActive();
  }
  if (foreground_for_gpu_resources_) {
    if (compositor_) {
      compositor_->OnCompositorVisible();
    }
    if (existing_compositor) {
      existing_compositor->OnCompositorHidden();
    }
  }
}

void BrowserViewRenderer::SetDipScale(float dip_scale) {
  dip_scale_ = dip_scale;
  CHECK_GT(dip_scale_, 0.f);
}

gfx::Point BrowserViewRenderer::max_scroll_offset() const {
  DCHECK_GT(dip_scale_, 0.f);
  return gfx::ToCeiledPoint(
      gfx::ScalePoint(max_scroll_offset_unscaled_, page_scale_factor_));
}

void BrowserViewRenderer::ScrollTo(const gfx::Point& scroll_offset) {
  gfx::Point max_offset = max_scroll_offset();
  gfx::PointF scroll_offset_unscaled;
  // To preserve the invariant that scrolling to the maximum physical pixel
  // value also scrolls to the maximum dip pixel value we transform the physical
  // offset into the dip offset by using a proportion (instead of dividing by
  // dip_scale * page_scale_factor).
  if (max_offset.x()) {
    scroll_offset_unscaled.set_x(
        (scroll_offset.x() * max_scroll_offset_unscaled_.x()) / max_offset.x());
  }
  if (max_offset.y()) {
    scroll_offset_unscaled.set_y(
        (scroll_offset.y() * max_scroll_offset_unscaled_.y()) / max_offset.y());
  }

  DCHECK_LE(0.f, scroll_offset_unscaled.x());
  DCHECK_LE(0.f, scroll_offset_unscaled.y());
  DCHECK(scroll_offset_unscaled.x() < max_scroll_offset_unscaled_.x() ||
         scroll_offset_unscaled.x() - max_scroll_offset_unscaled_.x() <
             kEpsilon)
      << scroll_offset_unscaled.x() << " " << max_scroll_offset_unscaled_.x();
  DCHECK(scroll_offset_unscaled.y() < max_scroll_offset_unscaled_.y() ||
         scroll_offset_unscaled.y() - max_scroll_offset_unscaled_.y() <
             kEpsilon)
      << scroll_offset_unscaled.y() << " " << max_scroll_offset_unscaled_.y();

  if (scroll_offset_unscaled_ == scroll_offset_unscaled)
    return;

  scroll_offset_unscaled_ = scroll_offset_unscaled;

  TRACE_EVENT_INSTANT2("android_webview", "BrowserViewRenderer::ScrollTo",
                       TRACE_EVENT_SCOPE_THREAD, "x",
                       scroll_offset_unscaled.x(), "y",
                       scroll_offset_unscaled.y());

  if (compositor_)
    compositor_->DidChangeRootLayerScrollOffset(scroll_offset_unscaled);
}

void BrowserViewRenderer::RestoreScrollAfterTransition(
    const gfx::Point& scroll_offset) {
  // Determine if the clipped scroll offset.
  gfx::Point clipped_offset = scroll_offset;
  clipped_offset.SetToMin(max_scroll_offset());

  // If the scroll will be clipped due to the max scroll then we haven't
  // received a ScrollStateUpdate with the revised max scroll values. This
  // situation occurs due to a race in exiting fullscreen mode request, we need
  // to wait for the max scroll values to be updated before applying the
  // restored scroll values.
  if (clipped_offset != scroll_offset) {
    scroll_on_scroll_state_update_ = scroll_offset;
  } else {
    ScrollTo(scroll_offset);
  }
}

void BrowserViewRenderer::DidUpdateContent(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::DidUpdateContent",
                       TRACE_EVENT_SCOPE_THREAD);
  if (compositor != compositor_)
    return;

  clear_view_ = false;
  if (on_new_picture_enable_)
    client_->OnNewPicture();
}

void BrowserViewRenderer::SetTotalRootLayerScrollOffset(
    const gfx::PointF& scroll_offset_unscaled) {
  if (scroll_offset_unscaled_ == scroll_offset_unscaled)
    return;
  scroll_offset_unscaled_ = scroll_offset_unscaled;

  gfx::Point max_offset = max_scroll_offset();
  gfx::Point scroll_offset;
  // For an explanation as to why this is done this way see the comment in
  // BrowserViewRenderer::ScrollTo.
  if (max_scroll_offset_unscaled_.x()) {
    scroll_offset.set_x(
        base::ClampRound((scroll_offset_unscaled.x() * max_offset.x()) /
                         max_scroll_offset_unscaled_.x()));
  }

  if (max_scroll_offset_unscaled_.y()) {
    scroll_offset.set_y(
        base::ClampRound((scroll_offset_unscaled.y() * max_offset.y()) /
                         max_scroll_offset_unscaled_.y()));
  }

  DCHECK_LE(0, scroll_offset.x());
  DCHECK_LE(0, scroll_offset.y());
  DCHECK_LE(scroll_offset.x(), max_offset.x());
  DCHECK_LE(scroll_offset.y(), max_offset.y());

  client_->ScrollContainerViewTo(scroll_offset);
}

void BrowserViewRenderer::UpdateRootLayerState(
    content::SynchronousCompositor* compositor,
    const gfx::PointF& total_scroll_offset,
    const gfx::PointF& total_max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  if (compositor != compositor_)
    return;

  gfx::SizeF scrollable_size_dip = scrollable_size;
  scrollable_size_dip.Scale(1 / dip_scale_);

  TRACE_EVENT_INSTANT1(
      "android_webview", "BrowserViewRenderer::UpdateRootLayerState",
      TRACE_EVENT_SCOPE_THREAD, "state",
      RootLayerStateAsValue(total_scroll_offset, scrollable_size_dip));

  DCHECK_GE(total_max_scroll_offset.x(), 0.f);
  DCHECK_GE(total_max_scroll_offset.y(), 0.f);
  DCHECK_GT(page_scale_factor, 0.f);
  // SetDipScale should have been called at least once before this is called.
  DCHECK_GT(dip_scale_, 0.f);

  bool apply_scroll_to = false;
  if (max_scroll_offset_unscaled_ != total_max_scroll_offset ||
      scrollable_size_dip_ != scrollable_size_dip ||
      page_scale_factor_ != page_scale_factor ||
      min_page_scale_factor_ != min_page_scale_factor ||
      max_page_scale_factor_ != max_page_scale_factor) {
    max_scroll_offset_unscaled_ = total_max_scroll_offset;
    scrollable_size_dip_ = scrollable_size_dip;
    page_scale_factor_ = page_scale_factor;
    min_page_scale_factor_ = min_page_scale_factor;
    max_page_scale_factor_ = max_page_scale_factor;

    client_->UpdateScrollState(max_scroll_offset(), scrollable_size_dip,
                               page_scale_factor, min_page_scale_factor,
                               max_page_scale_factor);
    apply_scroll_to = scroll_on_scroll_state_update_.has_value();
  }

  SetTotalRootLayerScrollOffset(total_scroll_offset);

  if (apply_scroll_to) {
    ScrollTo(scroll_on_scroll_state_update_.value());
    scroll_on_scroll_state_update_.reset();
  }
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
BrowserViewRenderer::RootLayerStateAsValue(
    const gfx::PointF& total_scroll_offset,
    const gfx::SizeF& scrollable_size_dip) {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());

  state->SetDouble("total_scroll_offset.x", total_scroll_offset.x());
  state->SetDouble("total_scroll_offset.y", total_scroll_offset.y());

  state->SetDouble("max_scroll_offset_unscaled.x",
                   max_scroll_offset_unscaled_.x());
  state->SetDouble("max_scroll_offset_unscaled.y",
                   max_scroll_offset_unscaled_.y());

  state->SetDouble("scrollable_size_dip.width", scrollable_size_dip.width());
  state->SetDouble("scrollable_size_dip.height", scrollable_size_dip.height());

  state->SetDouble("page_scale_factor", page_scale_factor_);
  return std::move(state);
}

void BrowserViewRenderer::DidOverscroll(
    content::SynchronousCompositor* compositor,
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::Vector2dF& latest_overscroll_delta,
    const gfx::Vector2dF& current_fling_velocity) {
  if (compositor != compositor_)
    return;

  const float physical_pixel_scale = dip_scale_ * page_scale_factor_;
  if (accumulated_overscroll == latest_overscroll_delta)
    overscroll_rounding_error_ = gfx::Vector2dF();
  gfx::Vector2dF scaled_overscroll_delta =
      gfx::ScaleVector2d(latest_overscroll_delta, physical_pixel_scale);
  gfx::Vector2d rounded_overscroll_delta = gfx::ToRoundedVector2d(
      scaled_overscroll_delta + overscroll_rounding_error_);
  overscroll_rounding_error_ =
      scaled_overscroll_delta - rounded_overscroll_delta;
  gfx::Vector2dF fling_velocity_pixels =
      gfx::ScaleVector2d(current_fling_velocity, physical_pixel_scale);

  client_->DidOverscroll(rounded_overscroll_delta, fling_velocity_pixels,
                         begin_frame_source_->inside_begin_frame());
}

ui::TouchHandleDrawable* BrowserViewRenderer::CreateDrawable() {
  return client_->CreateDrawable();
}

void BrowserViewRenderer::CopyOutput(
    content::SynchronousCompositor* compositor,
    std::unique_ptr<viz::CopyOutputRequest> copy_request) {
  if (compositor != compositor_ || !hardware_enabled_)
    return;
  copy_requests_.emplace_back(std::move(copy_request));
  PostInvalidate(compositor_);
}

void BrowserViewRenderer::Invalidate() {
  if (compositor_)
    compositor_->DidInvalidate();
  PostInvalidate(compositor_);
}

void BrowserViewRenderer::ReturnResourcesFromViz(
    viz::FrameSinkId frame_sink_id,
    uint32_t layer_tree_frame_sink_id,
    std::vector<viz::ReturnedResource> resources) {
  ReturnUsedResources(std::move(resources), frame_sink_id,
                      layer_tree_frame_sink_id);
}

void BrowserViewRenderer::OnCompositorFrameTransitionDirectiveProcessed(
    viz::FrameSinkId frame_sink_id,
    uint32_t layer_tree_frame_sink_id,
    uint32_t sequence_id) {
  content::SynchronousCompositor* compositor = FindCompositor(frame_sink_id);
  if (compositor) {
    compositor->OnCompositorFrameTransitionDirectiveProcessed(
        layer_tree_frame_sink_id, sequence_id);
  }
}

void BrowserViewRenderer::OnInputEvent() {
  if (root_frame_sink_proxy_)
    root_frame_sink_proxy_->OnInputEvent();
}

void BrowserViewRenderer::AddBeginFrameCompletionCallback(
    base::OnceClosure callback) {
  begin_frame_source_->AddBeginFrameCompletionCallback(std::move(callback));
}

void BrowserViewRenderer::SetThreadIds(const std::vector<int32_t>& thread_ids) {
  renderer_thread_ids_ = thread_ids;
}

void BrowserViewRenderer::PostInvalidate(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT_INSTANT0("android_webview", "BrowserViewRenderer::PostInvalidate",
                       TRACE_EVENT_SCOPE_THREAD);
  if (compositor != compositor_)
    return;

  did_invalidate_since_last_draw_ = true;
  client_->PostInvalidate(
      RootBeginFrameSourceWebView::GetInstance()->inside_begin_frame());
}

bool BrowserViewRenderer::CompositeSW(SkCanvas* canvas, bool software_canvas) {
  DCHECK(compositor_);
  return compositor_->DemandDrawSw(canvas, software_canvas);
}

std::string BrowserViewRenderer::ToString() const {
  std::string str;
  base::StringAppendF(&str, "is_paused: %d ", is_paused_);
  base::StringAppendF(&str, "view_visible: %d ", view_visible_);
  base::StringAppendF(&str, "window_visible: %d ", window_visible_);
  base::StringAppendF(&str, "dip_scale: %f ", dip_scale_);
  base::StringAppendF(&str, "page_scale_factor: %f ", page_scale_factor_);
  base::StringAppendF(&str, "view size: %s ", size_.ToString().c_str());
  base::StringAppendF(&str, "attached_to_window: %d ", attached_to_window_);
  base::StringAppendF(&str,
                      "global visible rect: %s ",
                      last_on_draw_global_visible_rect_.ToString().c_str());
  base::StringAppendF(&str, "scroll_offset_unscaled: %s ",
                      scroll_offset_unscaled_.ToString().c_str());
  base::StringAppendF(&str,
                      "overscroll_rounding_error_: %s ",
                      overscroll_rounding_error_.ToString().c_str());
  base::StringAppendF(
      &str, "on_new_picture_enable: %d ", on_new_picture_enable_);
  base::StringAppendF(&str, "clear_view: %d ", clear_view_);
  return str;
}

void BrowserViewRenderer::SetBrowserIOThreadId(
    base::PlatformThreadId thread_id) {
  browser_io_thread_id_ = thread_id;
}

}  // namespace android_webview
