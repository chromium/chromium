// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/repeating_test_future.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/lacros/geolocation/system_geolocation_source_lacros.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/prefs.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"

namespace {

using SystemGeolocationSourceLacrosTests = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceLacrosTests, PrefChange) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::Prefs>());

  absl::optional<::base::Value> out_value;
  crosapi::mojom::PrefsAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>().get());

  // By default, the the geolocation is allowed in ash.
  async_waiter.GetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                       &out_value);

  // As we are adding the crosapi change to ash in the same commit, we may be
  // missing the Pref when run with older versions of ash. Hence we'll skip this
  // test when the preference is not available.
  if (!out_value.has_value()) {
    GTEST_SKIP() << "Skipping as the geolocation pref is not available in the "
                    "current version of Ash";
  }

  ASSERT_TRUE(out_value.has_value());
  ASSERT_TRUE(out_value.value().GetBool());

  // Set up the system source to save the pref changes into a future object
  SystemGeolocationSourceLacros source;
  base::test::RepeatingTestFuture<device::LocationSystemPermissionStatus>
      status;

  source.RegisterPermissionUpdateCallback(status.GetCallback());
  // Wait for status to be asynchronously updated.

  // Initial value should be to allow.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());

  // Change the value in ash.
  async_waiter.SetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                       ::base::Value(false));

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied, status.Take());

  // Change the value in ash.
  async_waiter.SetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                       ::base::Value(true));

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());
}

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceLacrosTests,
                       IntegrationToBrowser) {
  class Observer : public device::GeolocationManager::PermissionObserver {
   public:
    // device::GeolocationManager::PermissionObserver:
    void OnSystemPermissionUpdated(
        device::LocationSystemPermissionStatus status) override {
      status_.AddValue(std::move(status));
    }
    base::test::RepeatingTestFuture<device::LocationSystemPermissionStatus>
        status_;
  };

  device::GeolocationManager* manager =
      g_browser_process->platform_part()->geolocation_manager();
  ASSERT_TRUE(manager);

  Observer observer;
  manager->AddObserver(&observer);

  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::Prefs>());

  absl::optional<::base::Value> out_value;
  crosapi::mojom::PrefsAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>().get());

  // By default, the the geolocation is allowed in ash.
  async_waiter.GetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                       &out_value);

  // As we are adding the crosapi change to ash in the same commit, we may be
  // missing the Pref when run with older versions of ash. Hence we'll skip this
  // test when the preference is not available.
  if (!out_value.has_value()) {
    GTEST_SKIP() << "Skipping as the geolocation pref is not available in the "
                    "current version of Ash";
  }

  ASSERT_TRUE(out_value.has_value());
  ASSERT_TRUE(out_value.value().GetBool());

  // Initial value should be to allow.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Change the value in ash.
  async_waiter.SetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                       ::base::Value(false));

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            observer.status_.Take());

  // Change the value in ash.
  async_waiter.SetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                       ::base::Value(true));

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());
}

}  // namespace
