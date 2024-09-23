// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_WINDOW_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_WINDOW_OBSERVER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/util/arc_window_watcher.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace arc {

constexpr char kArcWindowObserverName[] = "ArcWindowObserver";

// Listens to ARC Window count and blocks Doze Mode when windows are present.
class ArcWindowObserver : public ash::ThrottleObserver,
                          public ash::ArcWindowWatcher::ArcWindowCountObserver {
 public:
  ArcWindowObserver();

  ArcWindowObserver(const ArcWindowObserver&) = delete;
  ArcWindowObserver& operator=(const ArcWindowObserver&) = delete;

  ~ArcWindowObserver() override;

  // chromeos::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;

  // ash::ArcWindowWatcher::ArcWindowCountObserver:
  void OnArcWindowCountChanged(uint32_t count) override;
  void OnWillDestroyWatcher() override;

 private:
  base::ScopedObservation<ash::ArcWindowWatcher,
                          ash::ArcWindowWatcher::ArcWindowCountObserver>
      observation_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_WINDOW_OBSERVER_H_
