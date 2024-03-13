// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/geolocation_access_level.h"
#include "base/synchronization/condition_variable.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/lacros/geolocation/system_geolocation_source_lacros.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

namespace {

class SystemGeolocationSourceLacrosTests : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    // The tests work only if the privacy hub flags are passed to ash.
    StartUniqueAshChrome(
        /*enabled_features=*/{"CrosPrivacyHubV0", "CrosPrivacyHub"},
        /*disabled_features=*/{},
        /*additional_cmdline_switches=*/{},
        /*bug_number_and_reason=*/
        {"b/267681869 Switch to shared ash when clipboard "
         "history refresh is enabled by default"});

    InProcessBrowserTest::SetUp();
  }

  // Checks whether the required crosapi elements are available in Ash.
  // Ash may not be compatible due to a version skew.
  // TODO(b/313605503): remove in M123
  bool AshIsCompatible() const {
    auto& prefs =
        chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>();

    base::test::TestFuture<std::optional<::base::Value>> future;
    prefs->GetPref(crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
                   future.GetCallback());

    auto out_value = future.Take();
    return out_value.has_value() && out_value->is_int();
  }
};

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceLacrosTests, PrefChange) {
  if (!AshIsCompatible()) {
    // As we are adding the crosapi change to ash in the same commit, we may be
    // missing the Pref when run with older versions of ash. Hence we'll skip
    // this test when the ash is not compatible.
    GTEST_SKIP() << "Skipping as the Ash is not compatible with this test.";
  }

  auto& prefs =
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>();

  // By default, the geolocation is allowed in ash.
  base::test::TestFuture<std::optional<::base::Value>> future;
  prefs->GetPref(crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
                 future.GetCallback());

  auto out_value = future.Take();
  ASSERT_TRUE(out_value.has_value());

  EXPECT_EQ(
      static_cast<ash::GeolocationAccessLevel>(out_value.value().GetInt()),
      ash::GeolocationAccessLevel::kAllowed);

  // Set up the system source to save the pref changes into a future object
  base::test::TestFuture<device::LocationSystemPermissionStatus> status;

  device::GeolocationSystemPermissionManager::GetInstance()
      ->SystemGeolocationSourceForTest()
      .RegisterPermissionUpdateCallback(
          base::BindRepeating(status.GetRepeatingCallback()));

  // Wait for status to be asynchronously updated.
  // Initial value should be to allow.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());
  // Change the value in ash.
  base::test::TestFuture<void> set_future;
  prefs->SetPref(
      crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
      ::base::Value(static_cast<int>(ash::GeolocationAccessLevel::kDisallowed)),
      set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());
  set_future.Clear();

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied, status.Take());

  // Change the value in ash.
  prefs->SetPref(
      crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
      ::base::Value(static_cast<int>(ash::GeolocationAccessLevel::kAllowed)),
      set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());
  set_future.Clear();

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());

  // Change the value in ash.
  prefs->SetPref(crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
                 ::base::Value(static_cast<int>(
                     ash::GeolocationAccessLevel::kOnlyAllowedForSystem)),
                 set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());
  set_future.Clear();

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied, status.Take());
}

// This works only if the privacy hub flags are passed to ash.
IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceLacrosTests,
                       IntegrationToBrowser) {
  if (!AshIsCompatible()) {
    // As we are adding the crosapi change to ash in the same commit, we may be
    // missing the Pref when run with older versions of ash. Hence we'll skip
    // this test when the preference is not available.
    GTEST_SKIP() << "Skipping as the Ash is not compatible with this test.";
  }

  class Observer
      : public device::GeolocationSystemPermissionManager::PermissionObserver {
   public:
    // device::GeolocationSystemPermissionManager::PermissionObserver:
    void OnSystemPermissionUpdated(
        device::LocationSystemPermissionStatus status) override {
      status_.GetRepeatingCallback().Run(std::move(status));
    }
    base::test::TestFuture<device::LocationSystemPermissionStatus> status_;
  };

  device::GeolocationSystemPermissionManager* manager =
      device::GeolocationSystemPermissionManager::GetInstance();
  ASSERT_TRUE(manager);

  Observer observer;
  manager->AddObserver(&observer);

  base::test::TestFuture<std::optional<::base::Value>> future;
  auto& prefs =
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>();

  // By default, the the geolocation is allowed in ash.
  prefs->GetPref(crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
                 future.GetCallback());

  // As we are adding the crosapi change to ash in the same commit, we may be
  // missing the Pref when run with older versions of ash. Hence we'll skip this
  // test when the preference is not available.
  auto out_value = future.Take();
  if (!out_value.has_value()) {
    GTEST_SKIP() << "Skipping as the geolocation pref is not available in the "
                    "current version of Ash";
  }

  // Initial value should be to allow.
  EXPECT_EQ(out_value.value().GetInt(),
            static_cast<int>(ash::GeolocationAccessLevel::kAllowed));
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Change the value in ash.
  base::test::TestFuture<void> set_future;
  prefs->SetPref(
      crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
      ::base::Value(static_cast<int>(ash::GeolocationAccessLevel::kDisallowed)),
      set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());
  set_future.Clear();

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            observer.status_.Take());

  // Change the value in ash.
  prefs->SetPref(
      crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
      ::base::Value(static_cast<int>(ash::GeolocationAccessLevel::kAllowed)),
      set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());
  set_future.Clear();

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  // Observer needs to be removed here because it is allocated on stack.
  manager->RemoveObserver(&observer);
}

}  // namespace
