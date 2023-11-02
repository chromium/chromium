// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WINDOW_MANAGER_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WINDOW_MANAGER_TEST_HELPER_H_

#include "base/unguessable_token.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"

namespace borealis {

// A helper class used to emulate the behaviour of the InstanceRegistry when
// windows are created/destroyed.
class ScopedTestWindow {
 public:
  ScopedTestWindow(std::unique_ptr<aura::Window> window,
                   borealis::BorealisWindowManager* manager);
  ~ScopedTestWindow();

  aura::Window* window() { return window_.get(); }

 private:
  base::UnguessableToken instance_id_;
  std::unique_ptr<aura::Window> window_;
  borealis::BorealisWindowManager* manager_;
};

// Creates a widget for use in testing.
std::unique_ptr<aura::Window> MakeWindow(std::string name);
std::unique_ptr<borealis::ScopedTestWindow> MakeAndTrackWindow(
    std::string name,
    borealis::BorealisWindowManager* manager);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WINDOW_MANAGER_TEST_HELPER_H_
