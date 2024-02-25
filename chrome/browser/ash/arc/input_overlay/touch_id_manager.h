// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_ID_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_ID_MANAGER_H_

#include <optional>

#include "base/no_destructor.h"

namespace arc::input_overlay {

// TouchIdManager manages the touch id for input overlay feature.
class TouchIdManager {
 public:
  static TouchIdManager* GetInstance();

  TouchIdManager(const TouchIdManager&) = delete;
  TouchIdManager& operator=(const TouchIdManager&) = delete;

  // Return the minimum non-negative integer which is not used.
  // For example,
  // Used touch IDs: {0, 1, 2, 3}, return 4.
  // Used touch IDs: {1, 2, 3, 4}, return 0.
  // Used touch IDs: {0, 1, 3, 4},return 2.
  // Used touch IDs: {0, 1, 5, 6}, return 2.
  int ObtainTouchID();

  // Unset corresponding bit in touch_ids_.
  void ReleaseTouchID(int touch_id);

 private:
  friend class base::NoDestructor<TouchIdManager>;
  friend class TouchIdManagerTest;
  TouchIdManager();
  ~TouchIdManager();

  // Touch ID bits. If the bit is set, it means the touch_ID is used. Touch ID
  // is set from right bit.
  int touch_ids_ = 0;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_ID_MANAGER_H_
