// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_metrics_id_allocator.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/base/signin_prefs.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin {

class AccountMetricsIdAllocatorBrowserTest : public SigninBrowserTestBase {
 public:
  AccountMetricsIdAllocatorBrowserTest() = default;
  ~AccountMetricsIdAllocatorBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(AccountMetricsIdAllocatorBrowserTest, PersistenceTest) {
  base::HistogramTester histogram_tester;
  // Seed a test account.
  CoreAccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("user@gmail.com");
  GaiaId gaia_id = account_info.gaia;

  PrefService* pref_service = GetProfile()->GetPrefs();

  SigninPrefs signin_prefs(*pref_service);

  std::optional<int> id = GetOrAllocateAccountMetricsId(signin_prefs, gaia_id);
  ASSERT_TRUE(id.has_value());
  EXPECT_EQ(id.value(), 0);  // First ID is 0

  // Verify preference is persisted in the profile's actual PrefService.
  EXPECT_EQ(signin_prefs.GetNextAccountMetricsUnassignedId(), 1);

  // Verify SigninPrefs has it too.
  EXPECT_EQ(signin_prefs.GetAccountMetricsId(gaia_id).value_or(-1), 0);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Signin.AccountInPref.AssignedId", 0, 1);
}

IN_PROC_BROWSER_TEST_F(AccountMetricsIdAllocatorBrowserTest, CapReached) {
  PrefService* pref_service = GetProfile()->GetPrefs();
  SigninPrefs signin_prefs(*pref_service);

  // Allocate 100 IDs (0 to 99).
  for (int i = 0; i <= 99; ++i) {
    CoreAccountInfo account_info = identity_test_env()->MakeAccountAvailable(
        "user" + base::NumberToString(i) + "@gmail.com");
    GetOrAllocateAccountMetricsId(signin_prefs, account_info.gaia);
  }

  EXPECT_EQ(signin_prefs.GetNextAccountMetricsUnassignedId(), 100);

  // 101st account should return nullopt.
  CoreAccountInfo account_info_100 =
      identity_test_env()->MakeAccountAvailable("user100@gmail.com");
  std::optional<int> id =
      GetOrAllocateAccountMetricsId(signin_prefs, account_info_100.gaia);
  EXPECT_FALSE(id.has_value());

  EXPECT_EQ(signin_prefs.GetNextAccountMetricsUnassignedId(), 100);
  EXPECT_TRUE(signin_prefs.IsAccountMetricsIdCapped(account_info_100.gaia));
}

IN_PROC_BROWSER_TEST_F(AccountMetricsIdAllocatorBrowserTest,
                       CrossProfileIndependence) {
  // Main profile setup.
  PrefService* pref_service1 = GetProfile()->GetPrefs();
  SigninPrefs signin_prefs1(*pref_service1);

  CoreAccountInfo account_info1 =
      identity_test_env()->MakeAccountAvailable("user1@gmail.com");
  std::optional<int> id1 =
      GetOrAllocateAccountMetricsId(signin_prefs1, account_info1.gaia);
  EXPECT_EQ(id1.value_or(-1), 0);

  // Create a second profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile2 =
      profiles::testing::CreateProfileSync(profile_manager, profile_path);

  PrefService* pref_service2 = profile2.GetPrefs();
  SigninPrefs signin_prefs2(*pref_service2);

  // Second profile should start from 0 again.
  std::optional<int> id2 =
      GetOrAllocateAccountMetricsId(signin_prefs2, account_info1.gaia);
  EXPECT_EQ(id2.value_or(-1), 0);

  // Verify independence of global counter.
  EXPECT_EQ(signin_prefs1.GetNextAccountMetricsUnassignedId(), 1);
  EXPECT_EQ(signin_prefs2.GetNextAccountMetricsUnassignedId(), 1);
}

}  // namespace signin
