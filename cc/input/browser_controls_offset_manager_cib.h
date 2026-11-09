// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_CIB_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_CIB_H_

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_animation.h"
#include "cc/input/browser_controls_offset_manager_interface.h"
#include "cc/input/browser_controls_state.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class BrowserControlsOffsetManagerClient;
struct BrowserControlsAnimationParams;

// Browser-controls offset manager for Controls-in-Browser (CIB). Runs in the
// renderer compositor, and in its Viz display tree when TreesInViz is enabled;
// never in the browser process. The browser process owns the controls
// position; this class only applies the positions and animations that the
// browser process sends.
class CC_EXPORT BrowserControlsOffsetManagerCib
    : public BrowserControlsOffsetManagerInterface {
 public:
  explicit BrowserControlsOffsetManagerCib(
      BrowserControlsOffsetManagerClient* client);
  BrowserControlsOffsetManagerCib(const BrowserControlsOffsetManagerCib&) =
      delete;
  BrowserControlsOffsetManagerCib& operator=(
      const BrowserControlsOffsetManagerCib&) = delete;
  ~BrowserControlsOffsetManagerCib() override;

  // BrowserControlsOffsetManagerInterface:
  std::vector<viz::OffsetTagValue> GetOffsetTagValues() const override;
  std::pair<float, float> TopControlsShownRatioRange() override;
  std::pair<float, float> BottomControlsShownRatioRange() override;
  bool HasAnimation() override;
  void OnBrowserControlsParamsChanged(bool animate_changes) override;
  void SetAnimation(const BrowserControlsAnimationParams& params) override;
  void ScrollBegin() override;
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF& pending_delta,
                          bool is_inertial) override;
  void ScrollEnd(const gfx::Vector2dF& compensated_scroll_delta) override;
  void PinchBegin() override;
  void PinchEnd() override;
  gfx::Vector2dF Animate(base::TimeTicks monotonic_time) override;
  void ResetAnimations() override;

 protected:
  // BrowserControlsOffsetManagerInterface:
  void OnBrowserControlsStateUpdated(BrowserControlsState constraints,
                                     BrowserControlsState current,
                                     bool animate) override;

 private:
  // Shown-ratio animations driven by the browser process.
  BrowserControlsAnimation top_controls_animation_;
  BrowserControlsAnimation bottom_controls_animation_;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_CIB_H_
