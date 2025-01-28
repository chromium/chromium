// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BORDER_VIEW_H_
#define CHROME_BROWSER_GLIC_BORDER_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

class Browser;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace glic {

class BorderView : public views::View,
                   public ui::CompositorAnimationObserver,
                   public ui::CompositorObserver {
  METADATA_HEADER(BorderView, views::View)

 public:
  explicit BorderView(Browser* browser);
  BorderView(const BorderView&) = delete;
  BorderView& operator=(const BorderView&) = delete;
  ~BorderView() override;

  // `views::View`:
  void OnPaint(gfx::Canvas* canvas) override;

  // `ui::CompositorAnimationObserver`:
  void OnAnimationStep(base::TimeTicks timestamp) override;

  // `ui::CompositorObserver`:
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // TODO(liuwilliam): These should be private once we can end-to-end test the
  // UI behaviors.
  void StartAnimation();
  void CancelAnimation();

  ui::Compositor* compositor_for_testing() const { return compositor_; }

  float opacity_for_testing() const { return opacity_; }

  float emphasis_for_testing() const { return emphasis_; }

 private:
  // A value from 0 to 1 indicating how much the border is to be emphasized.
  float GetEmphasis(base::TimeDelta delta) const;

  // Only valid to call after the animation has started.
  void ResetEmphasisAndReplay();

  // A value from 0 to 1 indicating the opacity of the border.
  float GetOpacity(base::TimeDelta delta) const;

  // A utility class that subscribe to `GlicKeyedService` for various browser UI
  // status change.
  class BorderViewUpdater;
  const std::unique_ptr<BorderViewUpdater> updater_;

  raw_ptr<ui::Compositor> compositor_ = nullptr;

  // When it is true, the class directly presents a static border and when it is
  // false, it animates the border first.
  bool skip_animation_ = false;

  float opacity_ = 0.f;
  float emphasis_ = 0.f;

  base::TimeTicks first_frame_time_;
  base::TimeTicks first_emphasis_frame_;
};

BEGIN_VIEW_BUILDER(, BorderView, views::View)
VIEW_BUILDER_PROPERTY(bool, Visible)
VIEW_BUILDER_PROPERTY(bool, CanProcessEventsWithinSubtree)
END_VIEW_BUILDER

}  // namespace glic

DEFINE_VIEW_BUILDER(, glic::BorderView)

#endif  // CHROME_BROWSER_GLIC_BORDER_VIEW_H_
