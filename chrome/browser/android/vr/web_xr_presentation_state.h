// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_WEB_XR_PRESENTATION_STATE_H_
#define CHROME_BROWSER_ANDROID_VR_WEB_XR_PRESENTATION_STATE_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace gl {
class GLFence;
class GLImageEGL;
}  // namespace gl

namespace gpu {
class GpuMemoryBufferImplAndroidHardwareBuffer;
}  // namespace gpu

namespace vr {
// WebVR/WebXR frames go through a three-stage pipeline: Animating, Processing,
// and Rendering. There's also an Idle state used as the starting state before
// Animating and ending state after Rendering.
//
// The stages can overlap, but we enforce that there isn't more than one
// frame in a given non-Idle state at any one time.
//
//       <- GetFrameData
//   Idle
//       SendVSync
//   Animating
//       <- UpdateLayerBounds (optional)
//       <- GetFrameData
//       <- SubmitFrame
//       ProcessWebVrFrame
//   Processing
//       <- OnWebVrFrameAvailable
//       DrawFrame
//       DrawFrameSubmitWhenReady
//       <= poll prev_frame_completion_fence_
//       DrawFrameSubmitNow
//   Rendering
//       <= prev_frame_completion_fence_ signals
//       DrawFrameSubmitNow (of next frame)
//   Idle
//
// Note that the frame is considered to still be in "Animating" state until
// ProcessWebVrFrame is called. If the current processing frame isn't done yet
// at the time the incoming SubmitFrame arrives, we defer ProcessWebVrFrame
// until that finishes.
//
// The renderer may call SubmitFrameMissing instead of SubmitFrame. In that
// case, the frame transitions from Animating back to Idle.
//
//       <- GetFrameData
//   Idle
//       SendVSync
//   Animating
//       <- UpdateLayerBounds (optional)
//       <- GetFrameData
//       <- SubmitFrameMissing
//   Idle

struct WebXrSharedBuffer {
  WebXrSharedBuffer();
  ~WebXrSharedBuffer();

  gfx::Size size = {0, 0};

  // Shared GpuMemoryBuffer
  std::unique_ptr<gpu::GpuMemoryBufferImplAndroidHardwareBuffer> gmb;

  // Resources in the remote GPU process command buffer context
  gpu::MailboxHolder mailbox_holder;

  // Resources in the local GL context
  uint32_t local_texture = 0;
  // This refptr keeps the image alive while processing a frame. That's
  // required because it owns underlying resources, and must still be
  // alive when the mailbox texture backed by this image is used.
  scoped_refptr<gl::GLImageEGL> local_glimage;
};

struct WebXrFrame {
  WebXrFrame();
  ~WebXrFrame();

  bool IsValid();
  void Recycle();

  // If true, this frame cannot change state until unlocked. Used to mark
  // processing frames for the critical stage from drawing to Surface until
  // they arrive in OnWebVRFrameAvailable. See also recycle_once_unlocked.
  bool state_locked = false;

  // Start of elements that need to be reset on Recycle

  int16_t index = -1;

  // Set on an animating frame if it is waiting for being able to transition
  // to processing state.
  base::OnceClosure deferred_start_processing;

  // Set if a frame recycle failed due to being locked. The client should check
  // this after unlocking it and retry recycling it at that time.
  bool recycle_once_unlocked = false;

  std::unique_ptr<gl::GLFence> gvr_handoff_fence;

  // End of elements that need to be reset on Recycle

  base::TimeTicks time_pose;
  base::TimeTicks time_js_submit;
  base::TimeTicks time_copied;
  gfx::Transform head_pose;

  // In SharedBuffer mode, keep a swap chain.
  std::unique_ptr<WebXrSharedBuffer> shared_buffer;

  DISALLOW_COPY_AND_ASSIGN(WebXrFrame);
};

class WebXrPresentationState {
 public:
  // WebXR frames use an arbitrary sequential ID to help catch logic errors
  // involving out-of-order frames. We use an 8-bit unsigned counter, wrapping
  // from 255 back to 0. Elsewhere we use -1 to indicate a non-WebXR frame, so
  // most internal APIs use int16_t to ensure that they can store a full
  // -1..255 value range.
  using FrameIndexType = uint8_t;

  // We have at most one frame animating, one frame being processed,
  // and one frame tracked after submission to GVR.
  static constexpr int kWebXrFrameCount = 3;

  WebXrPresentationState();
  ~WebXrPresentationState();

  // State transitions for normal flow
  FrameIndexType StartFrameAnimating();
  void TransitionFrameAnimatingToProcessing();
  void TransitionFrameProcessingToRendering();
  void EndFrameRendering();

  // Shuts down a presentation session. This will recycle any
  // animating or rendering frame. A processing frame cannot be
  // recycled if its state is locked, it will be recycled later
  // once the state unlocks.
  void EndPresentation();

  // Variant transitions, if Renderer didn't call SubmitFrame,
  // or if we want to discard an unwanted incoming frame.
  void RecycleUnusedAnimatingFrame();
  bool RecycleProcessingFrameIfPossible();

  void ProcessOrDefer(base::OnceClosure callback);
  // Call this after state changes that could result in CanProcessFrame
  // becoming true.
  void TryDeferredProcessing();

  bool HaveAnimatingFrame() const { return animating_frame_; }
  WebXrFrame* GetAnimatingFrame();
  bool HaveProcessingFrame() const { return processing_frame_; }
  WebXrFrame* GetProcessingFrame();
  bool HaveRenderingFrame() const { return rendering_frame_; }
  WebXrFrame* GetRenderingFrame();

  bool mailbox_bridge_ready() { return mailbox_bridge_ready_; }
  void NotifyMailboxBridgeReady() { mailbox_bridge_ready_ = true; }

  // Extracts the shared buffers from all frames, resetting said frames to an
  // invalid state.
  // This is intended for resource cleanup, after EndPresentation was called.
  std::vector<std::unique_ptr<WebXrSharedBuffer>> TakeSharedBuffers();

  // Used by WebVrCanAnimateFrame() to detect when ui_->CanSendWebVrVSync()
  // transitions from false to true, as part of starting the incoming frame
  // timeout.
  bool last_ui_allows_sending_vsync = false;

  // GpuMemoryBuffer creation needs a buffer ID. We don't really care about
  // this, but try to keep it unique to avoid confusion.
  int next_memory_buffer_id = 0;

 private:
  // Checks if we're in a valid state for processing the current animating
  // frame. Invalid states include mailbox_bridge_ready_ being false, or an
  // already existing processing frame that's not done yet.
  bool CanProcessFrame() const;
  std::unique_ptr<WebXrFrame> frames_storage_[kWebXrFrameCount];

  // Index of the next animating WebXR frame.
  FrameIndexType next_frame_index_ = 0;

  WebXrFrame* animating_frame_ = nullptr;
  WebXrFrame* processing_frame_ = nullptr;
  WebXrFrame* rendering_frame_ = nullptr;
  base::queue<WebXrFrame*> idle_frames_;

  bool mailbox_bridge_ready_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebXrPresentationState);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_WEB_XR_PRESENTATION_STATE_H_
