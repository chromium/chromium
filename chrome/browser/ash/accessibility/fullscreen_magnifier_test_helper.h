// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_FULLSCREEN_MAGNIFIER_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_FULLSCREEN_MAGNIFIER_TEST_HELPER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"

class Profile;

namespace ash {

// Helper class for fullscreen magnifier tests.
class FullscreenMagnifierTestHelper {
 public:
  static void WaitForMagnifierJSReady(Profile* profile);

  // Set `center_position_on_load` to a non-zero point to load the magnifier
  // centered at a certain viewport each time LoadMagnifier is called.
  FullscreenMagnifierTestHelper(
      gfx::Point center_position_on_load = gfx::Point(0, 0));
  FullscreenMagnifierTestHelper(const FullscreenMagnifierTestHelper&) = delete;
  FullscreenMagnifierTestHelper& operator=(
      const FullscreenMagnifierTestHelper&) = delete;
  ~FullscreenMagnifierTestHelper();

  // Loads the magnifier and waits for load complete before returning.
  void LoadMagnifier(Profile* profile);

  // Moves the magnifier window and waits for the bounds to change.
  void MoveMagnifierWindow(int x_center, int y_center);

  // Waits for the next time the magnifier bounds change.
  void WaitForMagnifierBoundsChanged();

 private:
  void WaitForMagnifierBoundsChangedTo(gfx::Point center_point);
  void OnMagnifierBoundsChanged();

  gfx::Point center_position_on_load_;
  base::OnceClosure bounds_changed_waiter_;
  base::WeakPtrFactory<FullscreenMagnifierTestHelper> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_FULLSCREEN_MAGNIFIER_TEST_HELPER_H_
