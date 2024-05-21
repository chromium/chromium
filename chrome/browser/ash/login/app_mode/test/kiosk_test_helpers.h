// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_TEST_HELPERS_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_TEST_HELPERS_H_

#include <memory>

#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"

namespace ash {
class FakeOwnerSettingsService;

// Common classes that can be used for kiosk mode testing.
// Waits for kiosk session to be initialized.
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

// Used to replace OwnerSettingsService.
class ScopedDeviceSettings {
 public:
  ScopedDeviceSettings();
  ~ScopedDeviceSettings();

  FakeOwnerSettingsService* owner_settings_service() {
    return owner_settings_service_.get();
  }

 private:
  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<FakeOwnerSettingsService> owner_settings_service_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_TEST_HELPERS_H_
