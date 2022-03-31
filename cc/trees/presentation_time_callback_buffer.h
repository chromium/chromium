// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CC_TREES_PRESENTATION_TIME_CALLBACK_BUFFER_H_
#define CC_TREES_PRESENTATION_TIME_CALLBACK_BUFFER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {

// Maintains a queue of callbacks and compositor frame times that we want to
// buffer until a relevant frame is presented.
//
// Callbacks are queued through |RegisterMainThreadPresentationCallbacks| and
// |RegisterCompositorPresentationCallbacks| if the callback is to be run on
// the main thread or compositor thread respectively.
//
// Once a frame is presented, users of this class can call
// |PopPendingCallbacks| to get their callbacks back. This class never runs
// callbacks itself so it's up to calling code to |PostTask| or call |Run()| as
// needed.
//
// This class is thread unsafe so concurrent access would require external
// synchronization. In practice, however, instances of this class are only used
// on the compositor thread even though some of the buffered callbacks are
// intended to be run on the renderer main thread.
//
// CC_EXPORT is only needed for testing.
class CC_EXPORT PresentationTimeCallbackBuffer {
 public:
  // Maximum expected buffer size for presentation callbacks. We generally
  // don't expect many frames waiting for a presentation feedback, hence we
  // don't expect many presentation callbacks waiting for a frame presentation.
  static constexpr size_t kMaxBufferSize = 60u;

  // TODO(crbug.com/1199373): Compositor thread callbacks are only run for
  // successful presentations and only need the presentation timestamp. On the
  // other hand, main thread callbacks can be run on both successful and failed
  // presentations and need a full `gfx::PresentationFeedback`. Conceptually,
  // main thread callbacks should only be run for successful presentations, too,
  // in which case the two callback signatures can be unified.
  using MainCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  using CompositorCallback = base::OnceCallback<void(base::TimeTicks)>;

  PresentationTimeCallbackBuffer();

  PresentationTimeCallbackBuffer(const PresentationTimeCallbackBuffer&) =
      delete;
  PresentationTimeCallbackBuffer(PresentationTimeCallbackBuffer&&);

  PresentationTimeCallbackBuffer& operator=(
      const PresentationTimeCallbackBuffer&) = delete;
  PresentationTimeCallbackBuffer& operator=(PresentationTimeCallbackBuffer&&);

  ~PresentationTimeCallbackBuffer();

  // Buffers the given |callbacks| in preparation for a GPU frame swap at or
  // after the given |frame_token|. Calling code posts these callbacks to the
  // main thread once they're popped.
  void RegisterMainThreadPresentationCallbacks(
      uint32_t frame_token,
      std::vector<MainCallback> callbacks);

  // Buffers the given |callbacks| in preparation for a GPU frame swap at or
  // after the given |frame_token|. Calling code invokes these callbacks on the
  // compositor thread once they're popped.
  void RegisterCompositorPresentationCallbacks(
      uint32_t frame_token,
      std::vector<CompositorCallback> callbacks);

  // Structured return value for |PopPendingCallbacks|. CC_EXPORT is only
  // needed for testing.
  struct CC_EXPORT PendingCallbacks {
    PendingCallbacks();

    PendingCallbacks(const PendingCallbacks&) = delete;
    PendingCallbacks(PendingCallbacks&&);

    PendingCallbacks& operator=(const PendingCallbacks&) = delete;
    PendingCallbacks& operator=(PendingCallbacks&&);

    ~PendingCallbacks();

    // Holds callbacks registered through
    // |RegisterMainThreadPresentationCallbacks|.
    std::vector<MainCallback> main_thread_callbacks;

    // Holds callbacks registered through
    // |RegisterCompositorPresentationCallbacks|.
    std::vector<CompositorCallback> compositor_thread_callbacks;
  };

  // Call this once the presentation for the given `frame_token` has completed.
  // Yields any pending callbacks that were registered against a frame token
  // that was less than or equal to the given `frame_token`. If `main_only` is
  // true, only callbacks for the main thread are returned. It is the caller's
  // responsibility to run the callbacks on the right threads/sequences.
  PendingCallbacks PopPendingCallbacks(uint32_t frame_token, bool main_only);

 private:
  // Stores information needed once we get a response for a particular
  // presentation token.
  struct FrameTokenInfo {
    explicit FrameTokenInfo(uint32_t token);
    FrameTokenInfo(const FrameTokenInfo&) = delete;
    FrameTokenInfo(FrameTokenInfo&&);
    FrameTokenInfo& operator=(const FrameTokenInfo&) = delete;
    FrameTokenInfo& operator=(FrameTokenInfo&&);
    ~FrameTokenInfo();

    // A |CompositorFrameMetadata::frame_token| that we use to associate
    // presentation feedback with the relevant compositor frame.
    uint32_t token;

    // The callbacks to send back to the main thread.
    std::vector<MainCallback> main_thread_callbacks;

    // The callbacks to invoke on the compositor thread.
    std::vector<CompositorCallback> compositor_thread_callbacks;
  };

  // Returns a reference to a |FrameTokenInfo| with the given |frame_token|.
  // The instance is created if necessary and occupies the appropriate place in
  // |frame_token_infos_|.
  FrameTokenInfo& GetOrMakeRegistration(uint32_t frame_token);

  // Queue of pending registrations ordered by |token|. We can use a deque
  // because we require callers to use non-decreasing tokens when registering.
  base::circular_deque<FrameTokenInfo> frame_token_infos_;

  // When DCHECK is enabled, check that instances of this class aren't being
  // used concurrently.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cc

#endif  // CC_TREES_PRESENTATION_TIME_CALLBACK_BUFFER_H_
