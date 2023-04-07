// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_window_observer.h"

namespace arc {

ArcWindowObserver::ArcWindowObserver()
    : ThrottleObserver(kArcWindowObserverName) {}

void ArcWindowObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);

  OnArcWindowCountChanged(
      ash::ArcWindowWatcher::instance()->GetArcWindowCount());

  observation_.Observe(ash::ArcWindowWatcher::instance());
}

ArcWindowObserver::~ArcWindowObserver() = default;

void ArcWindowObserver::StopObserving() {
  observation_.Reset();
  ThrottleObserver::StopObserving();
}

void ArcWindowObserver::OnArcWindowCountChanged(uint32_t count) {
  SetActive(count > 0);
}

}  // namespace arc
