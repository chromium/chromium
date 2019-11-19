// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_pip_window_throttle_observer.h"

#include <algorithm>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "base/logging.h"
#include "components/arc/arc_util.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"

namespace arc {

namespace {

aura::Window* GetPipContainer() {
  if (!exo::WMHelper::HasInstance())
    return nullptr;
  auto* const wm_helper = exo::WMHelper::GetInstance();
  DCHECK(wm_helper);
  aura::Window* const pip_container =
      wm_helper->GetPrimaryDisplayContainer(ash::kShellWindowId_PipContainer);
  DCHECK(pip_container);
  return pip_container;
}

}  // namespace

ArcPipWindowThrottleObserver::ArcPipWindowThrottleObserver()
    : ThrottleObserver(ThrottleObserver::PriorityLevel::IMPORTANT,
                       "ArcPipWindowIsVisible") {}

void ArcPipWindowThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  auto* const container = GetPipContainer();
  if (!container)  // for testing
    return;
  container->AddObserver(this);
}

void ArcPipWindowThrottleObserver::StopObserving() {
  auto* const container = GetPipContainer();
  if (!container)  // for testing
    return;
  container->RemoveObserver(this);
  ThrottleObserver::StopObserving();
}

void ArcPipWindowThrottleObserver::OnWindowAdded(aura::Window* window) {
  if (IsArcAppWindow(window))
    SetActive(true);
}

void ArcPipWindowThrottleObserver::OnWindowRemoved(aura::Window* window) {
  // Check if there are any ARC windows left in the PipContainer. An old PIP
  // window may be removed after a new one is added.
  auto* const container = GetPipContainer();
  if (std::none_of(container->children().begin(), container->children().end(),
                   &IsArcAppWindow)) {
    SetActive(false);
  }
}

}  // namespace arc
