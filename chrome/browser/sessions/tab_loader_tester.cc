// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_loader_tester.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"

TabLoaderTester::TabLoaderTester() = default;

TabLoaderTester::TabLoaderTester(TabLoader* tab_loader)
    : tab_loader_(tab_loader) {}

TabLoaderTester::~TabLoaderTester() = default;

void TabLoaderTester::SetTabLoader(TabLoader* tab_loader) {
  tab_loader_ = tab_loader;
}

// static
void TabLoaderTester::SetMaxLoadedTabCountForTesting(size_t value) {
  TabLoader::SetMaxLoadedTabCountForTesting(value);
}

// static
void TabLoaderTester::SetConstructionCallbackForTesting(
    base::RepeatingCallback<void(TabLoader*)>* callback) {
  TabLoader::SetConstructionCallbackForTesting(callback);
}

void TabLoaderTester::SetMaxSimultaneousLoadsForTesting(size_t loading_slots) {
  tab_loader_->SetMaxSimultaneousLoadsForTesting(loading_slots);
}

void TabLoaderTester::SetTickClockForTesting(base::TickClock* tick_clock) {
  tab_loader_->SetTickClockForTesting(tick_clock);
}

void TabLoaderTester::MaybeLoadSomeTabsForTesting() {
  tab_loader_->MaybeLoadSomeTabsForTesting();
}

void TabLoaderTester::ForceLoadTimerFired() {
  tab_loader_->ForceLoadTimerFired();
}

void TabLoaderTester::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  tab_loader_->OnMemoryPressure(memory_pressure_level);
}

void TabLoaderTester::SetTabLoadingEnabled(bool enabled) {
  tab_loader_->SetTabLoadingEnabled(enabled);
}

bool TabLoaderTester::IsLoadingEnabled() const {
  return tab_loader_->IsLoadingEnabled();
}

size_t TabLoaderTester::force_load_delay_multiplier() const {
  return tab_loader_->force_load_delay_multiplier_;
}

base::TimeTicks TabLoaderTester::force_load_time() const {
  return tab_loader_->force_load_time_;
}

base::OneShotTimer& TabLoaderTester::force_load_timer() {
  return tab_loader_->force_load_timer_;
}

const TabLoader::TabVector& TabLoaderTester::tabs_to_load() const {
  return tab_loader_->tabs_to_load_;
}

size_t TabLoaderTester::scheduled_to_load_count() const {
  return tab_loader_->scheduled_to_load_count_;
}

// static
TabLoader* TabLoaderTester::shared_tab_loader() {
  return TabLoader::shared_tab_loader_;
}

resource_coordinator::SessionRestorePolicy* TabLoaderTester::GetPolicy() {
  return tab_loader_->delegate_->GetPolicyForTesting();
}

bool TabLoaderTester::IsSharedTabLoader() const {
  return tab_loader_ == TabLoader::shared_tab_loader_;
}

bool TabLoaderTester::HasTimedOutLoads() const {
  if (tab_loader_->tabs_loading_.empty())
    return false;
  base::TimeTicks expiry_time =
      tab_loader_->tabs_loading_.begin()->loading_start_time +
      tab_loader_->GetLoadTimeoutPeriod();
  return expiry_time <= tab_loader_->clock_->NowTicks();
}

void TabLoaderTester::WaitForTabLoadingEnabled() {
  base::RunLoop run_loop;
  TabLoader* tab_loader = tab_loader_;
  auto callback =
      base::BindLambdaForTesting([&run_loop, tab_loader](bool loading_enabled) {
        if (loading_enabled && tab_loader->IsLoadingEnabled())
          run_loop.Quit();
      });
  tab_loader_->SetTabLoadingEnabledCallbackForTesting(&callback);
  run_loop.Run();
  tab_loader_->SetTabLoadingEnabledCallbackForTesting(nullptr);
}
