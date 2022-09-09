// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_pip_window_throttle_observer.h"

#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "base/check.h"
#include "base/ranges/algorithm.h"
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
    : ThrottleObserver("ArcPipWindowIsVisible") {}

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
  ThrottleObserver::StopObserving();
  auto* const container = GetPipContainer();
  if (!container)  // for testing
    return;
  container->RemoveObserver(this);
}

void ArcPipWindowThrottleObserver::OnWindowAdded(aura::Window* window) {
  if (ash::IsArcWindow(window))
    SetActive(true);
}

void ArcPipWindowThrottleObserver::OnWindowRemoved(aura::Window* window) {
  // Check if there are any ARC windows left in the PipContainer. An old PIP
  // window may be removed after a new one is added.
  auto* const container = GetPipContainer();
  if (!container ||
      base::ranges::none_of(container->children(), &ash::IsArcWindow)) {
    SetActive(false);
  }
}

void ArcPipWindowThrottleObserver::OnWindowDestroying(aura::Window* window) {
  SetActive(false);
  StopObserving();
}

}  // namespace arc
