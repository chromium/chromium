// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_TAB_ADDED_WAITER_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_TAB_ADDED_WAITER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"

class Profile;
namespace tabs {
class TabInterface;
}

namespace glic {

// Waits for a tab to be added using GlicTabObserver.
class GlicTestTabAddedWaiter {
 public:
  explicit GlicTestTabAddedWaiter(Profile* profile);
  ~GlicTestTabAddedWaiter();

  GlicTestTabAddedWaiter(const GlicTestTabAddedWaiter&) = delete;
  GlicTestTabAddedWaiter& operator=(const GlicTestTabAddedWaiter&) = delete;

  // Waits for a tab to be added and returns it.
  tabs::TabInterface* Wait();

 private:
  void OnTabEvent(const GlicTabEvent& event);

  base::RunLoop run_loop_;
  raw_ptr<tabs::TabInterface> new_tab_ = nullptr;
  std::unique_ptr<GlicTabObserver> tab_observer_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_TAB_ADDED_WAITER_H_
