// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/browser_view_renderer.h"

#include <memory>
#include <utility>

#include "android_webview/browser/browser_view_renderer_client.h"
#include "android_webview/browser/compositor_frame_consumer.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

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
    return data ? data->bvr_ : NULL;
  }

 private:
  BrowserViewRenderer* bvr_;
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
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
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
      offscreen_pre_raster_(false) {}

BrowserViewRenderer::~BrowserViewRenderer() {
  DCHECK(compositor_map_.empty());
  SetCurrentCompositorFrameConsumer(nullptr);
  while (compositor_frame_consumers_.size()) {
    RemoveCompositorFrameConsumer(*compositor_frame_consumers_.begin());
  }
}

void BrowserViewRenderer::SetCurrentCompositorFrameConsumer(
    CompositorFrameConsumer* compositor_frame_consumer) {
  if (compositor_frame_consumer == current_compositor_frame_consumer_) {
    return;
  }
  current_compositor_frame_consumer_ = compositor_frame_consumer;
  if (current_compositor_frame_consumer_) {
    compositor_frame_consumers_.insert(current_compositor_frame_consumer_);
    current_compositor_frame_consumer_->SetCompositorFrameProducer(this);
    OnParentDrawConstraintsUpdated(current_compositor_frame_consumer_);
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
  // Just set the memory limit to 0 and drop all tiles. This will be reset to
  // normal levels in the next DrawGL call.
  if (!offscreen_pre_raster_)
    ReleaseHardware();
}

void BrowserViewRenderer::UpdateMemoryPolicy() {
  if (!compositor_) {
    return;
  }

  if (!hardware_enabled_) {
    compositor_->SetMemoryPolicy(0u);
    return;
  }

  size_t bytes_limit = 0u;
  if (g_memory_override_in_bytes) {
    bytes_limit = static_cast<size_t>(g_memory_override_in_bytes);
  } else {
    gfx::Rect interest_rect =
        offscreen_pre_raster_ || external_draw_constraints_.is_layer
            ? gfx::Rect(size_)
            : last_on_draw_global_visible_rect_;
    size_t width = interest_rect.width();
    size_t height = interest_rect.height();
    bytes_limit = kMemoryMultiplier * kBytesPerPixel * width * height;
    // Round up to a multiple of kMemoryAllocationStep.
    bytes_limit =
        (bytes_limit / kMemoryAllocationStep + 1) * kMemoryAllocationStep;
  }

  compositor_->SetMemoryPolicy(bytes_limit);
}

content::SynchronousCompositor* BrowserViewRenderer::FindCompositor(
    const CompositorID& compositor_id) const {
  const auto& compositor_iterator = compositor_map_.find(compositor_id);
  if (compositor_iterator == compositor_map_.end())
    return nullptr;

  return compositor_iterator->second;
}

void BrowserViewRenderer::PrepareToDraw(const gfx::Vector2d& scroll,
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

  current_compositor_frame_consumer_->InitializeHardwareDrawIfNeededOnUI();

  if (!CanOnDraw()) {
    return false;
  }

  current_compositor_frame_consumer_->SetScrollOffsetOnUI(
      last_on_draw_scroll_offset_);
  hardware_enabled_ = true;

  external_draw_constraints_ =
      current_compositor_frame_consumer_->GetParentDrawConstraintsOnUI();

  ReturnResourceFromParent(current_compositor_frame_consumer_);
  UpdateMemoryPolicy();

  gfx::Transform transform_for_tile_priority =
      external_draw_constraints_.transform;

  gfx::Rect viewport_rect_for_tile_priority =
      ComputeViewportRectForTilePriority();

  scoped_refptr<content::SynchronousCompositor::FrameFuture> future =
      compositor_->DemandDrawHwAsync(size_, viewport_rect_for_tile_priority,
                                     transform_for_tile_priority);
  std::unique_ptr<ChildFrame> child_frame = std::make_unique<ChildFrame>(
      std::move(future), compositor_id_,
      viewport_rect_for_tile_priority.IsEmpty(), transform_for_tile_priority,
      offscreen_pre_raster_, external_draw_constraints_.is_layer);

  ReturnUnusedResource(
      current_compositor_frame_consumer_->SetFrameOnUI(std::move(child_frame)));
  return true;
}

gfx::Rect BrowserViewRenderer::ComputeViewportRectForTilePriority() {
  // If the WebView is on a layer, WebView does not know what transform is
  // applied onto the layer so global visible rect does not make sense here.
  // In this case, just use the surface rect for tiling.
  // Leave viewport_rect_for_tile_priority empty if offscreen_pre_raster_ is on.
  gfx::Rect viewport_rect_for_tile_priority;

  if (!offscreen_pre_raster_ && !external_draw_constraints_.is_layer) {
    viewport_rect_for_tile_priority = last_on_draw_global_visible_rect_;
  }
  return viewport_rect_for_tile_priority;
}

void BrowserViewRenderer::ReturnedResourceAvailable(
    CompositorFrameConsumer* compositor_frame_consumer) {
  DCHECK(compositor_frame_consumers_.count(compositor_frame_consumer));
  ReturnResourceFromParent(compositor_frame_consumer);
}

void BrowserViewRenderer::OnParentDrawConstraintsUpdated(
    CompositorFrameConsumer* compositor_frame_consumer) {
  DCHECK(compositor_frame_consumer);
  if (compositor_frame_consumer != current_compositor_frame_consumer_)
    return;
  ParentCompositorDrawConstraints new_constraints =
      current_compositor_frame_consumer_->GetParentDrawConstraintsOnUI();
  if (external_draw_constraints_ == new_constraints)
    return;
  external_draw_constraints_ = new_constraints;
  PostInvalidate(compositor_);
  UpdateMemoryPolicy();
}

void BrowserViewRenderer::RemoveCompositorFrameConsumer(
    CompositorFrameConsumer* compositor_frame_consumer) {
  DCHECK(compositor_frame_consumers_.count(compositor_frame_consumer));
  compositor_frame_consumers_.erase(compositor_frame_consumer);
  if (current_compositor_frame_consumer_ == compositor_frame_consumer) {
    SetCurrentCompositorFrameConsumer(nullptr);
  }

  // At this point the compositor frame consumer has to hand back all resources
  // to the child compositor.
  compositor_frame_consumer->DeleteHardwareRendererOnUI();
  ReturnUncommittedFrames(
      compositor_frame_consumer->PassUncommittedFrameOnUI());
  ReturnResourceFromParent(compositor_frame_consumer);
  compositor_frame_consumer->SetCompositorFrameProducer(nullptr);
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
      FindCompositor(child_frame->compositor_id);
  if (compositor && !resources.empty())
    compositor->ReturnResources(child_frame->layer_tree_frame_sink_id,
                                std::move(resources));
}

void BrowserViewRenderer::ReturnResourceFromParent(
    CompositorFrameConsumer* compositor_frame_consumer) {
  CompositorFrameConsumer::ReturnedResourcesMap returned_resource_map;
  compositor_frame_consumer->SwapReturnedResourcesOnUI(&returned_resource_map);
  for (auto& pair : returned_resource_map) {
    CompositorID compositor_id = pair.first;
    content::SynchronousCompositor* compositor = FindCompositor(compositor_id);
    std::vector<viz::ReturnedResource> resources;
    resources.swap(pair.second.resources);

    if (compositor && !resources.empty()) {
      compositor->ReturnResources(pair.second.layer_tree_frame_sink_id,
                                  resources);
    }

    has_rendered_frame_ = true;
  }
}

bool BrowserViewRenderer::OnDrawSoftware(SkCanvas* canvas) {
  return CanOnDraw() && CompositeSW(canvas);
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
  SkCanvas* rec_canvas = recorder.beginRecording(width, height, NULL, 0);
  if (compositor_) {
    {
      // Reset scroll back to the origin, will go back to the old
      // value when scroll_reset is out of scope.
      base::AutoReset<gfx::Vector2dF> scroll_reset(&scroll_offset_unscaled_,
                                                   gfx::Vector2dF());
      compositor_->DidChangeRootLayerScrollOffset(
          gfx::ScrollOffset(scroll_offset_unscaled_));
      CompositeSW(rec_canvas);
    }
    compositor_->DidChangeRootLayerScrollOffset(
        gfx::ScrollOffset(scroll_offset_unscaled_));
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
    UpdateMemoryPolicy();
  }
}

void BrowserViewRenderer::SetIsPaused(bool paused) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetIsPaused",
                       TRACE_EVENT_SCOPE_THREAD,
                       "paused",
                       paused);
  is_paused_ = paused;
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
    UpdateMemoryPolicy();
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
    UpdateMemoryPolicy();
}

void BrowserViewRenderer::OnDetachedFromWindow() {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::OnDetachedFromWindow");
  attached_to_window_ = false;
  ReleaseHardware();
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
  for (auto* compositor_frame_consumer : compositor_frame_consumers_) {
    ReturnUncommittedFrames(
        compositor_frame_consumer->PassUncommittedFrameOnUI());
    ReturnResourceFromParent(compositor_frame_consumer);
    DCHECK(compositor_frame_consumer->ReturnedResourcesEmptyOnUI());
  }
  hardware_enabled_ = false;
  has_rendered_frame_ = false;
  UpdateMemoryPolicy();
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

gfx::Rect BrowserViewRenderer::GetScreenRect() const {
  return gfx::Rect(client_->GetLocationOnScreen(), size_);
}

void BrowserViewRenderer::DidInitializeCompositor(
    content::SynchronousCompositor* compositor,
    int process_id,
    int routing_id) {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::DidInitializeCompositor",
                       TRACE_EVENT_SCOPE_THREAD);
  DCHECK(compositor);
  CompositorID compositor_id(process_id, routing_id);
  // This assumes that a RenderViewHost has at most 1 synchronous compositor
  // througout its lifetime.
  DCHECK(compositor_map_.count(compositor_id) == 0);
  compositor_map_[compositor_id] = compositor;

  // At this point, the RVHChanged event for the new RVH that contains the
  // |compositor| might have been fired already, in which case just set the
  // current compositor with the new compositor.
  if (compositor_id.Equals(compositor_id_))
    SetActiveCompositor(compositor);
}

void BrowserViewRenderer::DidDestroyCompositor(
    content::SynchronousCompositor* compositor,
    int process_id,
    int routing_id) {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::DidDestroyCompositor",
                       TRACE_EVENT_SCOPE_THREAD);
  CompositorID compositor_id(process_id, routing_id);
  DCHECK(compositor_map_.count(compositor_id));
  if (compositor_ == compositor) {
    compositor_ = nullptr;
  }

  compositor_map_.erase(compositor_id);
}

void BrowserViewRenderer::SetActiveCompositorID(
    const CompositorID& compositor_id) {
  compositor_id_ = compositor_id;
  SetActiveCompositor(FindCompositor(compositor_id));
}

void BrowserViewRenderer::SetActiveCompositor(
    content::SynchronousCompositor* compositor) {
  if (compositor_ == compositor)
    return;

  if (compositor_)
    compositor_->SetMemoryPolicy(0u);
  compositor_ = compositor;
  if (compositor_) {
    UpdateMemoryPolicy();
    compositor_->DidBecomeActive();
  }
}

void BrowserViewRenderer::SetDipScale(float dip_scale) {
  dip_scale_ = dip_scale;
  CHECK_GT(dip_scale_, 0.f);
}

gfx::Vector2d BrowserViewRenderer::max_scroll_offset() const {
  DCHECK_GT(dip_scale_, 0.f);
  float scale = content::IsUseZoomForDSFEnabled()
                    ? page_scale_factor_
                    : dip_scale_ * page_scale_factor_;
  return gfx::ToCeiledVector2d(
      gfx::ScaleVector2d(max_scroll_offset_unscaled_, scale));
}

void BrowserViewRenderer::ScrollTo(const gfx::Vector2d& scroll_offset) {
  gfx::Vector2d max_offset = max_scroll_offset();
  gfx::Vector2dF scroll_offset_unscaled;
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
    compositor_->DidChangeRootLayerScrollOffset(
        gfx::ScrollOffset(scroll_offset_unscaled));
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
    const gfx::Vector2dF& scroll_offset_unscaled) {
  if (scroll_offset_unscaled_ == scroll_offset_unscaled)
    return;
  scroll_offset_unscaled_ = scroll_offset_unscaled;

  gfx::Vector2d max_offset = max_scroll_offset();
  gfx::Vector2d scroll_offset;
  // For an explanation as to why this is done this way see the comment in
  // BrowserViewRenderer::ScrollTo.
  if (max_scroll_offset_unscaled_.x()) {
    scroll_offset.set_x(
        gfx::ToRoundedInt((scroll_offset_unscaled.x() * max_offset.x()) /
                          max_scroll_offset_unscaled_.x()));
  }

  if (max_scroll_offset_unscaled_.y()) {
    scroll_offset.set_y(
        gfx::ToRoundedInt((scroll_offset_unscaled.y() * max_offset.y()) /
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
    const gfx::Vector2dF& total_scroll_offset,
    const gfx::Vector2dF& total_max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  if (compositor != compositor_)
    return;

  gfx::SizeF scrollable_size_dip = scrollable_size;
  if (content::IsUseZoomForDSFEnabled())
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
  }
  SetTotalRootLayerScrollOffset(total_scroll_offset);
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
BrowserViewRenderer::RootLayerStateAsValue(
    const gfx::Vector2dF& total_scroll_offset,
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

  client_->DidOverscroll(rounded_overscroll_delta, fling_velocity_pixels);
}

ui::TouchHandleDrawable* BrowserViewRenderer::CreateDrawable() {
  return client_->CreateDrawable();
}

void BrowserViewRenderer::PostInvalidate(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT_INSTANT0("android_webview", "BrowserViewRenderer::PostInvalidate",
                       TRACE_EVENT_SCOPE_THREAD);
  if (compositor != compositor_)
    return;

  client_->PostInvalidate();
}

bool BrowserViewRenderer::CompositeSW(SkCanvas* canvas) {
  DCHECK(compositor_);
  return compositor_->DemandDrawSw(canvas);
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

}  // namespace android_webview
