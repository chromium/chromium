// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CC_TREES_PRESENTATION_TIME_CALLBACK_BUFFER_H_
#define CC_TREES_PRESENTATION_TIME_CALLBACK_BUFFER_H_

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "components/viz/common/frame_timing_details.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {

// Maintains a queue of callbacks and compositor frame times that we want to
// buffer until a relevant frame is presented.
//
// Callbacks are queued through `RegisterMainThreadCallbacks()` or
// `RegisterMainThreadSuccessfulCallbacks()` if the they are to be run on the
// main thread and `RegisterCompositorThreadSuccessfulCallbacks()` if they are
// to be run on the compositor thread.
//
// Once a frame is presented, users of this class can call
// `PopPendingCallbacks()` to get their callbacks back. This class never runs
// callbacks itself so it's up to calling code to `PostTask()` or call `Run()`
// as needed.
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

  using Callback = base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  using SuccessfulCallback = base::OnceCallback<void(base::TimeTicks)>;
  using SuccessfulCallbackWithDetails =
      base::OnceCallback<void(const viz::FrameTimingDetails& details)>;

  PresentationTimeCallbackBuffer();

  PresentationTimeCallbackBuffer(const PresentationTimeCallbackBuffer&) =
      delete;
  PresentationTimeCallbackBuffer(PresentationTimeCallbackBuffer&&);

  PresentationTimeCallbackBuffer& operator=(
      const PresentationTimeCallbackBuffer&) = delete;
  PresentationTimeCallbackBuffer& operator=(PresentationTimeCallbackBuffer&&);

  ~PresentationTimeCallbackBuffer();

  // Buffers the given `callbacks` in preparation for a presentation at or after
  // the given `frame_token`. The presentation is not necessarily successful.
  // Calling code posts these callbacks to the main thread once they're popped.
  void RegisterMainThreadCallbacks(uint32_t frame_token,
                                   std::vector<Callback> callbacks);

  // Buffers the given `callbacks` in preparation for a successful presentation
  // at or after the given `frame_token`. Calling code posts these callbacks to
  // the main thread once they're popped.
  void RegisterMainThreadSuccessfulCallbacks(
      uint32_t frame_token,
      std::vector<SuccessfulCallbackWithDetails> callbacks);

  // Buffers the given `callbacks` in preparation for a successful presentation
  // at or after the given `frame_token`. Calling code invokes these callbacks
  // on the compositor thread once they're popped.
  void RegisterCompositorThreadSuccessfulCallbacks(
      uint32_t frame_token,
      std::vector<SuccessfulCallback> callbacks);

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
    // `RegisterMainThreadPresentationCallbacks()`.
    std::vector<Callback> main_callbacks;

    // Holds callbacks registered through
    // `RegisterMainThreadSuccessfulPresentationCallbacks()`.
    std::vector<SuccessfulCallbackWithDetails> main_successful_callbacks;

    // Holds callbacks registered through
    // `RegisterCompositorThreadSuccessfulPresentationCallbacks()`.
    std::vector<SuccessfulCallback> compositor_successful_callbacks;
  };

  // Call this once the presentation for the given `frame_token` has completed.
  // Yields any pending callbacks that were registered against a frame token
  // that was less than or equal to the given `frame_token`. If
  // `presentation_failed` is true, successful presentation time callbacks are
  // not returned. They are only returned on successful presentations. Note that
  // since failed presentation feedbacks can arrive out of order (i.e. earlier
  // than previous frames that might get presented successfully), when called on
  // a failed presentation, this might return callbacks for previous frames that
  // are still in flight. This is okay for now as we aim to only allow
  // registering callbacks for successful presentations which will make this a
  // non-issue.
  // It is the caller's responsibility to run the callbacks on the right
  // threads/sequences.
  PendingCallbacks PopPendingCallbacks(uint32_t frame_token,
                                       bool presentation_failed);

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

    // The presentation callbacks to send back to the main thread.
    std::vector<Callback> main_callbacks;

    // The successful presentation callbacks to send back to the main thread.
    std::vector<SuccessfulCallbackWithDetails> main_successful_callbacks;

    // The successful presentation callbacks to invoke on the compositor thread.
    std::vector<SuccessfulCallback> compositor_successful_callbacks;
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
