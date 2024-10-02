// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_view_scheduler.h"

#include <utility>

#include "ash/wm/overview/overview_ui_task_pool.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace {

class OverviewItemViewScheduler : public aura::WindowObserver,
                                  public ui::LayerAnimationObserver,
                                  public views::WidgetObserver {
 public:
  // `should_init_view` is only false if the overview item or session is
  // destroyed before the `OverviewItemWidget` has a chance to become visible
  // (should be rare).
  using CompletionCallback = base::OnceCallback<void(bool should_init_view)>;

  OverviewItemViewScheduler() = default;
  OverviewItemViewScheduler(const OverviewItemViewScheduler&) = delete;
  OverviewItemViewScheduler& operator=(const OverviewItemViewScheduler&) =
      delete;
  ~OverviewItemViewScheduler() override = default;

  void Init(aura::Window& overview_item_window,
            views::Widget& overview_item_widget,
            OverviewUiTaskPool& enter_animation_task_pool,
            CompletionCallback completion_cb) {
    completion_cb_ = std::move(completion_cb);
    CHECK(completion_cb_);
    // Defer initialization until there's a free period on the UI thread during
    // the enter animation. Or wait until the widget becomes non-transparent
    // (i.e. on-demand). Whichever comes first.
    enter_animation_task_pool.AddTask(
        base::BindOnce(&OverviewItemViewScheduler::RunCompletionCallback,
                       weak_ptr_factory_.GetWeakPtr(), true));
    window_observation_.Observe(overview_item_widget.GetNativeWindow());
    layer_animation_observation_.Observe(
        overview_item_widget.GetLayer()->GetAnimator());
    widget_observation_.Observe(&overview_item_widget);
  }

 private:
  // aura::WindowObserver:
  //
  // For cases where the widget is made opaque without an animation.
  void OnWindowOpacitySet(aura::Window* window,
                          ui::PropertyChangeReason reason) override {
    if (window->layer()->GetTargetOpacity() > 0.f) {
      RunCompletionCallback(true);
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    RunCompletionCallback(false);
  }

  // ui::LayerAnimationObserver:
  //
  // For cases where the widget is made opaque with an animation. Do not check
  // target opacity since that is unavailable if the animation is a repeating
  // animation. Any animation that modifies the opacity counts.
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {
    if (sequence->properties() & ui::LayerAnimationElement::OPACITY) {
      RunCompletionCallback(true);
    }
  }

  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    RunCompletionCallback(false);
  }

  void RunCompletionCallback(bool should_init_view) {
    if (completion_cb_) {
      std::move(completion_cb_).Run(should_init_view);
    }
  }

  CompletionCallback completion_cb_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
  base::ScopedObservation<ui::LayerAnimator, ui::LayerAnimationObserver>
      layer_animation_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::WeakPtrFactory<OverviewItemViewScheduler> weak_ptr_factory_{this};
};

void ForwardInitSignalToCaller(
    base::OnceClosure original_initialize_item_view_cb,
    std::unique_ptr<OverviewItemViewScheduler> scheduler,
    bool should_initialize_item_view) {
  if (should_initialize_item_view) {
    CHECK(original_initialize_item_view_cb);
    std::move(original_initialize_item_view_cb).Run();
  }
}

}  // namespace

void ScheduleOverviewItemViewInitialization(
    aura::Window& overview_item_window,
    views::Widget& overview_item_widget,
    OverviewUiTaskPool& enter_animation_task_pool,
    bool should_enter_without_animations,
    base::OnceClosure initialize_item_view_cb) {
  CHECK(initialize_item_view_cb);
  const bool should_immediately_init_view =
      // Minimized windows are immediately visible in the first overview frame
      // and contain a mirror of the overview item's layer tree.
      window_util::IsMinimizedOrTucked(&overview_item_window) ||
      should_enter_without_animations ||
      !chromeos::features::AreOverviewSessionInitOptimizationsEnabled();
  if (should_immediately_init_view) {
    std::move(initialize_item_view_cb).Run();
    return;
  }

  auto scheduler = std::make_unique<OverviewItemViewScheduler>();
  OverviewItemViewScheduler& scheduler_ref = *scheduler.get();
  scheduler_ref.Init(
      overview_item_window, overview_item_widget, enter_animation_task_pool,
      base::BindOnce(&ForwardInitSignalToCaller,
                     std::move(initialize_item_view_cb), std::move(scheduler)));
}

}  // namespace ash
