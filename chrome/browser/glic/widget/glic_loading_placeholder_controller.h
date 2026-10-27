// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_LOADING_PLACEHOLDER_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_LOADING_PLACEHOLDER_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/widget/glic_loading_placeholder_view.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {

class GlicView;

// Manages the lazy creation, bounds, focus handoff, draggable header hit-test,
// and stay-awake `WebContents` capturer lock of `GlicLoadingPlaceholderView`
// inside `GlicView`.
class GlicLoadingPlaceholderController {
 public:
  // `glic_view` must be non-null and must outlive this controller.
  explicit GlicLoadingPlaceholderController(GlicView* glic_view);
  GlicLoadingPlaceholderController(const GlicLoadingPlaceholderController&) =
      delete;
  GlicLoadingPlaceholderController& operator=(
      const GlicLoadingPlaceholderController&) = delete;
  ~GlicLoadingPlaceholderController();

  void Configure(GlicLoadingPlaceholderView::Mode mode,
                 base::RepeatingClosure close_callback);
  void SetShowing(bool showing);
  bool is_showing() const { return is_showing_; }

  void OnWebContentsAttached(content::WebContents* new_web_contents);
  bool MaybeRequestFocus();
  void UpdateBounds();
  bool IsPointWithinDraggableRegion(const gfx::Point& point_in_glic_view) const;
  void SetBackgroundRoundedCorners(const gfx::RoundedCornersF& radii);
  void ResetCapturer();
  void SyncFocusAccessibilityState();

  GlicLoadingPlaceholderView* placeholder_view() { return placeholder_view_; }

 private:
  void EnsurePlaceholderView();
  void UpdateLoadingCapturer();

  const raw_ref<GlicView> glic_view_;
  GlicLoadingPlaceholderView::Mode mode_ =
      GlicLoadingPlaceholderView::Mode::kSidePanel;
  base::RepeatingClosure close_callback_;
  raw_ptr<GlicLoadingPlaceholderView> placeholder_view_ = nullptr;
  bool is_showing_ = false;
  bool restore_focus_on_web_contents_attach_ = false;
  bool restore_stored_focus_on_web_contents_attach_ = false;
  base::ScopedClosureRunner loading_capturer_runner_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_LOADING_PLACEHOLDER_CONTROLLER_H_
