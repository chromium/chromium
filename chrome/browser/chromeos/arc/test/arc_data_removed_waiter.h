// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TEST_ARC_DATA_REMOVED_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TEST_ARC_DATA_REMOVED_WAITER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"

namespace base {
class RunLoop;
}

namespace arc {

// Waits for ARC data has been removed.
class ArcDataRemovedWaiter : public ArcSessionManager::Observer {
 public:
  ArcDataRemovedWaiter();
  ~ArcDataRemovedWaiter() override;

  // Waits until ARC data is removed. Waiting is end once ArcSessionManager
  // sends OnArcDataRemoved notification.
  void Wait();

 private:
  // ArcSessionManager::Observer:
  void OnArcDataRemoved() override;

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ArcDataRemovedWaiter);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TEST_ARC_DATA_REMOVED_WAITER_H_
