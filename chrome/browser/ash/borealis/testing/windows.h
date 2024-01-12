// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_TESTING_WINDOWS_H_
#define CHROME_BROWSER_ASH_BOREALIS_TESTING_WINDOWS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "ui/aura/window.h"

namespace views {
class Widget;
}

namespace borealis {

class BorealisWindowManager;

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
  raw_ptr<borealis::BorealisWindowManager> manager_;
};

// Creates a window for use in testing.
std::unique_ptr<aura::Window> MakeWindow(std::string name);
std::unique_ptr<borealis::ScopedTestWindow> MakeAndTrackWindow(
    std::string name,
    borealis::BorealisWindowManager* manager);

// Creates and displays a widget with the given |name|.
std::unique_ptr<views::Widget> CreateFakeWidget(std::string name,
                                                bool fullscreen = false);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_TESTING_WINDOWS_H_
