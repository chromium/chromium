// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_animator.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {
namespace {

const int kFadingAnimationDurationInMS = 200;
const int kFadingTimeoutDurationInSeconds = 10;

// CallbackRunningObserver accepts multiple layer animations and
// runs the specified |callback| when all of the animations have finished.
class CallbackRunningObserver {
 public:
  explicit CallbackRunningObserver(base::OnceClosure callback)
      : completed_counter_(0),
        animation_aborted_(false),
        callback_(std::move(callback)) {}

  CallbackRunningObserver(const CallbackRunningObserver&) = delete;
  CallbackRunningObserver& operator=(const CallbackRunningObserver&) = delete;

  void AddNewAnimator(ui::LayerAnimator* animator) {
    auto observer = std::make_unique<Observer>(animator, this);
    animator->AddObserver(observer.get());
    observer_list_.push_back(std::move(observer));
  }

 private:
  void OnSingleTaskCompleted() {
    completed_counter_++;
    if (completed_counter_ >= observer_list_.size()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                    this);
      if (!animation_aborted_)
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(callback_));
    }
  }

  void OnSingleTaskAborted() {
    animation_aborted_ = true;
    OnSingleTaskCompleted();
  }

  // The actual observer to listen each animation completion.
  class Observer : public ui::LayerAnimationObserver {
   public:
    Observer(ui::LayerAnimator* animator, CallbackRunningObserver* observer)
        : animator_(animator), observer_(observer) {}

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

   protected:
    // ui::LayerAnimationObserver overrides:
    void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
      animator_->RemoveObserver(this);
      observer_->OnSingleTaskCompleted();
    }
    void OnLayerAnimationAborted(
        ui::LayerAnimationSequence* sequence) override {
      animator_->RemoveObserver(this);
      observer_->OnSingleTaskAborted();
    }
    void OnLayerAnimationScheduled(
        ui::LayerAnimationSequence* sequence) override {}
    bool RequiresNotificationWhenAnimatorDestroyed() const override {
      return true;
    }

   private:
    raw_ptr<ui::LayerAnimator> animator_;
    raw_ptr<CallbackRunningObserver> observer_;
  };

  size_t completed_counter_;
  bool animation_aborted_;
  std::vector<std::unique_ptr<Observer>> observer_list_;
  base::OnceClosure callback_;
};

}  // namespace

DisplayAnimator::DisplayAnimator() {
  Shell::Get()->display_configurator()->AddObserver(this);
}

DisplayAnimator::~DisplayAnimator() {
  Shell::Get()->display_configurator()->RemoveObserver(this);
  ClearHidingLayers();
}

void DisplayAnimator::StartFadeOutAnimation(base::OnceClosure callback) {
  CallbackRunningObserver* observer =
      new CallbackRunningObserver(std::move(callback));
  ClearHidingLayers();

  // Make the fade-out animation for all root windows.  Instead of actually
  // hiding the root windows, we put a black layer over a root window for
  // safety.  These layers remain to hide root windows and will be deleted
  // after the animation of OnDisplayModeChanged().
  for (aura::Window* root_window : Shell::Get()->GetAllRootWindows()) {
    std::unique_ptr<ui::Layer> hiding_layer =
        std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    hiding_layer->SetColor(SK_ColorBLACK);
    hiding_layer->SetBounds(root_window->bounds());
    ui::Layer* parent =
        Shell::GetContainer(root_window, kShellWindowId_OverlayContainer)
            ->layer();
    parent->Add(hiding_layer.get());

    hiding_layer->SetOpacity(0.0);

    ui::ScopedLayerAnimationSettings settings(hiding_layer->GetAnimator());
    settings.SetTransitionDuration(
        base::Milliseconds(kFadingAnimationDurationInMS));
    observer->AddNewAnimator(hiding_layer->GetAnimator());
    hiding_layer->SetOpacity(1.0f);
    hiding_layer->SetVisible(true);
    hiding_layers_[root_window] = std::move(hiding_layer);
  }

  // In case that OnDisplayModeChanged() isn't called or its animator is
  // canceled due to some unknown errors, we set a timer to clear these
  // hiding layers.
  timer_ = std::make_unique<base::OneShotTimer>();
  timer_->Start(FROM_HERE, base::Seconds(kFadingTimeoutDurationInSeconds), this,
                &DisplayAnimator::ClearHidingLayers);
}

void DisplayAnimator::StartFadeInAnimation() {
  // We want to make sure clearing all of hiding layers after the animation
  // finished.  Note that this callback can be canceled, but the cancel only
  // happens when the next animation is scheduled.  Thus the hiding layers
  // should be deleted eventually.
  CallbackRunningObserver* observer =
      new CallbackRunningObserver(base::BindOnce(
          &DisplayAnimator::ClearHidingLayers, weak_ptr_factory_.GetWeakPtr()));

  // Ensure that layers are not animating.
  for (auto& e : hiding_layers_) {
    ui::LayerAnimator* animator = e.second->GetAnimator();
    if (animator->is_animating())
      animator->StopAnimating();
  }

  // Schedules the fade-in effect for all root windows.  Because we put the
  // black layers for fade-out, here we actually turn those black layers
  // invisible.
  for (aura::Window* root_window : Shell::Get()->GetAllRootWindows()) {
    ui::Layer* hiding_layer = nullptr;
    if (!base::Contains(hiding_layers_, root_window)) {
      // In case of the transition from mirroring->non-mirroring, new root
      // windows appear and we do not have the black layers for them.  Thus
      // we need to create the layer and make it visible.
      hiding_layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
      hiding_layer->SetColor(SK_ColorBLACK);
      hiding_layer->SetBounds(root_window->bounds());
      ui::Layer* parent =
          Shell::GetContainer(root_window, kShellWindowId_OverlayContainer)
              ->layer();
      parent->Add(hiding_layer);
      hiding_layer->SetOpacity(1.0f);
      hiding_layer->SetVisible(true);
      hiding_layers_[root_window] = base::WrapUnique(hiding_layer);
    } else {
      hiding_layer = hiding_layers_[root_window].get();
      if (hiding_layer->bounds() != root_window->bounds())
        hiding_layer->SetBounds(root_window->bounds());
    }

    ui::ScopedLayerAnimationSettings settings(hiding_layer->GetAnimator());
    settings.SetTransitionDuration(
        base::Milliseconds(kFadingAnimationDurationInMS));
    observer->AddNewAnimator(hiding_layer->GetAnimator());
    hiding_layer->SetOpacity(0.0f);
    hiding_layer->SetVisible(false);
  }
}

void DisplayAnimator::OnDisplayConfigurationChanged(
    const display::DisplayConfigurator::DisplayStateList& displays) {
  if (!hiding_layers_.empty())
    StartFadeInAnimation();
}

void DisplayAnimator::OnDisplayConfigurationChangeFailed(
    const display::DisplayConfigurator::DisplayStateList& displays,
    display::MultipleDisplayState failed_new_state) {
  if (!hiding_layers_.empty())
    StartFadeInAnimation();
}

void DisplayAnimator::ClearHidingLayers() {
  if (timer_) {
    timer_->Stop();
    timer_.reset();
  }
  hiding_layers_.clear();
}

}  // namespace ash
