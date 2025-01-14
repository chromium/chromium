// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_resize_animation.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/glic_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#include "ui/display/screen.h"
#endif

namespace glic {

GlicWindowResizeAnimation::GlicWindowResizeAnimation(
    views::Widget* widget,
    GlicView* view,
    gfx::Size new_size,
    base::TimeDelta duration,
    FinishedCallback finished_callback)
    : widget_(widget),
      view_(view),
      finished_callback_(std::move(finished_callback)) {
#if BUILDFLAG(IS_MAC)
  // Intentionally disable this block until we can test.
  volatile bool enabled = false;
  if (enabled) {
    int64_t display_id =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(widget->GetNativeWindow())
            .id();
    display_link_ = ui::DisplayLinkMac::GetForDisplay(display_id);
    vsync_callback_mac_ = display_link_->RegisterCallback(base::BindRepeating(
        &GlicWindowResizeAnimation::Vsync, base::Unretained(this)));
    CreateAnimationCurve(widget->GetSize(), new_size, duration);
    return;
  }
#endif

  widget->SetSize(new_size);
  view->web_view()->SetSize(new_size);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&GlicWindowResizeAnimation::Finished,
                                weak_ptr_factory_.GetWeakPtr()));
}

GlicWindowResizeAnimation::~GlicWindowResizeAnimation() = default;

void GlicWindowResizeAnimation::Finished() {
  // Destroys `this`.
  std::move(finished_callback_).Run();
}

#if BUILDFLAG(IS_MAC)
void GlicWindowResizeAnimation::CreateAnimationCurve(gfx::Size start_size,
                                                     gfx::Size final_size,
                                                     base::TimeDelta duration) {
  double refresh_rate = display_link_->GetRefreshRate();
  base::TimeDelta step_duration = base::Milliseconds(16);
  if (refresh_rate) {
    step_duration = base::Seconds(1) / refresh_rate;
  }

  // We use a minimum of 1 step to avoid pathological edge cases.
  int steps = duration / step_duration;
  steps = std::max(steps, 1);

  int step_width = (final_size.width() - start_size.width()) / steps;
  int step_height = (final_size.height() - start_size.height()) / steps;
  auto now = base::TimeTicks::Now();
  for (int i = 1; i < steps + 1; ++i) {
    int new_height = start_size.height() + i * step_height;
    int new_width = start_size.width() + i * step_width;

    // To avoid rounding issues, the final step should use final_size.
    if (i == steps) {
      new_width = final_size.width();
      new_height = final_size.height();
    }

    AnimationStep step;
    step.time = now + i * step_duration;
    step.size = gfx::Size(new_width, new_height);
    animation_curve_.push_back(step);
  }
}

void GlicWindowResizeAnimation::Vsync(ui::VSyncParamsMac) {
  CHECK(!animation_curve_.empty());

  // We may need to skip some steps if chrome was unable to render in time.
  auto now = base::TimeTicks::Now();
  size_t step_to_use = 0;
  for (size_t i = 0; i < animation_curve_.size(); ++i) {
    if (animation_curve_[i].time < now) {
      step_to_use = i;
    }
  }

  // Delete all steps up through and including the one used.
  gfx::Size next_size = animation_curve_[step_to_use].size;
  animation_curve_.erase(animation_curve_.begin(),
                         animation_curve_.begin() + step_to_use + 1);

  // This ensures that the web contents resize occurs in sync with the window
  // resize.
  ui::CATransactionCoordinator::Get().Synchronize();

  widget_->SetSize(next_size);
  view_->web_view()->SetSize(next_size);

  // If the animation curve is now empty, we're finished.
  if (animation_curve_.empty()) {
    vsync_callback_mac_.reset();
    display_link_.reset();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&GlicWindowResizeAnimation::Finished,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}
#endif

}  // namespace glic
