// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_INTERFACE_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_INTERFACE_H_

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_state.h"
#include "cc/trees/render_frame_metadata.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class BrowserControlsOffsetManagerClient;
struct BrowserControlsAnimationParams;

// Manages the position of the browser controls.
//
// This class owns the state that does not depend on how the controls are
// moved: the controls geometry, the offset tags and the constraint synced to
// the main thread. Subclasses decide how the shown ratios change in response
// to scrolls, state updates and animation ticks.
class CC_EXPORT BrowserControlsOffsetManagerInterface {
 public:
  BrowserControlsOffsetManagerInterface(
      const BrowserControlsOffsetManagerInterface&) = delete;
  BrowserControlsOffsetManagerInterface& operator=(
      const BrowserControlsOffsetManagerInterface&) = delete;
  virtual ~BrowserControlsOffsetManagerInterface();

  // The offset from the window top to the top edge of the controls. Runs from 0
  // (controls fully shown) to negative values (down is positive).
  float ControlsTopOffset() const;

  // The amount of offset of the web content area. Same as the current shown
  // height of the browser controls.
  float ContentTopOffset() const;

  float TopControlsShownRatio() const;
  float TopControlsHeight() const;
  float TopControlsMinHeight() const;

  // The minimum shown ratio top controls can have.
  float TopControlsMinShownRatio() const;

  // The current top controls min-height. If the min-height is changing with an
  // animation, this will return a value between the old min-height and the new
  // min-height, which is equal to the current visible min-height. Otherwise,
  // this will return the same value as `TopControlsMinHeight()`.
  float TopControlsMinHeightOffset() const;

  // The amount of offset of the web content area, calculating from the bottom.
  // Same as the current shown height of the bottom controls.
  float ContentBottomOffset() const;

  // Similar to TopControlsHeight(), this method should return a static value.
  // The current animated height should be acquired from ContentBottomOffset().
  float BottomControlsHeight() const;
  float BottomControlsMinHeight() const;
  float BottomControlsShownRatio() const;

  // The minimum shown ratio bottom controls can have.
  float BottomControlsMinShownRatio() const;

  // The current bottom controls min-height. If the min-height is changing with
  // an animation, this will return a value between the old min-height and the
  // new min-height, which is equal to the current visible min-height.
  // Otherwise, this will return the same value as `BottomControlsMinHeight()`.
  float BottomControlsMinHeightOffset() const;

  // See UpdateBrowserControlsState in
  // third_party/blink/public/mojom/frame/frame.mojom
  //
  // Stores the constraint and the offset tag modifications, then calls
  // OnBrowserControlsStateUpdated().
  void UpdateBrowserControlsState(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagModifications>
          offset_tag_modifications);

  // Directly set the current offset tag modifications.
  // This is used to set the offset tag modifications from the viz process.
  void SetOffsetTagModifications(
      const BrowserControlsOffsetTagModifications& offset_tag_modifications) {
    offset_tag_modifications_ = offset_tag_modifications;
  }

  // Returns the current offset tag modifications.
  const BrowserControlsOffsetTagModifications& GetOffsetTagModifications()
      const {
    return offset_tag_modifications_;
  }

  BrowserControlsMetadata GetMetadata() const;

  // Return the browser control constraint that must be synced to the
  // main renderer thread (to trigger viewport and related changes).
  BrowserControlsState PullConstraintForMainThread(
      bool* out_changed_since_commit);
  // Called to notify this object that the control constraint has
  // been pushed to the main thread. When a compositor commit does not
  // happen the value pulled by the method above may not be synced;
  // a call to this method notifies us that it has.
  void NotifyConstraintSyncedToMainThread();

  // Predict what the outer viewport container bounds delta will be as browser
  // controls are shown or hidden during a scroll gesture before the Blink
  // WebView is resized to reflect the new state.
  double PredictViewportBoundsDelta(double current_bounds_delta,
                                    gfx::Vector2dF scroll_distance);

  // Returns the current offset tag values for the browser controls. Currently
  // used only on Android.
  virtual std::vector<viz::OffsetTagValue> GetOffsetTagValues() const = 0;

  // Valid shown ratio range for the top controls. The values will be (0, 1) if
  // there is no animation running.
  virtual std::pair<float, float> TopControlsShownRatioRange() = 0;

  // Valid shown ratio range for the bottom controls. The values will be (0, 1)
  // if there is no animation running.
  virtual std::pair<float, float> BottomControlsShownRatioRange() = 0;

  // Returns true if Animate() must be called on the next frame to update the
  // shown ratios and/or counter-scroll the page.
  virtual bool HasAnimation() = 0;

  virtual void OnBrowserControlsParamsChanged(bool animate_changes) = 0;

  // Animates the visible heights of the controls to the targets in `params`
  // as dictated by the browser process. A zero duration applies the
  // targets on the next Animate() together with the counter-scroll of the
  // page. Replaces a running animation. Only called when the browser process
  // owns the controls position.
  virtual void SetAnimation(const BrowserControlsAnimationParams& params) = 0;

  virtual void ScrollBegin() = 0;
  // Scrolls the browser controls by the given `pending_delta` and returns the
  // remaining scroll delta that was not applied. `is_inertial` indicates
  // whether the scroll update received from the browser process corresponds to
  // a scroll that continues after the user lifts their finger off the screen
  // (e.g. fling).
  virtual gfx::Vector2dF ScrollBy(const gfx::Vector2dF& pending_delta,
                                  bool is_inertial) = 0;
  virtual void ScrollEnd(const gfx::Vector2dF& compensated_scroll_delta) = 0;

  // The caller should ensure that `Pinch{Begin,End}` are called within
  // the scope of `Scroll{Begin,End}`.
  virtual void PinchBegin() = 0;
  virtual void PinchEnd() = 0;

  // Updates the controls shown ratios for the frame at `monotonic_time` and
  // returns the scroll delta needed to counter-scroll the viewport as top
  // controls move.
  virtual gfx::Vector2dF Animate(base::TimeTicks monotonic_time) = 0;

  virtual void ResetAnimations() = 0;

 protected:
  explicit BrowserControlsOffsetManagerInterface(
      BrowserControlsOffsetManagerClient* client);

  // Moves the controls to satisfy `constraints` and `current`. Called by
  // UpdateBrowserControlsState() after the constraint is stored.
  virtual void OnBrowserControlsStateUpdated(BrowserControlsState constraints,
                                             BrowserControlsState current,
                                             bool animate) = 0;

  BrowserControlsOffsetManagerClient& client() const { return *client_; }

  BrowserControlsState permitted_state() const { return permitted_state_; }

  void set_top_controls_min_height_offset(float offset) {
    top_controls_min_height_offset_ = offset;
  }
  void set_bottom_controls_min_height_offset(float offset) {
    bottom_controls_min_height_offset_ = offset;
  }

 private:
  // The client manages the lifecycle of this.
  raw_ptr<BrowserControlsOffsetManagerClient> client_;

  BrowserControlsState permitted_state_ = BrowserControlsState::kBoth;

  // Used to track whether the constraint has changed and we need to reflect
  // the changes to Blink.
  bool constraint_changed_since_commit_ = false;

  // Current top/bottom controls min-height.
  float top_controls_min_height_offset_ = 0.0f;
  float bottom_controls_min_height_offset_ = 0.0f;

  BrowserControlsOffsetTagModifications offset_tag_modifications_;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_INTERFACE_H_
