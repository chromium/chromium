// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/gvr_scheduler_delegate.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "chrome/browser/android/vr/gl_browser_interface.h"
#include "chrome/browser/android/vr/metrics_util_android.h"
#include "chrome/browser/android/vr/scoped_gpu_trace.h"
#include "chrome/browser/vr/scheduler_browser_renderer_interface.h"
#include "chrome/browser/vr/scheduler_ui_interface.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "content/public/common/content_features.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"

namespace {

// Default downscale factor for computing the recommended WebXR
// render_width/render_height from the 1:1 pixel mapped size. Using a rather
// aggressive downscale due to the high overhead of copying pixels
// twice before handing off to GVR. For comparison, the polyfill
// uses approximately 0.55 on a Pixel XL.
static constexpr float kRecommendedResolutionScale = 0.7;

// The scale factor for WebXR on devices that don't have shared buffer
// support. (Android N and earlier.)
static constexpr float kNoSharedBufferResolutionScale = 0.5;

constexpr int kWebVrInitialFrameTimeoutSeconds = 5;
constexpr int kWebVrSpinnerTimeoutSeconds = 2;

// Heuristic time limit to detect overstuffed GVR buffers for a
// >60fps capable web app.
constexpr base::TimeDelta kWebVrSlowAcquireThreshold = base::Milliseconds(2);

// If running too fast, allow dropping frames occasionally to let GVR catch up.
// Drop at most one frame in MaxDropRate.
constexpr int kWebVrUnstuffMaxDropRate = 7;

// Timeout for checking for the WebVR rendering GL fence. If the timeout is
// reached, yield to let other tasks execute before rechecking.
constexpr base::TimeDelta kWebVRFenceCheckTimeout = base::Microseconds(2000);

// Polling interval for checking for the WebVR rendering GL fence. Used as
// an alternative to kWebVRFenceCheckTimeout if the GPU workaround is active.
// The actual interval may be longer due to PostDelayedTask's resolution.
constexpr base::TimeDelta kWebVRFenceCheckPollInterval =
    base::Microseconds(500);

bool ValidateRect(const gfx::RectF& bounds) {
  // Bounds should be between 0 and 1, with positive width/height.
  // We simply clamp to [0,1], but still validate that the bounds are not NAN.
  return !std::isnan(bounds.width()) && !std::isnan(bounds.height()) &&
         !std::isnan(bounds.x()) && !std::isnan(bounds.y());
}

}  // namespace

namespace vr {

GvrSchedulerDelegate::GvrSchedulerDelegate(GlBrowserInterface* browser,
                                           SchedulerUiInterface* ui,
                                           gvr::GvrApi* gvr_api,
                                           GvrGraphicsDelegate* graphics,
                                           bool cardboard_gamepad,
                                           size_t sliding_time_size)
    : BaseSchedulerDelegate(ui,
                            kWebVrSpinnerTimeoutSeconds,
                            kWebVrInitialFrameTimeoutSeconds),
      browser_(browser),
      gvr_api_(gvr_api),
      cardboard_gamepad_(cardboard_gamepad),
      vsync_helper_(base::BindRepeating(&GvrSchedulerDelegate::OnVSync,
                                        base::Unretained(this))),
      graphics_(graphics),
      webvr_render_time_(sliding_time_size),
      webvr_js_time_(sliding_time_size),
      webvr_js_wait_time_(sliding_time_size) {
  if (cardboard_gamepad_) {
    browser_->ToggleCardboardGamepad(true);
  }
}

GvrSchedulerDelegate::~GvrSchedulerDelegate() {
  ClosePresentationBindings();
  webxr_.EndPresentation();
  if (webxr_use_shared_buffer_draw_) {
    std::vector<std::unique_ptr<device::WebXrSharedBuffer>> buffers =
        webxr_.TakeSharedBuffers();
    for (auto& buffer : buffers) {
      if (!buffer->mailbox_holder.mailbox.IsZero()) {
        DCHECK(mailbox_bridge_);
        DVLOG(2) << ": DestroySharedImage, mailbox="
                 << buffer->mailbox_holder.mailbox.ToDebugString();
        mailbox_bridge_->DestroySharedImage(buffer->mailbox_holder);
      }
    }
  }
}

void GvrSchedulerDelegate::SetBrowserRenderer(
    SchedulerBrowserRendererInterface* browser_renderer) {
  browser_renderer_ = browser_renderer;
}

void GvrSchedulerDelegate::AddInputSourceState(
    device::mojom::XRInputSourceStatePtr state) {
  input_states_.push_back(std::move(state));
}

void GvrSchedulerDelegate::OnPause() {
  vsync_helper_.CancelVSyncRequest();
  gvr_api_->PauseTracking();
  CancelWebXrFrameTimeout();
}

void GvrSchedulerDelegate::OnResume() {
  gvr_api_->RefreshViewerProfile();
  gvr_api_->ResumeTracking();
  OnVSync(base::TimeTicks::Now());
  ScheduleOrCancelWebVrFrameTimeout();
}

void GvrSchedulerDelegate::ConnectPresentingService(
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  ClosePresentationBindings();

  std::vector<device::mojom::XRViewPtr> views =
      device::gvr_utils::CreateViews(gvr_api_, nullptr /*pose*/);
  int width = 0;
  int height = 0;
  for (const auto& view : views) {
    width += view->viewport.width();
    height = std::max(height, view->viewport.height());
  }

  gfx::Size webxr_size(width, height);
  DVLOG(1) << __func__ << ": resize initial to " << webxr_size.width() << "x"
           << webxr_size.height();

  // Decide which transport mechanism we want to use. This sets
  // the webxr_use_* options as a side effect.
  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      GetWebXrFrameTransportOptions(options);

  if (webxr_use_shared_buffer_draw_) {
    // Create the mailbox bridge if it doesn't exist yet. We can continue
    // reusing the existing one if it does, its resources such as mailboxes
    // are still valid.
    if (!mailbox_bridge_)
      CreateSurfaceBridge(nullptr);
  } else {
    CreateOrResizeWebXrSurface(webxr_size);
  }

  auto submit_frame_sink = device::mojom::XRPresentationConnection::New();
  submit_frame_sink->client_receiver =
      submit_client_.BindNewPipeAndPassReceiver();
  submit_frame_sink->provider =
      presentation_receiver_.BindNewPipeAndPassRemote();
  submit_frame_sink->transport_options = std::move(transport_options);

  auto session = device::mojom::XRSession::New();
  session->data_provider = frame_data_receiver_.BindNewPipeAndPassRemote();
  session->submit_frame_sink = std::move(submit_frame_sink);

  // Currently, the initial filtering of supported devices happens on the
  // browser side (BrowserXRRuntimeImpl::SupportsFeature()), so if we have
  // reached this point, it is safe to assume that all requested features are
  // enabled.
  // TODO(https://crbug.com/995377): revisit the approach when the bug is fixed.
  session->enabled_features.insert(session->enabled_features.end(),
                                   options->required_features.begin(),
                                   options->required_features.end());
  session->enabled_features.insert(session->enabled_features.end(),
                                   options->optional_features.begin(),
                                   options->optional_features.end());

  DVLOG(3) << __func__ << ": options->required_features.size()="
           << options->required_features.size()
           << ", options->optional_features.size()="
           << options->optional_features.size()
           << ", session->enabled_features.size()="
           << session->enabled_features.size();

  session->device_config = device::mojom::XRSessionDeviceConfig::New();
  auto* config = session->device_config.get();

  config->views = std::move(views);
  config->supports_viewport_scaling = true;
  session->enviroment_blend_mode =
      device::mojom::XREnvironmentBlendMode::kOpaque;
  session->interaction_mode = device::mojom::XRInteractionMode::kScreenSpace;

  // This scalar will be applied in the renderer to the recommended render
  // target sizes. For WebVR it will always be applied, for WebXR it can be
  // overridden.
  if (base::AndroidHardwareBufferCompat::IsSupportAvailable()) {
    config->default_framebuffer_scale = kRecommendedResolutionScale;
  } else {
    config->default_framebuffer_scale = kNoSharedBufferResolutionScale;
  }

  if (CanSendWebXrVSync())
    ScheduleWebXrFrameTimeout();

  browser_->SendRequestPresentReply(std::move(session));
}

device::mojom::XRPresentationTransportOptionsPtr
GvrSchedulerDelegate::GetWebXrFrameTransportOptions(
    const device::mojom::XRRuntimeSessionOptionsPtr& options) {
  webxr_use_shared_buffer_draw_ = false;
  webxr_use_gpu_fence_ = false;

  // Use SharedBuffer if supported, otherwise fall back to GpuFence, or
  // ClientWait if that also isn't available.
  if (gl::GLFence::IsGpuFenceSupported()) {
    webxr_use_gpu_fence_ = true;
    if (base::AndroidHardwareBufferCompat::IsSupportAvailable()) {
      webxr_use_shared_buffer_draw_ = true;
    }
  }

  // Identify the synchronization method used for debugging purposes.
  // (This corresponds to the retired XRRenderPath metric.)
  DVLOG(1) << __func__
           << ": use_shared_buffer_draw=" << webxr_use_shared_buffer_draw_
           << " use_gpu_fence=" << webxr_use_gpu_fence_;

  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();
  // Only set boolean options that we need. Default is false, and we should be
  // able to safely ignore ones that our implementation doesn't care about.
  transport_options->wait_for_transfer_notification = true;
  if (webxr_use_shared_buffer_draw_) {
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::DRAW_INTO_TEXTURE_MAILBOX;
    DCHECK(webxr_use_gpu_fence_);
    transport_options->wait_for_gpu_fence = true;
  } else {
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::SUBMIT_AS_MAILBOX_HOLDER;
    transport_options->wait_for_transfer_notification = true;
    if (webxr_use_gpu_fence_) {
      transport_options->wait_for_gpu_fence = true;
    } else {
      transport_options->wait_for_render_notification = true;
    }
  }
  graphics_->set_webxr_use_shared_buffer_draw(webxr_use_shared_buffer_draw_);
  return transport_options;
}

void GvrSchedulerDelegate::CreateOrResizeWebXrSurface(const gfx::Size& size) {
  if (!graphics_->CreateOrResizeWebXrSurface(
          size,
          base::BindRepeating(&GvrSchedulerDelegate::OnWebXrFrameAvailable,
                              weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }
  if (!mailbox_bridge_) {
    CreateSurfaceBridge(graphics_->webxr_surface_texture());
  } else if (graphics_->webxr_surface_texture()) {
    // Need to resize only if we have surface.
    mailbox_bridge_->ResizeSurface(size.width(), size.height());
  }
}

void GvrSchedulerDelegate::OnGpuProcessConnectionReady() {
  DVLOG(1) << __func__;
  CHECK(mailbox_bridge_);

  DCHECK(!webxr_.HaveAnimatingFrame());

  // See if we can send a VSync.
  webxr_.NotifyMailboxBridgeReady();
  WebXrTryStartAnimatingFrame();
}

void GvrSchedulerDelegate::CreateSurfaceBridge(
    gl::SurfaceTexture* surface_texture) {
  DCHECK(!mailbox_bridge_);
  DCHECK(!webxr_.mailbox_bridge_ready());
  mailbox_bridge_ = std::make_unique<webxr::MailboxToSurfaceBridgeImpl>();
  if (surface_texture)
    mailbox_bridge_->CreateSurface(surface_texture);
  mailbox_bridge_->CreateAndBindContextProvider(
      base::BindOnce(&GvrSchedulerDelegate::OnGpuProcessConnectionReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GvrSchedulerDelegate::OnWebXrFrameAvailable() {
  // This is called each time a frame that was drawn on the WebVR Surface
  // arrives on the SurfaceTexture.

  // This event should only occur in response to a SwapBuffers from
  // an incoming SubmitFrame call.
  DCHECK(!pending_frames_.empty()) << ": Frame arrived before SubmitFrame";

  // LIFECYCLE: we should have exactly one pending frame. This is true
  // even after exiting a session with a not-yet-surfaced frame.
  DCHECK_EQ(pending_frames_.size(), 1U);

  int frame_index = pending_frames_.front();
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  pending_frames_.pop();

  graphics_->OnWebXrFrameAvailable();

  // LIFECYCLE: we should be in processing state.
  DCHECK(webxr_.HaveProcessingFrame());
  device::WebXrFrame* processing_frame = webxr_.GetProcessingFrame();

  // Frame should be locked. Unlock it.
  DCHECK(processing_frame->state_locked);
  processing_frame->state_locked = false;

  if (ShouldDrawWebVr() && !processing_frame->recycle_once_unlocked) {
    DCHECK_EQ(processing_frame->index, frame_index);
    DrawFrame(frame_index, base::TimeTicks::Now());
  } else {
    // Silently consume a frame if we don't want to draw it. This can happen
    // due to an active exclusive UI such as a permission prompt, or after
    // exiting a presentation session when a pending frame arrives late.
    DVLOG(1) << __func__ << ": discarding frame, UI is active";
    WebXrCancelProcessingFrameAfterTransfer();
    // We're no longer in processing state, unblock pending processing frames.
    webxr_.TryDeferredProcessing();
  }
}

void GvrSchedulerDelegate::WebXrCancelProcessingFrameAfterTransfer() {
  DVLOG(2) << __func__;
  DCHECK(webxr_.HaveProcessingFrame());
  bool did_recycle = webxr_.RecycleProcessingFrameIfPossible();
  DCHECK(did_recycle);
  if (submit_client_) {
    // We've already sent the transferred notification.
    // Just report rendering complete.
    WebVrSendRenderNotification(false);
  }
}

void GvrSchedulerDelegate::ScheduleOrCancelWebVrFrameTimeout() {
  // TODO(mthiesse): We should also timeout after the initial frame to prevent
  // bad experiences, but we have to be careful to handle things like splash
  // screens correctly. For now just ensure we receive a first frame.
  if (webxr_frames_received() > 0) {
    CancelWebXrFrameTimeout();
    return;
  }
  if (CanSendWebXrVSync() && submit_client_)
    ScheduleWebXrFrameTimeout();
}

bool GvrSchedulerDelegate::CanSendWebXrVSync() const {
  return true;
}

void GvrSchedulerDelegate::OnVSync(base::TimeTicks frame_time) {
  TRACE_EVENT0("gpu", __func__);
  // Create a synthetic VSync trace event for the reported last-VSync time. Use
  // this specific type since it appears to be the only one which supports
  // supplying a timestamp different from the current time, which is useful //
  // since we seem to be >1ms behind the vsync time when we receive this call.
  //
  // See third_party/catapult/tracing/tracing/extras/vsync/vsync_auditor.html
  auto args = std::make_unique<base::trace_event::TracedValue>();
  args->SetDouble("frame_time_us",
                  (frame_time - base::TimeTicks()).InMicrosecondsF());
  TRACE_EVENT_INSTANT1("viz", "DisplayScheduler::BeginFrame",
                       TRACE_EVENT_SCOPE_THREAD, "args", std::move(args));

  vsync_helper_.RequestVSync();

  // The controller update logic is a bit complicated. We poll controller state
  // on every VSync to ensure that the "exit VR" button stays responsive even
  // for uncooperative apps. This has the side effect of filling in the
  // input_states_ variable which gets attached to the frame data sent via
  // the next SendVSync. However, fetching controller data needs a head pose
  // to ensure the elbow model works right.
  //
  // If we're about to run SendVSync now, fetch a fresh head pose now and use
  // that for both the controller update and for SendVSync. If not, just use
  // a recent-ish head pose for the controller update, or get a new pose if
  // that isn't available.

  webxr_vsync_pending_ = true;
  pending_time_ = frame_time;
  bool can_animate = WebVrCanAnimateFrame(true);

  if (ShouldDrawWebVr()) {
    gfx::Transform head_mat;
    device::mojom::VRPosePtr pose;
    // We need a new head pose if we're about to start a new animating frame,
    // or if we don't have a current animating frame from which we could
    // get a recent one. We don't want to fall back to an identity transform
    // since that would cause controller position glitches, especially for
    // 6DoF headsets.
    if (can_animate || !webxr_.HaveAnimatingFrame()) {
      pose = GetHeadPose(&head_mat);
    } else {
      // Get the most-recently-used head pose from the current animating frame.
      // (The condition above guarantees that we have one.)
      head_mat = webxr_.GetAnimatingFrame()->head_pose;
    }

    browser_renderer_->ProcessControllerInputForWebXr(head_mat, frame_time);

    if (can_animate)
      SendVSync(std::move(pose), head_mat);
  } else {
    DrawFrame(-1, frame_time);
    if (can_animate)
      SendVSyncWithNewHeadPose();
  }
}

void GvrSchedulerDelegate::DrawFrame(int16_t frame_index,
                                     base::TimeTicks current_time) {
  TRACE_EVENT1("gpu", "Vr.DrawFrame", "frame", frame_index);
  DCHECK(browser_renderer_);
  bool is_webxr_frame = frame_index >= 0;
  if (!webxr_delayed_gvr_submit_.IsCancelled()) {
    // The last submit to GVR didn't complete, we have an acquired frame. This
    // is normal when exiting WebVR, in that case we just want to reuse the
    // frame. It's not supposed to happen during WebVR presentation.
    DCHECK(!is_webxr_frame)
        << "Unexpected WebXR DrawFrame during acquired frame";
    webxr_delayed_gvr_submit_.Cancel();
    browser_renderer_->DrawBrowserFrame(current_time);
  }

  if (!ShouldDrawWebVr()) {
    // We're in a WebVR session, but don't want to draw WebVR frames, i.e.
    // because UI has taken over for a permissions prompt. Do state cleanup if
    // needed.
    if (webxr_.HaveAnimatingFrame() &&
        webxr_.GetAnimatingFrame()->deferred_start_processing) {
      // We have an animating frame. Cancel it if it's waiting to start
      // processing. If not, keep it to receive the incoming SubmitFrame.
      DVLOG(1) << __func__ << ": cancel waiting WebVR frame, UI is active";
      WebXrCancelAnimatingFrame();
    }
  }

  // From this point on, the current frame is either a pure UI frame
  // (frame_index==-1), or a WebVR frame (frame_index >= 0). If it's a WebVR
  // frame, it must be the current processing frame. Careful, we may still have
  // a processing frame in UI mode that couldn't be cancelled yet. For example
  // when showing a permission prompt, ShouldDrawWebVr() may have become false
  // in the time between SubmitFrame and OnWebXrFrameAvailable or
  // OnWebVRTokenSignaled. In that case we continue handling the current frame
  // as a WebVR frame. Also, WebVR frames can still have overlay UI drawn on top
  // of them.
  DCHECK(!is_webxr_frame || webxr_.HaveProcessingFrame());

  graphics_->UpdateViewports();

  if (is_webxr_frame) {
    UpdatePendingBounds(frame_index);
    if (!graphics_->ResizeForWebXr())
      return;
  } else {
    graphics_->ResizeForBrowser();
  }

  if (!graphics_->AcquireGvrFrame(frame_index))
    return;

  if (is_webxr_frame) {
    // When using async reprojection, we need to know which pose was
    // used in the WebVR app for drawing this frame and supply it when
    // submitting. Technically we don't need a pose if not reprojecting,
    // but keeping it uninitialized seems likely to cause problems down
    // the road. Copying it is cheaper than fetching a new one.
    DCHECK(webxr_.HaveProcessingFrame());
    browser_renderer_->DrawWebXrFrame(current_time,
                                      webxr_.GetProcessingFrame()->head_pose);
  } else {
    browser_renderer_->DrawBrowserFrame(current_time);
  }
}

void GvrSchedulerDelegate::UpdatePendingBounds(int16_t frame_index) {
  // Process all pending_bounds_ changes targeted for before this frame, being
  // careful of wrapping frame indices.
  static constexpr unsigned max = std::numeric_limits<
      device::WebXrPresentationState::FrameIndexType>::max();
  static_assert(max > device::WebXrPresentationState::kWebXrFrameCount * 2,
                "To detect wrapping, kPoseRingBufferSize must be smaller "
                "than half of next_frame_index_ range.");
  while (!pending_bounds_.empty()) {
    uint16_t index = pending_bounds_.front().first;
    // If index is less than the frame_index it's possible we've wrapped, so we
    // extend the range and 'un-wrap' to account for this.
    if (index < frame_index)
      index += max + 1;
    // If the pending bounds change is for an upcoming frame within our buffer
    // size, wait to apply it. Otherwise, apply it immediately. This guarantees
    // that even if we miss many frames, the queue can't fill up with stale
    // bounds.
    if (index > frame_index &&
        index <= frame_index + device::WebXrPresentationState::kWebXrFrameCount)
      break;

    const WebVrBounds& bounds = pending_bounds_.front().second;
    graphics_->SetWebXrBounds(bounds);
    DVLOG(1) << __func__ << ": resize from pending_bounds to "
             << bounds.source_size.width() << "x"
             << bounds.source_size.height();
    CreateOrResizeWebXrSurface(bounds.source_size);
    pending_bounds_.pop();
  }
}

void GvrSchedulerDelegate::SubmitDrawnFrame(FrameType frame_type,
                                            const gfx::Transform& head_pose) {
  std::unique_ptr<gl::GLFenceEGL> fence;
  if (frame_type == kWebXrFrame && graphics_->DoesSurfacelessRendering()) {
    webxr_.GetProcessingFrame()->time_copied = base::TimeTicks::Now();
    if (webxr_use_gpu_fence_) {
      // Continue with submit once the previous frame's GL fence signals that
      // it is done rendering. This avoids blocking in GVR's Submit. Fence is
      // null for the first frame, in that case the fence wait is skipped.
      if (webvr_prev_frame_completion_fence_ &&
          webvr_prev_frame_completion_fence_->HasCompleted()) {
        // The fence had already signaled. We can get the signaled time from the
        // fence and submit immediately.
        AddWebVrRenderTimeEstimate(
            webvr_prev_frame_completion_fence_->GetStatusChangeTime());
        webvr_prev_frame_completion_fence_.reset();
      } else {
        fence = std::move(webvr_prev_frame_completion_fence_);
      }
    } else {
      // Continue with submit once a GL fence signals that current drawing
      // operations have completed.
      fence = gl::GLFenceEGL::Create();
    }
  }
  if (fence) {
    webxr_delayed_gvr_submit_.Reset(
        base::BindOnce(&GvrSchedulerDelegate::DrawFrameSubmitWhenReady,
                       base::Unretained(this)));
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(webxr_delayed_gvr_submit_.callback(),
                                  frame_type, head_pose, std::move(fence)));
  } else {
    // Continue with submit immediately.
    DrawFrameSubmitNow(frame_type, head_pose);
  }
}

void GvrSchedulerDelegate::DrawFrameSubmitWhenReady(
    FrameType frame_type,
    const gfx::Transform& head_pose,
    std::unique_ptr<gl::GLFenceEGL> fence) {
  TRACE_EVENT1("gpu", __func__, "frame_type", frame_type);
  DVLOG(2) << __func__ << ": frame_type=" << frame_type;
  bool use_polling = webxr_.mailbox_bridge_ready() &&
                     mailbox_bridge_->IsGpuWorkaroundEnabled(
                         gpu::DONT_USE_EGLCLIENTWAITSYNC_WITH_TIMEOUT);
  if (fence) {
    if (!use_polling) {
      // Use wait-with-timeout to find out as soon as possible when rendering
      // is complete.
      fence->ClientWaitWithTimeoutNanos(
          kWebVRFenceCheckTimeout.InNanoseconds());
    }
    if (!fence->HasCompleted()) {
      webxr_delayed_gvr_submit_.Reset(
          base::BindOnce(&GvrSchedulerDelegate::DrawFrameSubmitWhenReady,
                         base::Unretained(this)));
      if (use_polling) {
        // Poll the fence status at a short interval. This burns some CPU, but
        // avoids excessive waiting on devices which don't handle timeouts
        // correctly. Downside is that the completion status is only detected
        // with a delay of up to one polling interval.
        task_runner()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(webxr_delayed_gvr_submit_.callback(), frame_type,
                           head_pose, std::move(fence)),
            kWebVRFenceCheckPollInterval);
      } else {
        task_runner()->PostTask(
            FROM_HERE, base::BindOnce(webxr_delayed_gvr_submit_.callback(),
                                      frame_type, head_pose, std::move(fence)));
      }
      return;
    }
  }

  if (fence && webxr_use_gpu_fence_) {
    // We were waiting for the fence, so the time now is the actual
    // finish time for the previous frame's rendering.
    AddWebVrRenderTimeEstimate(base::TimeTicks::Now());
  }

  webxr_delayed_gvr_submit_.Cancel();
  DrawFrameSubmitNow(frame_type, head_pose);
}

void GvrSchedulerDelegate::AddWebVrRenderTimeEstimate(
    base::TimeTicks fence_complete_time) {
  if (!webxr_.HaveRenderingFrame())
    return;

  device::WebXrFrame* rendering_frame = webxr_.GetRenderingFrame();
  base::TimeTicks prev_js_submit = rendering_frame->time_js_submit;
  if (webxr_use_gpu_fence_ && !prev_js_submit.is_null() &&
      !fence_complete_time.is_null()) {
    webvr_render_time_.AddSample(fence_complete_time - prev_js_submit);
  }
}

void GvrSchedulerDelegate::DrawFrameSubmitNow(FrameType frame_type,
                                              const gfx::Transform& head_pose) {
  TRACE_EVENT1("gpu", "Vr.SubmitFrameNow", "frame_type", frame_type);
  {
    std::unique_ptr<ScopedGpuTrace> browser_gpu_trace;
    if (gl::GLFence::IsGpuFenceSupported() && frame_type == kUiFrame) {
      // This fence instance is created for the tracing side effect. Insert it
      // before GVR submit. Then replace the previous instance below after GVR
      // submit completes, at which point the previous fence (if any) should be
      // complete. Doing this in two steps avoids a race condition - a fence
      // that was inserted after Submit may not be complete yet when the next
      // Submit finishes.
      browser_gpu_trace =
          std::make_unique<ScopedGpuTrace>("Vr.PostSubmitDrawOnGpu");
    }
    graphics_->SubmitToGvr(head_pose);

    if (browser_gpu_trace) {
      // Replacing the previous instance will record the trace result for
      // the previous instance.
      DCHECK(!gpu_trace_ || gpu_trace_->fence()->HasCompleted());
      gpu_trace_ = std::move(browser_gpu_trace);
    }
  }

  // At this point, ShouldDrawWebVr and webvr_frame_processing_ may have become
  // false for a WebVR frame. Ignore the ShouldDrawWebVr status to ensure we
  // send render notifications while paused for exclusive UI mode. Skip the
  // steps if we lost the processing state, that means presentation has ended.
  if (frame_type == kWebXrFrame && webxr_.HaveProcessingFrame()) {
    // Report rendering completion to the Renderer so that it's permitted to
    // submit a fresh frame. We could do this earlier, as soon as the frame
    // got pulled off the transfer surface, but that results in overstuffed
    // buffers.
    WebVrSendRenderNotification(true);

    base::TimeTicks pose_time = webxr_.GetProcessingFrame()->time_pose;
    base::TimeTicks js_submit_time =
        webxr_.GetProcessingFrame()->time_js_submit;
    webvr_js_time_.AddSample(js_submit_time - pose_time);
    if (!webxr_use_gpu_fence_) {
      // Estimate render time from wallclock time, we waited for the pre-submit
      // render fence to signal.
      base::TimeTicks now = base::TimeTicks::Now();
      webvr_render_time_.AddSample(now - js_submit_time);
    }

    if (webxr_.HaveRenderingFrame()) {
      webxr_.EndFrameRendering();
    }
    webxr_.TransitionFrameProcessingToRendering();
  }

  // After saving the timestamp, fps will be available via GetFPS().
  // TODO(vollick): enable rendering of this framerate in a HUD.
  vr_ui_fps_meter_.AddFrame(base::TimeTicks::Now());
  DVLOG(2) << "fps: " << vr_ui_fps_meter_.GetFPS();
  TRACE_COUNTER1("gpu", "VR UI FPS", vr_ui_fps_meter_.GetFPS());

  if (frame_type == kWebXrFrame) {
    // We finished processing a frame, this may make pending WebVR
    // work eligible to proceed.
    webxr_.TryDeferredProcessing();
  }

  if (ShouldDrawWebVr()) {
    // See if we can animate a new WebVR frame. Intentionally using
    // ShouldDrawWebVr here since we also want to run this check after
    // UI frames, i.e. transitioning from transient UI to WebVR.
    WebXrTryStartAnimatingFrame();
  }
}

bool GvrSchedulerDelegate::WebVrCanAnimateFrame(bool is_from_onvsync) {
  // This check needs to be first to ensure that we start the WebVR
  // first-frame timeout on presentation start.
  bool can_send_webvr_vsync = CanSendWebXrVSync();
  if (!webxr_.last_ui_allows_sending_vsync && can_send_webvr_vsync) {
    // We will start sending vsync to the WebVR page, so schedule the incoming
    // frame timeout.
    ScheduleOrCancelWebVrFrameTimeout();
  }
  webxr_.last_ui_allows_sending_vsync = can_send_webvr_vsync;
  if (!can_send_webvr_vsync) {
    DVLOG(2) << __func__ << ": waiting for can_send_webvr_vsync";
    return false;
  }

  // If we want to send vsync-aligned frames, we only allow animation to start
  // when called from OnVSync, so if we're called from somewhere else we can
  // skip all the other checks. Legacy Cardboard mode (not surfaceless) doesn't
  // use vsync aligned frames.
  if (graphics_->DoesSurfacelessRendering() && !is_from_onvsync) {
    DVLOG(3) << __func__ << ": waiting for onvsync (vsync aligned)";
    return false;
  }

  if (get_frame_data_callback_.is_null()) {
    DVLOG(2) << __func__ << ": waiting for get_frame_data_callback_";
    return false;
  }

  if (!webxr_vsync_pending_) {
    DVLOG(2) << __func__ << ": waiting for pending_vsync (too fast)";
    return false;
  }

  // If we already have a JS frame that's animating, don't send another one.
  // This check depends on the Renderer calling either SubmitFrame or
  // SubmitFrameMissing for each animated frame.
  if (webxr_.HaveAnimatingFrame()) {
    DVLOG(2) << __func__
             << ": waiting for current animating frame to start processing";
    return false;
  }
  if (webxr_use_shared_buffer_draw_ && !webxr_.mailbox_bridge_ready()) {
    // For exclusive scheduling, we need the mailbox bridge before the first
    // frame so that we can place a sync token. For shared buffer draw, we
    // need it to set up buffers before starting client rendering.
    DVLOG(2) << __func__ << ": waiting for mailbox_bridge_ready";
    return false;
  }
  if (webxr_use_shared_buffer_draw_ &&
      graphics_->webxr_surface_size().IsEmpty()) {
    // For shared buffer draw, wait for a nonzero size before creating
    // the shared buffer for use as a drawing destination.
    DVLOG(2) << __func__ << ": waiting for nonzero size";
    return false;
  }

  // Keep the heuristic tests last since they update a trace counter, they
  // should only be run if the remaining criteria are already met. There's no
  // corresponding WebVrTryStartAnimating call for this, the retries happen
  // via OnVSync.
  bool still_rendering = WebVrHasSlowRenderingFrame();
  bool overstuffed = WebVrHasOverstuffedBuffers();
  TRACE_COUNTER2("gpu", "WebVR frame skip", "still rendering", still_rendering,
                 "overstuffed", overstuffed);
  if (still_rendering || overstuffed) {
    DVLOG(2) << __func__ << ": waiting for backlogged frames,"
             << " still_rendering=" << still_rendering
             << " overstuffed=" << overstuffed;
    return false;
  }

  DVLOG(2) << __func__ << ": ready to animate frame";
  return true;
}

void GvrSchedulerDelegate::WebXrTryStartAnimatingFrame() {
  // This method is only used outside OnVSync, so the is_from_onvsync argument
  // to WebVrCanAnimateFrame is always false. OnVSync calls SendVSync directly
  // if needed, bypassing this method, so that it can supply a specific pose.
  if (WebVrCanAnimateFrame(false)) {
    SendVSyncWithNewHeadPose();
  }
}

bool GvrSchedulerDelegate::ShouldDrawWebVr() {
  return webxr_frames_received() > 0;
}

void GvrSchedulerDelegate::WebXrCancelAnimatingFrame() {
  DVLOG(2) << __func__;
  webxr_.RecycleUnusedAnimatingFrame();
  if (submit_client_) {
    // We haven't written to the Surface yet. Mark as transferred and rendered.
    submit_client_->OnSubmitFrameTransferred(true);
    WebVrSendRenderNotification(false);
  }
}

void GvrSchedulerDelegate::WebVrSendRenderNotification(bool was_rendered) {
  if (!submit_client_)
    return;

  TRACE_EVENT0("gpu", __func__);
  if (webxr_use_gpu_fence_) {
    // Renderer is waiting for a frame-separating GpuFence.

    if (was_rendered) {
      // Save a fence for local completion checking.
      webvr_prev_frame_completion_fence_ =
          gl::GLFenceAndroidNativeFenceSync::CreateForGpuFence();
    }

    // Create a local GpuFence and pass it to the Renderer via IPC.
    std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
    std::unique_ptr<gfx::GpuFence> gpu_fence = gl_fence->GetGpuFence();
    submit_client_->OnSubmitFrameGpuFence(
        gpu_fence->GetGpuFenceHandle().Clone());
  } else {
    // Renderer is waiting for the previous frame to render, unblock it now.
    submit_client_->OnSubmitFrameRendered();
  }
}

bool GvrSchedulerDelegate::WebVrHasSlowRenderingFrame() {
  // Disable heuristic for traditional render path where we submit completed
  // frames.
  if (!webxr_use_gpu_fence_)
    return false;

  base::TimeDelta frame_interval = vsync_helper_.DisplayVSyncInterval();
  base::TimeDelta mean_render_time =
      webvr_render_time_.GetAverageOrDefault(frame_interval);

  // Check estimated completion of the rendering frame, that's two frames back.
  // It might not exist, i.e. for the first couple of frames when starting
  // presentation, or if the app failed to submit a frame in its rAF loop.
  // Also, AddWebVrRenderTimeEstimate zeroes the submit time once the rendered
  // frame is complete. In all of those cases, we don't need to wait for render
  // completion.
  if (webxr_.HaveRenderingFrame() && webxr_.HaveProcessingFrame()) {
    base::TimeTicks prev_js_submit = webxr_.GetRenderingFrame()->time_js_submit;
    base::TimeDelta mean_js_time = webvr_js_time_.GetAverage();
    base::TimeDelta mean_js_wait = webvr_js_wait_time_.GetAverage();
    base::TimeDelta prev_render_time_left =
        mean_render_time - (base::TimeTicks::Now() - prev_js_submit);
    // We don't want the next animating frame to arrive too early. Estimated
    // time-to-submit is the net JavaScript time, not counting time spent
    // waiting. JS is blocked from submitting if the rendering frame (two
    // frames back) is not complete yet, so there's no point submitting earlier
    // than that. There's also a processing frame (one frame back), so we have
    // at least a VSync interval spare time after that. Aim for submitting 3/4
    // of a VSync interval after the rendering frame completes to keep a bit of
    // safety margin. We're currently scheduling at VSync granularity, so skip
    // this VSync if we'd arrive a full VSync interval early.
    if (mean_js_time - mean_js_wait + frame_interval <
        prev_render_time_left + frame_interval * 3 / 4) {
      return true;
    }
  }
  return false;
}

bool GvrSchedulerDelegate::WebVrHasOverstuffedBuffers() {
  base::TimeDelta frame_interval = vsync_helper_.DisplayVSyncInterval();
  base::TimeDelta mean_render_time =
      webvr_render_time_.GetAverageOrDefault(frame_interval);

  if (webvr_unstuff_ratelimit_frames_ > 0) {
    --webvr_unstuff_ratelimit_frames_;
  } else if (graphics_->GetAcquireTimeAverage() >= kWebVrSlowAcquireThreshold &&
             mean_render_time < frame_interval) {
    // This is a fast app with average render time less than the frame
    // interval. If GVR acquire is slow, that means its internal swap chain was
    // already full when we tried to give it the next frame. We can skip a
    // SendVSync to drain one frame from the GVR queue. That should reduce
    // latency by one frame.
    webvr_unstuff_ratelimit_frames_ = kWebVrUnstuffMaxDropRate;
    return true;
  }
  return false;
}

device::mojom::VRPosePtr GvrSchedulerDelegate::GetHeadPose(
    gfx::Transform* head_mat_out) {
  int64_t prediction_nanos = GetPredictedFrameTime().InNanoseconds();

  TRACE_EVENT_BEGIN0("gpu", "GvrSchedulerDelegate::GetVRPosePtrWithNeckModel");
  device::mojom::VRPosePtr pose =
      device::GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_, head_mat_out,
                                                     prediction_nanos);
  TRACE_EVENT_END0("gpu", "GvrSchedulerDelegate::GetVRPosePtrWithNeckModel");

  return pose;
}

void GvrSchedulerDelegate::SendVSyncWithNewHeadPose() {
  gfx::Transform head_mat;

  device::mojom::VRPosePtr pose = GetHeadPose(&head_mat);
  SendVSync(std::move(pose), head_mat);
}

void GvrSchedulerDelegate::SendVSync(device::mojom::VRPosePtr pose,
                                     const gfx::Transform& head_mat) {
  DCHECK(!get_frame_data_callback_.is_null());
  DCHECK(webxr_vsync_pending_);

  // Mark the VSync as consumed.
  webxr_vsync_pending_ = false;

  device::mojom::XRFrameDataPtr frame_data = device::mojom::XRFrameData::New();

  // The internal frame index is an uint8_t that generates a wrapping 0.255
  // frame number. We store it in an int16_t to match mojo APIs, and to avoid
  // it appearing as a char in debug logs.
  frame_data->frame_id = webxr_.StartFrameAnimating();
  DVLOG(2) << __func__ << " frame=" << frame_data->frame_id;

  if (webxr_use_shared_buffer_draw_) {
    WebXrPrepareSharedBuffer();

    CHECK(webxr_.mailbox_bridge_ready());
    CHECK(webxr_.HaveAnimatingFrame());
    device::WebXrSharedBuffer* buffer =
        webxr_.GetAnimatingFrame()->shared_buffer.get();
    DCHECK(buffer);
    DCHECK(buffer->mailbox_holder.sync_token.verified_flush());
    frame_data->buffer_holder = buffer->mailbox_holder;
  }

  // Process all events. Check for ones we wish to react to.
  gvr::Event last_event;
  while (gvr_api_->PollEvent(&last_event)) {
    frame_data->mojo_space_reset |= last_event.type == GVR_EVENT_RECENTER;
  }

  frame_data->views = device::gvr_utils::CreateViews(gvr_api_, pose.get());

  TRACE_EVENT0("gpu", "GvrSchedulerDelegate::XRInput");
  frame_data->input_state = std::move(input_states_);

  frame_data->mojo_from_viewer = std::move(pose);

  device::WebXrFrame* frame = webxr_.GetAnimatingFrame();
  frame->head_pose = head_mat;
  frame->time_pose = base::TimeTicks::Now();

  frame_data->time_delta = pending_time_ - base::TimeTicks();

  TRACE_EVENT0("gpu", "GvrSchedulerDelegate::RunCallback");
  std::move(get_frame_data_callback_).Run(std::move(frame_data));
}

void GvrSchedulerDelegate::WebXrPrepareSharedBuffer() {
  TRACE_EVENT0("gpu", __func__);
  const auto& webxr_surface_size = graphics_->webxr_surface_size();
  DVLOG(2) << __func__ << ": size=" << webxr_surface_size.width() << "x"
           << webxr_surface_size.height();
  CHECK(webxr_.mailbox_bridge_ready());
  CHECK(webxr_.HaveAnimatingFrame());

  device::WebXrSharedBuffer* buffer;
  if (webxr_.GetAnimatingFrame()->shared_buffer) {
    buffer = webxr_.GetAnimatingFrame()->shared_buffer.get();
  } else {
    // Create buffer and do one-time setup for resources that stay valid after
    // size changes.
    webxr_.GetAnimatingFrame()->shared_buffer =
        std::make_unique<device::WebXrSharedBuffer>();
    buffer = webxr_.GetAnimatingFrame()->shared_buffer.get();

    // Local resources
    glGenTextures(1, &buffer->local_texture);
  }

  if (webxr_surface_size != buffer->size) {
    // Don't need the image for zero copy mode.
    WebXrCreateOrResizeSharedBufferImage(buffer, webxr_surface_size);

    // Save the size to avoid expensive reallocation next time.
    buffer->size = webxr_surface_size;
  }
}

void GvrSchedulerDelegate::WebXrCreateOrResizeSharedBufferImage(
    device::WebXrSharedBuffer* buffer,
    const gfx::Size& size) {
  TRACE_EVENT0("gpu", __func__);
  // Unbind previous image (if any).
  if (!buffer->mailbox_holder.mailbox.IsZero()) {
    DVLOG(2) << ": DestroySharedImage, mailbox="
             << buffer->mailbox_holder.mailbox.ToDebugString();
    mailbox_bridge_->DestroySharedImage(buffer->mailbox_holder);
  }

  DVLOG(2) << __func__ << ": width=" << size.width()
           << " height=" << size.height();
  // Remove reference to previous image (if any).
  buffer->local_eglimage.reset();

  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  const gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;

  gfx::GpuMemoryBufferId kBufferId(webxr_.next_memory_buffer_id++);
  buffer->gmb = gpu::GpuMemoryBufferImplAndroidHardwareBuffer::Create(
      kBufferId, size, format, usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback());

  uint32_t shared_image_usage = gpu::SHARED_IMAGE_USAGE_SCANOUT |
                                gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                gpu::SHARED_IMAGE_USAGE_GLES2;
  buffer->mailbox_holder = mailbox_bridge_->CreateSharedImage(
      buffer->gmb.get(), gfx::ColorSpace(), shared_image_usage);
  DVLOG(2) << ": CreateSharedImage, mailbox="
           << buffer->mailbox_holder.mailbox.ToDebugString();

  base::android::ScopedHardwareBufferHandle ahb =
      buffer->gmb->CloneHandle().android_hardware_buffer;
  auto egl_image = gpu::CreateEGLImageFromAHardwareBuffer(ahb.get());
  if (!egl_image.is_valid()) {
    DLOG(WARNING) << __func__ << ": ERROR: failed to initialize image!";
    // Exiting VR is a bit drastic, but this error shouldn't occur under normal
    // operation. If it's an issue in practice, look into other recovery
    // options such as shutting down the WebVR/WebXR presentation session.
    browser_->ForceExitVr();
    return;
  }
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer->local_texture);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image.get());
  buffer->local_eglimage = std::move(egl_image);
}

base::TimeDelta GvrSchedulerDelegate::GetPredictedFrameTime() {
  base::TimeDelta frame_interval = vsync_helper_.DisplayVSyncInterval();
  // If we aim to submit at vsync, that frame will start scanning out
  // one vsync later. Add a half frame to split the difference between
  // left and right eye.
  base::TimeDelta js_time = webvr_js_time_.GetAverageOrDefault(frame_interval);
  base::TimeDelta render_time =
      webvr_render_time_.GetAverageOrDefault(frame_interval);
  base::TimeDelta overhead_time = frame_interval * 3 / 2;
  base::TimeDelta expected_frame_time = js_time + render_time + overhead_time;
  TRACE_COUNTER2("gpu", "WebVR frame time (ms)", "javascript",
                 js_time.InMilliseconds(), "rendering",
                 render_time.InMilliseconds());
  graphics_->RecordFrameTimeTraces();
  TRACE_COUNTER1("gpu", "WebVR pose prediction (ms)",
                 expected_frame_time.InMilliseconds());
  return expected_frame_time;
}

void GvrSchedulerDelegate::ClosePresentationBindings() {
  CancelWebXrFrameTimeout();
  submit_client_.reset();
  if (!get_frame_data_callback_.is_null()) {
    // When this Presentation provider is going away we have to respond to
    // pending callbacks, so instead of providing a VSync, tell the requester
    // the connection is closing.
    std::move(get_frame_data_callback_).Run(nullptr);
  }
  presentation_receiver_.reset();
  frame_data_receiver_.reset();
}

void GvrSchedulerDelegate::GetFrameData(
    device::mojom::XRFrameDataRequestOptionsPtr,
    device::mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("gpu", __func__);
  if (!get_frame_data_callback_.is_null()) {
    DLOG(WARNING) << ": previous get_frame_data_callback_ was not used yet";
    frame_data_receiver_.ReportBadMessage(
        "Requested VSync before waiting for response to previous request.");
    ClosePresentationBindings();
    return;
  }

  get_frame_data_callback_ = std::move(callback);
  WebXrTryStartAnimatingFrame();
}

void GvrSchedulerDelegate::SubmitFrameMissing(
    int16_t frame_index,
    const gpu::SyncToken& sync_token) {
  TRACE_EVENT1("gpu", "GvrSchedulerDelegate::SubmitWebVRFrame", "frame",
               frame_index);

  if (!IsSubmitFrameExpected(frame_index))
    return;

  if (webxr_use_shared_buffer_draw_) {
    // Renderer didn't submit a frame. Stash the sync token in the mailbox
    // holder, so that we use the dependency before destroying or recycling the
    // shared image.
    device::WebXrSharedBuffer* buffer =
        webxr_.GetAnimatingFrame()->shared_buffer.get();
    DCHECK(buffer);
    DCHECK(sync_token.verified_flush());
    buffer->mailbox_holder.sync_token = sync_token;
  } else {
    // Renderer didn't submit a frame. Wait for the sync token to ensure
    // that any mailbox_bridge_ operations for the next frame happen after
    // whatever drawing the Renderer may have done before exiting.
    if (webxr_.mailbox_bridge_ready())
      mailbox_bridge_->WaitSyncToken(sync_token);
  }

  DVLOG(2) << __func__ << ": recycle unused animating frame";
  DCHECK(webxr_.HaveAnimatingFrame());
  webxr_.RecycleUnusedAnimatingFrame();
}

void GvrSchedulerDelegate::SubmitFrame(int16_t frame_index,
                                       const gpu::MailboxHolder& mailbox,
                                       base::TimeDelta time_waited) {
  if (!SubmitFrameCommon(frame_index, time_waited))
    return;

  webxr_.ProcessOrDefer(
      base::BindOnce(&GvrSchedulerDelegate::ProcessWebVrFrameFromMailbox,
                     weak_ptr_factory_.GetWeakPtr(), frame_index, mailbox));
}

void GvrSchedulerDelegate::SubmitFrameDrawnIntoTexture(
    int16_t frame_index,
    const gpu::SyncToken& sync_token,
    base::TimeDelta time_waited) {
  if (!SubmitFrameCommon(frame_index, time_waited))
    return;

  if (webxr_use_shared_buffer_draw_) {
    // Renderer submitted a frame. Stash the sync token in the mailbox
    // holder, so that we use the dependency before destroying or recycling the
    // shared image.
    device::WebXrSharedBuffer* buffer =
        webxr_.GetAnimatingFrame()->shared_buffer.get();
    DCHECK(buffer);
    DCHECK(sync_token.verified_flush());
    buffer->mailbox_holder.sync_token = sync_token;
  } else {
    presentation_receiver_.ReportBadMessage(
        "SubmitFrameDrawnIntoTexture called while using the wrong transport "
        "mode");
    ClosePresentationBindings();
    return;
  }

  webxr_.ProcessOrDefer(
      base::BindOnce(&GvrSchedulerDelegate::ProcessWebVrFrameFromGMB,
                     weak_ptr_factory_.GetWeakPtr(), frame_index, sync_token));
}

void GvrSchedulerDelegate::UpdateLayerBounds(int16_t frame_index,
                                             const gfx::RectF& left_bounds,
                                             const gfx::RectF& right_bounds,
                                             const gfx::Size& source_size) {
  if (!ValidateRect(left_bounds) || !ValidateRect(right_bounds)) {
    presentation_receiver_.ReportBadMessage(
        "UpdateLayerBounds called with invalid bounds");
    ClosePresentationBindings();
    return;
  }

  if (frame_index >= 0 && !webxr_.HaveAnimatingFrame()) {
    // The optional UpdateLayerBounds call must happen before SubmitFrame.
    presentation_receiver_.ReportBadMessage(
        "UpdateLayerBounds called without animating frame");
    ClosePresentationBindings();
    return;
  }

  WebVrBounds webxr_bounds(left_bounds, right_bounds, source_size);
  if (frame_index < 0) {
    graphics_->SetWebXrBounds(webxr_bounds);
    CreateOrResizeWebXrSurface(source_size);
    pending_bounds_ = {};
  } else {
    pending_bounds_.emplace(frame_index, webxr_bounds);
  }
}

bool GvrSchedulerDelegate::IsSubmitFrameExpected(int16_t frame_index) {
  // submit_client_ could be null when we exit presentation, if there were
  // pending SubmitFrame messages queued.  XRSessionClient::OnExitPresent
  // will clean up state in blink, so it doesn't wait for
  // OnSubmitFrameTransferred or OnSubmitFrameRendered. Similarly,
  // the animating frame state is cleared when exiting presentation,
  // and we should ignore a leftover queued SubmitFrame.
  if (!submit_client_.get() || !webxr_.HaveAnimatingFrame())
    return false;

  device::WebXrFrame* animating_frame = webxr_.GetAnimatingFrame();

  if (animating_frame->index != frame_index) {
    DVLOG(1) << __func__ << ": wrong frame index, got " << frame_index
             << ", expected " << animating_frame->index;
    presentation_receiver_.ReportBadMessage(
        "SubmitFrame called with wrong frame index");
    ClosePresentationBindings();
    return false;
  }

  // Frame looks valid.
  return true;
}

bool GvrSchedulerDelegate::SubmitFrameCommon(int16_t frame_index,
                                             base::TimeDelta time_waited) {
  TRACE_EVENT1("gpu", "GvrSchedulerDelegate::SubmitWebVRFrame", "frame",
               frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;

  if (!IsSubmitFrameExpected(frame_index))
    return false;

  // If we get here, treat as a valid submit.
  DCHECK(webxr_.HaveAnimatingFrame());
  device::WebXrFrame* animating_frame = webxr_.GetAnimatingFrame();

  animating_frame->time_js_submit = base::TimeTicks::Now();

  // The JavaScript wait time is supplied externally and not trustworthy. Clamp
  // to a reasonable range to avoid math errors.
  if (time_waited.is_negative())
    time_waited = base::TimeDelta();
  if (time_waited > base::Seconds(1))
    time_waited = base::Seconds(1);
  webvr_js_wait_time_.AddSample(time_waited);
  TRACE_COUNTER1("gpu", "WebVR JS wait (ms)",
                 webvr_js_wait_time_.GetAverage().InMilliseconds());

  // Always tell the UI that we have a new WebVR frame, so that it can
  // transition the UI state to "presenting" and cancel any pending timeouts.
  // That's a prerequisite for ShouldDrawWebVr to become true, which is in turn
  // required to complete a processing frame.
  OnNewWebXrFrame();

  if (!ShouldDrawWebVr()) {
    DVLOG(1) << "Discarding received frame, UI is active";
    WebXrCancelAnimatingFrame();
    return false;
  }

  return true;
}

void GvrSchedulerDelegate::ProcessWebVrFrameFromMailbox(
    int16_t frame_index,
    const gpu::MailboxHolder& mailbox) {
  TRACE_EVENT0("gpu", __func__);

  // LIFECYCLE: pending_frames_ should be empty when there's no processing
  // frame. It gets one element here, and then is emptied again before leaving
  // processing state. Swapping twice on a Surface without calling
  // updateTexImage in between can lose frames, so don't draw+swap if we
  // already have a pending frame we haven't consumed yet.
  DCHECK(pending_frames_.empty());

  // LIFECYCLE: We shouldn't have gotten here unless mailbox_bridge_ is ready.
  DCHECK(webxr_.mailbox_bridge_ready());

  // Don't allow any state changes for this processing frame until it
  // arrives on the Surface. See OnWebXrFrameAvailable.
  DCHECK(webxr_.HaveProcessingFrame());
  webxr_.GetProcessingFrame()->state_locked = true;

  // We don't do any scaling here, so we can just pass an identity transform.
  bool swapped =
      mailbox_bridge_->CopyMailboxToSurfaceAndSwap(mailbox, gfx::Transform());
  DCHECK(swapped);
  // Tell OnWebXrFrameAvailable to expect a new frame to arrive on
  // the SurfaceTexture, and save the associated frame index.
  pending_frames_.emplace(frame_index);

  // LIFECYCLE: we should have a pending frame now.
  DCHECK_EQ(pending_frames_.size(), 1U);

  // Notify the client that we're done with the mailbox so that the underlying
  // image is eligible for destruction.
  submit_client_->OnSubmitFrameTransferred(true);

  // Unblock the next animating frame in case it was waiting for this
  // one to start processing.
  WebXrTryStartAnimatingFrame();
}

void GvrSchedulerDelegate::OnWebXrTokenSignaled(
    int16_t frame_index,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;

  // Ignore if not processing a frame. This can happen on exiting presentation.
  if (!webxr_.HaveProcessingFrame())
    return;

  webxr_.GetProcessingFrame()->gvr_handoff_fence =
      gl::GLFence::CreateFromGpuFence(*gpu_fence);

  base::TimeTicks now = base::TimeTicks::Now();
  DrawFrame(frame_index, now);
}

void GvrSchedulerDelegate::ProcessWebVrFrameFromGMB(
    int16_t frame_index,
    const gpu::SyncToken& sync_token) {
  TRACE_EVENT0("gpu", __func__);

  mailbox_bridge_->CreateGpuFence(
      sync_token, base::BindOnce(&GvrSchedulerDelegate::OnWebXrTokenSignaled,
                                 weak_ptr_factory_.GetWeakPtr(), frame_index));

  // Unblock the next animating frame in case it was waiting for this
  // one to start processing.
  WebXrTryStartAnimatingFrame();
}

void GvrSchedulerDelegate::GetEnvironmentIntegrationProvider(
    mojo::PendingAssociatedReceiver<
        device::mojom::XREnvironmentIntegrationProvider> environment_provider) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  frame_data_receiver_.ReportBadMessage(
      "Environment integration is not supported.");
}

}  // namespace vr
