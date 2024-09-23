// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENDED_UPDATES_TEST_SCOPED_EXTENDED_UPDATES_CONTROLLER_H_
#define CHROME_BROWSER_ASH_EXTENDED_UPDATES_TEST_SCOPED_EXTENDED_UPDATES_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"

namespace ash {

// Test helper class that replaces the global controller instance with the one
// given on construction, and restores the original controller on destruction.
class ScopedExtendedUpdatesController {
 public:
  explicit ScopedExtendedUpdatesController(
      ExtendedUpdatesController* controller);
  ScopedExtendedUpdatesController(const ScopedExtendedUpdatesController&) =
      delete;
  ScopedExtendedUpdatesController& operator=(
      const ScopedExtendedUpdatesController&) = delete;
  virtual ~ScopedExtendedUpdatesController();

 private:
  raw_ptr<ExtendedUpdatesController> original_controller_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTENDED_UPDATES_TEST_SCOPED_EXTENDED_UPDATES_CONTROLLER_H_
