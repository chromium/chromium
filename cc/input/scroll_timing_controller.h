// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_TIMING_CONTROLLER_H_
#define CC_INPUT_SCROLL_TIMING_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/input/scroll_timing_info.h"
#include "cc/paint/element_id.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

// Owns the compositor-thread state for the Performance Scroll Timing API.
// `InputHandler` instantiates one of these and forwards notifications about
// scroll lifecycle events; all per-scroll tracking, filtering, and entry
// finalization happens here so `InputHandler` is not aware of the details.
//
// Lifetime / threading: lives as a value member of `InputHandler`, so all
// access is from the compositor thread.
class CC_EXPORT ScrollTimingController {
 public:
  ScrollTimingController();
  ScrollTimingController(const ScrollTimingController&) = delete;
  ScrollTimingController& operator=(const ScrollTimingController&) = delete;
  ~ScrollTimingController();

  // Starts tracking a scroll gesture. `event_timestamp` is the originating
  // input event's hardware timestamp; Input types not within the API's scope
  // are silently ignored.
  void DidScrollBegin(ui::ScrollInputType input_type,
                      base::TimeTicks event_timestamp);

  // Notifies that a scroll update moved a scroller. The first call within
  // a gesture captures the latched `element_id`; subsequent calls in the
  // same gesture are no-ops. Safe to call when no gesture is being tracked.
  void DidScrollUpdate(ElementId element_id);

  // Ends tracking of the current gesture. A record is enqueued iff a
  // scroller was latched during the gesture; `input_type` mismatches are
  // dropped defensively.
  void DidScrollEnd(ui::ScrollInputType input_type);

  // Drains finalized records for transfer to the main thread via
  // `CompositorCommitData` during commit.
  std::vector<ScrollTimingInfo> TakeCompletedScrollTimingInfos();

  // Test-only: start time of the in-flight record, if any. Used by
  // InputHandlerProxy tests in blink to verify the hardware-timestamp
  // handoff without driving a full scroll-end + commit (that end-to-end
  // path is covered in layer_tree_host_impl_unittest.cc).
  std::optional<base::TimeTicks> ActiveScrollStartForTesting() const;

 private:
  // The in-flight scroll being tracked, if any. Finalized on gesture end
  // and copied into `completed_infos_`.
  std::optional<ScrollTimingInfo> active_info_;

  // Multiple gestures may finalize between commits, so keep a batch here
  // and drain it on commit.
  std::vector<ScrollTimingInfo> completed_infos_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_TIMING_CONTROLLER_H_
