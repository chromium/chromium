// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CC_TREES_PRESENTATION_TIME_CALLBACK_BUFFER_H_
#define CC_TREES_PRESENTATION_TIME_CALLBACK_BUFFER_H_

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/sequence_checker.h"
#include "cc/trees/layer_tree_host.h"

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
  using CallbackType = LayerTreeHost::PresentationTimeCallback;

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
      std::vector<CallbackType> callbacks);

  // Buffers the given |callbacks| in preparation for a GPU frame swap at or
  // after the given |frame_token|. Calling code invokes these callbacks on the
  // compositor thread once they're popped.
  void RegisterCompositorPresentationCallbacks(
      uint32_t frame_token,
      std::vector<CallbackType> callbacks);

  // The given |frame_time| is associated with the given |frame_token| and will
  // be exposed through |PopPendingCallbacks| if there is an exact frame token
  // match. Note that it is an error to register distinct |frame_time|s against
  // the same |frame_token|.
  void RegisterFrameTime(uint32_t frame_token, base::TimeTicks frame_time);

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
    std::vector<CallbackType> main_thread_callbacks;

    // Holds callbacks registered through
    // |RegisterCompositorPresentationCallbacks|.
    std::vector<CallbackType> compositor_thread_callbacks;

    // Note: calling code needs to test against frame_time.is_null() because
    // frame_time is not always defined. See |PopPendingCallbacks|.
    base::TimeTicks frame_time;
  };

  // Call this once the presentation for the given |frame_token| has completed.
  // Yields any pending callbacks that were registered against a frame token
  // that was less than or equal to the given |frame_token|. It is the caller's
  // responsibility to run the callbacks on the right threads/sequences. When
  // the given |frame_token| is an exact match to a registered entry,
  // |frame_time| will be set to the frame time supplied through
  // |RegisterFrameTime|. Otherwise, |frame_time| will be default constructed
  // and should not be used. Calling code can assume |frame_time| is meaningful
  // iff frame_time.is_null() returns false.
  PendingCallbacks PopPendingCallbacks(uint32_t frame_token);

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

    // A copy of the |frame_time| from the |BeginFrameArgs| associated with
    // frame. Useful for tracking latency between frame requests and frame
    // presentations.
    base::TimeTicks frame_time;

    // The callbacks to send back to the main thread.
    std::vector<CallbackType> main_thread_callbacks;

    // The callbacks to invoke on the compositor thread.
    std::vector<CallbackType> compositor_thread_callbacks;
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
