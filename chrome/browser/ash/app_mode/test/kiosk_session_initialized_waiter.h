// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_SESSION_INITIALIZED_WAITER_H_
#define CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_SESSION_INITIALIZED_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"

namespace ash {

// Waits for kiosk session to be initialized in tests.
class KioskSessionInitializedWaiter : public KioskAppManagerObserver {
 public:
  KioskSessionInitializedWaiter();
  ~KioskSessionInitializedWaiter() override;
  KioskSessionInitializedWaiter(const KioskSessionInitializedWaiter&) = delete;
  KioskSessionInitializedWaiter& operator=(
      const KioskSessionInitializedWaiter&) = delete;

  void Wait();

 private:
  // KioskAppManagerObserver:
  void OnKioskSessionInitialized() override;

  base::ScopedMultiSourceObservation<KioskAppManagerBase,
                                     KioskAppManagerObserver>
      scoped_observations_{this};
  base::RunLoop run_loop_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_SESSION_INITIALIZED_WAITER_H_
