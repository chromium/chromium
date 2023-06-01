// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_LOADER_TESTER_H_
#define CHROME_BROWSER_SESSIONS_TAB_LOADER_TESTER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/resource_coordinator/session_restore_policy.h"
#include "chrome/browser/sessions/tab_loader.h"

// Wraps a TabLoader and exposes helper functions for testing. See tab_loader.h
// for full documentation.
class TabLoaderTester {
 public:
  using TabVector = TabLoader::TabVector;
  using TabSet = TabLoader::TabSet;

  TabLoaderTester();
  explicit TabLoaderTester(TabLoader* tab_loader);

  TabLoaderTester(const TabLoaderTester&) = delete;
  TabLoaderTester& operator=(const TabLoaderTester&) = delete;

  ~TabLoaderTester();

  void SetTabLoader(TabLoader*);
  TabLoader* tab_loader() { return tab_loader_; }

  // Test only functions exposed from TabLoader.
  static void SetMaxLoadedTabCountForTesting(size_t value);
  static void SetConstructionCallbackForTesting(
      base::RepeatingCallback<void(TabLoader*)>* callback);
  void SetMaxSimultaneousLoadsForTesting(size_t loading_slots);
  void SetTickClockForTesting(base::TickClock* tick_clock);
  void MaybeLoadSomeTabsForTesting();

  // Additional exposed TabLoader functions.
  void ForceLoadTimerFired();
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);
  void SetTabLoadingEnabled(bool enabled);
  bool IsLoadingEnabled() const;

  // Accessors to TabLoader internals.
  size_t force_load_delay_multiplier() const;
  base::TimeTicks force_load_time() const;
  base::OneShotTimer& force_load_timer();
  const TabVector& tabs_to_load() const;
  const TabSet& tabs_load_initiated() const;
  size_t scheduled_to_load_count() const;
  static TabLoader* shared_tab_loader();

  // Returns the session restore policy engine that is currently being used.
  resource_coordinator::SessionRestorePolicy* GetPolicy();

  // Additional helper functions.
  bool IsSharedTabLoader() const;
  bool HasTimedOutLoads() const;
  void WaitForTabLoadingEnabled();

 private:
  raw_ptr<TabLoader, DanglingUntriaged> tab_loader_ = nullptr;
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_LOADER_TESTER_H_
