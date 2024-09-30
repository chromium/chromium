// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/util/arc_window_watcher.h"

#include <algorithm>

#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool ShouldIgnoreWindow(aura::Window* window) {
  // ArcWindowWatcher doesn't interested in Ash browser windows.
  return chrome::FindBrowserWithWindow(window);
}

class Tracker : public aura::WindowObserver {
 public:
  explicit Tracker(aura::Window* window) : window_(window) {
    window->AddObserver(this);
  }

  Tracker(const Tracker&) = delete;
  Tracker& operator=(const Tracker&) = delete;

  ~Tracker() override { window_->RemoveObserver(this); }

  void OnPackageNameChanged() {
    if (display_reported_) {
      // Must not do this more than once
      return;
    }
    const auto* pkg_name = arc_window_->GetProperty(ash::kArcPackageNameKey);
    if (!pkg_name || pkg_name->empty()) {
      return;
    }
    display_reported_ = true;
    ash::ArcWindowWatcher::instance()->BroadcastArcWindowDisplay(*pkg_name);
  }

  void MaybeTagArcWindow() {
    if (!ash::IsArcWindow(window_)) {
      return;
    }
    arc_window_ = window_;
    ash::ArcWindowWatcher::instance()->OnArcWindowAdded();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    ash::ArcWindowWatcher::instance()->OnTrackerRemoved(
        this, arc_window_ /*may be nullptr*/);
    // WARNING: this is deleted here - must return immediately.
  }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (arc_window_) {
      if (key == ash::kArcPackageNameKey) {
        OnPackageNameChanged();
      }
      return;
    }

    if (ShouldIgnoreWindow(window)) {
      ash::ArcWindowWatcher::instance()->OnTrackerRemoved(this, nullptr);
      // WARNING: this is deleted here - must return immediately.
      return;
    }

    if (key == chromeos::kAppTypeKey) {
      // Maybe it just became an ARC window.
      MaybeTagArcWindow();
    }
  }

 private:
  raw_ptr<aura::Window> window_;
  raw_ptr<aura::Window> arc_window_ =
      nullptr;  // set to window_ when we know it is ARC.
  bool display_reported_ = false;
};

}  // namespace

// static
ArcWindowWatcher* ArcWindowWatcher::instance_ = nullptr;

ArcWindowWatcher::ArcWindowWatcher() {
  DCHECK(!instance_);
  instance_ = this;
  aura::Env::GetInstance()->AddObserver(this);
}

ArcWindowWatcher::~ArcWindowWatcher() {
  DCHECK(instance_ == this);
  // Stop observing Env, to ensure no new trackers are created.
  aura::Env::GetInstance()->RemoveObserver(this);

  // Then remove all existing trackers in one shot.
  trackers_.clear();

  // Tell observers, so they have a chance to un-subscribe.
  for (auto& observer : arc_window_display_observers_) {
    observer.OnWillDestroyWatcher();
  }
  for (auto& observer : arc_window_count_observers_) {
    observer.OnWillDestroyWatcher();
  }

  instance_ = nullptr;
}

uint32_t ArcWindowWatcher::GetArcWindowCount() const {
  return arc_window_count_;
}

void ArcWindowWatcher::OnWindowInitialized(aura::Window* window) {
  // Filter a large set of controls that cannot be ARC windows.
  if (window->GetType() != aura::client::WINDOW_TYPE_NORMAL ||
      !window->delegate()) {
    return;
  }
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level()) {
    return;
  }

  if (ShouldIgnoreWindow(window)) {
    return;
  }

  trackers_.push_back(std::make_unique<Tracker>(window));
  trackers_.back()->MaybeTagArcWindow();
}

// This is the main "plus" point, where we know an ARC window is born.
void ArcWindowWatcher::OnArcWindowAdded() {
  ++arc_window_count_;
  BroadcastArcWindowCount(arc_window_count_);
}

// This is the main "minus" point, where we know an ARC window is gone.
void ArcWindowWatcher::OnArcWindowRemoved() {
  --arc_window_count_;
  BroadcastArcWindowCount(arc_window_count_);
}

void ArcWindowWatcher::OnTrackerRemoved(Tracker* tracker,
                                        aura::Window* arc_window) {
  // Order N -- we opted for simplicit of code rather than fastest performance,
  // as the number of elements is small (number of top windows on the system).
  // alternative considered: keep iterator when adding, and use a container
  // such as list, where elements can be removed from the middle with order 1.
  // Note that this would increase the # of dynamic allocations and fragment
  // memory.
  auto tracker_iterator =
      std::find_if(trackers_.begin(), trackers_.end(),
                   [tracker](const auto& e) { return e.get() == tracker; });
  DCHECK(tracker_iterator != trackers_.end());
  trackers_.erase(tracker_iterator);

  if (arc_window) {
    OnArcWindowRemoved();
  }
}

// Manage the list of arc-window-count observers.
void ArcWindowWatcher::AddObserver(ArcWindowCountObserver* observer) {
  arc_window_count_observers_.AddObserver(observer);
}

void ArcWindowWatcher::RemoveObserver(ArcWindowCountObserver* observer) {
  arc_window_count_observers_.RemoveObserver(observer);
}

void ArcWindowWatcher::BroadcastArcWindowCount(uint32_t count) {
  for (auto& observer : arc_window_count_observers_) {
    observer.OnArcWindowCountChanged(count);
  }
}

// Manage the list of arc-window-display observers.
void ArcWindowWatcher::AddObserver(ArcWindowDisplayObserver* observer) {
  arc_window_display_observers_.AddObserver(observer);
}

void ArcWindowWatcher::RemoveObserver(ArcWindowDisplayObserver* observer) {
  arc_window_display_observers_.RemoveObserver(observer);
}

void ArcWindowWatcher::BroadcastArcWindowDisplay(const std::string& pkg_name) {
  for (auto& observer : arc_window_display_observers_) {
    observer.OnArcWindowDisplayed(pkg_name);
  }
}

bool ArcWindowWatcher::HasCountObserver(
    ArcWindowCountObserver* observer) const {
  return arc_window_count_observers_.HasObserver(observer);
}

bool ArcWindowWatcher::HasDisplayObserver(
    ArcWindowDisplayObserver* observer) const {
  return arc_window_display_observers_.HasObserver(observer);
}

}  // namespace ash
