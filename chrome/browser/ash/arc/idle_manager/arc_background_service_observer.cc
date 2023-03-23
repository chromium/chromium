// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_background_service_observer.h"

namespace arc {

ArcBackgroundServiceObserver::ArcBackgroundServiceObserver()
    : ThrottleObserver(kArcBackgroundServiceObserverName) {}

ArcBackgroundServiceObserver::~ArcBackgroundServiceObserver() = default;

void ArcBackgroundServiceObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  DCHECK_EQ(context_, nullptr);
  context_ = context;
  ThrottleObserver::StartObserving(context_, callback);
  auto* bridge = ArcSystemStateBridge::GetForBrowserContext(context_);
  if (bridge) {
    observation_.Observe(bridge);
  }
}

void ArcBackgroundServiceObserver::StopObserving() {
  DCHECK_NE(context_, nullptr);
  observation_.Reset();
  ThrottleObserver::StopObserving();
  context_ = nullptr;
}

void ArcBackgroundServiceObserver::OnArcSystemAppRunningStateChange(
    const mojom::SystemAppRunningState& state) {
  SetActive(state.background_service);
}

}  // namespace arc
