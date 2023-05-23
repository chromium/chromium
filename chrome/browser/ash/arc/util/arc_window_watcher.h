// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_UTIL_ARC_WINDOW_WATCHER_H_
#define CHROME_BROWSER_ASH_ARC_UTIL_ARC_WINDOW_WATCHER_H_

#include <vector>

#include "base/observer_list.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

namespace {
class Tracker;
}

// ArcWindowWatcher provides live monitoring of Arc Windows.
// It distills basic window transition events into consolidated
// ARC-window and Android-task related events.
class ArcWindowWatcher : public aura::EnvObserver {
 public:
  using TrackerList = std::vector<std::unique_ptr<Tracker>>;
  class ArcWindowCountObserver : public base::CheckedObserver {
   public:
    // Notifies that window count has changed.
    virtual void OnArcWindowCountChanged(uint32_t count) = 0;
    virtual void OnWillDestroyWatcher() = 0;
  };

  class ArcWindowDisplayObserver : public base::CheckedObserver {
   public:
    // Notifies that a new window is display. This is guaranteed to happen
    // after the count is updated.
    virtual void OnArcWindowDisplayed(const std::string& pkg_name) = 0;
    virtual void OnWillDestroyWatcher() = 0;
  };

  // Returns the single ArcWindowWatcher instance.
  static ArcWindowWatcher* instance() { return instance_; }

  ArcWindowWatcher();

  ArcWindowWatcher(const ArcWindowWatcher&) = delete;
  ArcWindowWatcher& operator=(const ArcWindowWatcher&) = delete;

  ~ArcWindowWatcher() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  uint32_t GetArcWindowCount() const;

  void AddObserver(ArcWindowCountObserver* observer);
  void RemoveObserver(ArcWindowCountObserver* observer);

  // Manage the list of arc-window-display observers.
  void AddObserver(ArcWindowDisplayObserver* observer);
  void RemoveObserver(ArcWindowDisplayObserver* observer);

  // Notifies observers of a change in window count
  void BroadcastArcWindowCount(uint32_t count);

  // Notifies observers that a window got a package name, i.e.,
  // it was displayed by Android.
  void BroadcastArcWindowDisplay(const std::string& pkg_name);

  // Nudging from trackers to maintain internal collection.
  void OnArcWindowAdded();
  void OnTrackerRemoved(Tracker* tracker, aura::Window* arc_window);
  void OnArcWindowRemoved();

  // Query existence of a particular observer, by type.
  bool HasCountObserver(ArcWindowCountObserver* observer) const;
  bool HasDisplayObserver(ArcWindowDisplayObserver* observer) const;

 private:
  base::ObserverList<ArcWindowCountObserver, /* check_empty= */ true>
      arc_window_count_observers_;
  base::ObserverList<ArcWindowDisplayObserver, /* check_empty= */ true>
      arc_window_display_observers_;

  // Some are arc windows, some are unknown.
  TrackerList trackers_;

  // Keeps track of ARC windows. Notice we're not tracking the list of windows,
  // but it is very easy to track that too (when needed), at places where
  // this counter is modified.
  uint32_t arc_window_count_ = 0;

  static ArcWindowWatcher* instance_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ARC_UTIL_ARC_WINDOW_WATCHER_H_
