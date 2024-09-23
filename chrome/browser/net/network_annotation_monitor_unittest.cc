// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_annotation_monitor.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/regmon/regmon_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NetworkAnnotationMonitorTest, ReportTest) {
  constexpr int32_t kTestDisabledHashCode = 123;
  constexpr int32_t kTestAllowedHashCode = 456;
  content::BrowserTaskEnvironment task_environment;

  // Setup profile with the disabled hash code in blocklist pref.
  TestingProfileManager profile_manager_(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_.SetUp());
  profile_manager_.CreateTestingProfile("testing_profile", true);
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetDict(
      prefs::kNetworkAnnotationBlocklist,
      base::Value::Dict().Set(base::NumberToString(kTestDisabledHashCode),
                              true));

  // Disable secondary profiles pref since we skip reporting on lacros when this
  // is enabled.
  profile_manager_.local_state()->Get()->SetBoolean(
      prefs::kLacrosSecondaryProfilesAllowed, false);

  // Initialize fake Regmon D-Bus client. This fake client is used below to
  // verify that violations are reported.
  chromeos::RegmonClient::InitializeFake();

  NetworkAnnotationMonitor monitor;
  mojo::Remote<network::mojom::NetworkAnnotationMonitor> remote;
  remote.Bind(monitor.GetClient());

  remote->Report(kTestDisabledHashCode);
  remote->Report(kTestAllowedHashCode);
  monitor.FlushForTesting();

  // Disabled hash codes should trigger a violation.
  chromeos::RegmonClient::TestInterface* regmon_client =
      chromeos::RegmonClient::Get()->GetTestInterface();
  std::list<int32_t> expected_reported_hash_codes{kTestDisabledHashCode};
  EXPECT_EQ(regmon_client->GetReportedHashCodes(),
            expected_reported_hash_codes);
}

// Verify that GetClient() can be called multiple times. This simulates what
// happens when the Network Service crashes and restarts.
TEST(NetworkAnnotationMonitorTest, GetClientResetTest) {
  base::test::SingleThreadTaskEnvironment task_environment;
  NetworkAnnotationMonitor monitor;

  EXPECT_TRUE(monitor.GetClient().is_valid());
  EXPECT_TRUE(monitor.GetClient().is_valid());
}
