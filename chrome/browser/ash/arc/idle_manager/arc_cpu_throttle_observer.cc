// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_cpu_throttle_observer.h"

#include "chrome/browser/ash/arc/instance_throttle/arc_instance_throttle.h"

namespace arc {

ArcCpuThrottleObserver::ArcCpuThrottleObserver()
    : ThrottleObserver(kArcCpuThrottleObserverName) {}

void ArcCpuThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);

  auto* const instance_throttle =
      arc::ArcInstanceThrottle::GetForBrowserContext(context);

  // This may be null in unit tests.
  if (instance_throttle) {
    instance_throttle->AddServiceObserver(this);
    // Align internal state with that of the underlying service.
    OnThrottle(instance_throttle->should_throttle());
  }
}

void ArcCpuThrottleObserver::StopObserving() {
  auto* const instance_throttle =
      arc::ArcInstanceThrottle::GetForBrowserContext(context());

  // This may be null in unit tests.
  if (instance_throttle)
    instance_throttle->RemoveServiceObserver(this);

  ThrottleObserver::StopObserving();
}

void ArcCpuThrottleObserver::OnThrottle(bool throttled) {
  // The role of the observer is to vote "true" as a means to halt throttling on
  // the parent service. For that reason, it the negative version of the
  // _throttled_ flag managed by the service we are watching.
  SetActive(!throttled);
}

}  // namespace arc
