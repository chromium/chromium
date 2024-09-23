// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/signin_data_counter.h"
#include "chrome/browser/browsing_data/counters/site_data_counter.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browsing_data/counters/tabs_counter.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/strings/string_split.h"
#include "chrome/browser/browsing_data/counters/hosted_apps_counter.h"
#endif

namespace browsing_data_counter_utils {

class BrowsingDataCounterUtilsTest : public testing::Test {
 public:
  ~BrowsingDataCounterUtilsTest() override = default;

  TestingProfile* GetProfile() { return &profile_; }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(BrowsingDataCounterUtilsTest, CacheCounterResult) {
#if BUILDFLAG(IS_ANDROID)
  scoped_feature_list_.InitAndDisableFeature(
      chrome::android::kQuickDeleteForAndroid);
#endif

  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());
  const int kBytesInAMegabyte = 1024 * 1024;

  // Test the output for various forms of CacheResults.
  const struct TestCase {
    int bytes;
    bool is_upper_limit;
    bool is_basic_tab;
    std::string expected_output;
  } kTestCases[] = {
      {42, false, false, "Less than 1 MB"},
      {42, false, true,
       "Frees up less than 1 MB. Some sites may load more slowly on your next "
       "visit."},
      {static_cast<int>(2.312 * kBytesInAMegabyte), false, false, "2.3 MB"},
      {static_cast<int>(2.312 * kBytesInAMegabyte), false, true,
       "Frees up 2.3 MB. Some sites may load more slowly on your next visit."},
      {static_cast<int>(2.312 * kBytesInAMegabyte), true, false,
       "Less than 2.3 MB"},
      {static_cast<int>(2.312 * kBytesInAMegabyte), true, true,
       "Frees up less than 2.3 MB. Some sites may load more slowly on your "
       "next visit."},
      {static_cast<int>(500.2 * kBytesInAMegabyte), false, false, "500 MB"},
      {static_cast<int>(500.2 * kBytesInAMegabyte), true, false,
       "Less than 500 MB"},
  };

  for (const TestCase& test_case : kTestCases) {
    CacheCounter counter(GetProfile());
    browsing_data::ClearBrowsingDataTab tab =
        test_case.is_basic_tab ? browsing_data::ClearBrowsingDataTab::BASIC
                               : browsing_data::ClearBrowsingDataTab::ADVANCED;
    counter.Init(GetProfile()->GetPrefs(), tab,
                 browsing_data::BrowsingDataCounter::ResultCallback());
    CacheCounter::CacheResult result(&counter, test_case.bytes,
                                     test_case.is_upper_limit);
    SCOPED_TRACE(base::StringPrintf(
        "Test params: %d bytes, %d is_upper_limit, %d is_basic_tab.",
        test_case.bytes, test_case.is_upper_limit, test_case.is_basic_tab));

    std::u16string output =
        GetChromeCounterTextFromResult(&result, GetProfile());
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

#if BUILDFLAG(IS_ANDROID)
// Tests the output of the hosted apps counter.
TEST_F(BrowsingDataCounterUtilsTest, QuickDeleteAdvancedCacheCounterResult) {
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kQuickDeleteForAndroid);

  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());
  const int kBytesInAMegabyte = 1024 * 1024;

  // Test the output for various forms of CacheResults.
  const struct TestCase {
    int bytes;
    bool is_upper_limit;
    std::string expected_output;
  } kTestCases[] = {
      {42, false,
       "Less than 1 MB. Some sites may load more slowly on your next "
       "visit."},
      {static_cast<int>(2.312 * kBytesInAMegabyte), false,
       "2.3 MB. Some sites may load more slowly on your next "
       "visit."},
      {static_cast<int>(2.312 * kBytesInAMegabyte), true,
       "Less than 2.3 MB. Some sites may load more slowly on your next "
       "visit."}};

  for (const TestCase& test_case : kTestCases) {
    CacheCounter counter(GetProfile());
    browsing_data::ClearBrowsingDataTab tab =
        browsing_data::ClearBrowsingDataTab::ADVANCED;
    counter.Init(GetProfile()->GetPrefs(), tab,
                 browsing_data::BrowsingDataCounter::ResultCallback());
    CacheCounter::CacheResult result(&counter, test_case.bytes,
                                     test_case.is_upper_limit);
    SCOPED_TRACE(base::StringPrintf("Test params: %d bytes, %d is_upper_limit",
                                    test_case.bytes, test_case.is_upper_limit));

    std::u16string output =
        GetChromeCounterTextFromResult(&result, GetProfile());
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Tests the complex output of the hosted apps counter.
TEST_F(BrowsingDataCounterUtilsTest, HostedAppsCounterResult) {
  HostedAppsCounter counter(GetProfile());

  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());

  // Test the output for various numbers of hosted apps.
  const struct TestCase {
    std::string apps_list;
    std::string expected_output;
  } kTestCases[] = {
      {"", "None"},
      {"App1", "1 app (App1)"},
      {"App1, App2", "2 apps (App1, App2)"},
      {"App1, App2, App3", "3 apps (App1, App2, and 1 more)"},
      {"App1, App2, App3, App4", "4 apps (App1, App2, and 2 more)"},
      {"App1, App2, App3, App4, App5", "5 apps (App1, App2, and 3 more)"},
  };

  for (const TestCase& test_case : kTestCases) {
    // Split the list of installed apps by commas.
    std::vector<std::string> apps =
        base::SplitString(test_case.apps_list, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    // The first two apps in the list are used as examples.
    std::vector<std::string> examples;
    examples.assign(apps.begin(),
                    apps.begin() + (apps.size() > 2 ? 2 : apps.size()));

    HostedAppsCounter::HostedAppsResult result(&counter, apps.size(), examples);

    std::u16string output =
        GetChromeCounterTextFromResult(&result, GetProfile());
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}
#endif

// Tests the output for "Passwords and passkeys" on the advanced tab.
TEST_F(BrowsingDataCounterUtilsTest, DeletePasswordsAndSigninData) {
  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());

  auto password_store =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  password_store->Init(GetProfile()->GetPrefs(),
                       /*affiliated_match_helper=*/nullptr);

  // This counter does not really count anything; we just need a reference to
  // pass to the SigninDataResult ctor.
  browsing_data::SigninDataCounter counter(
      /*profile_store=*/password_store, /*account_store=*/nullptr,
      /*pref_service=*/nullptr, SyncServiceFactory::GetForProfile(GetProfile()),
      /*opt_platform_credential_store=*/nullptr);

  // Use a separate struct for input to make test cases easier to read after
  // auto formatting.
  struct TestInput {
    int num_passwords;
    int num_account_passwords;
    int num_webauthn_credentials;
    bool sync_enabled;
    std::vector<std::string> domain_examples;
    std::vector<std::string> account_domain_examples;
  };
  // Sign-in data is referred to as passkeys on macOS only currently.
  auto signin_data_str = [](size_t n) {
    DCHECK(n > 0);
    return n == 1 ? "sign-in data for 1 account"
                  : base::StringPrintf("sign-in data for %zu accounts", n);
  };
  const struct TestCase {
    TestInput input;
    std::string expected_output;
  } kTestCases[] = {
      {{0, 0, 0, false, {}, {}}, "None"},
      {{0, 0, 0, true, {}, {}}, "None"},
      {{1, 0, 0, false, {"a.com"}, {}}, "1 password (for a.com)"},
      {{1, 0, 0, true, {"a.com"}, {}}, "1 password (for a.com, synced)"},
      {{2, 0, 0, false, {"a.com"}, {}}, "2 passwords (for a.com)"},
      {{2, 0, 0, false, {"a.com", "b.com"}, {}},
       "2 passwords (for a.com, b.com)"},
      {{2, 0, 0, true, {"a.com", "b.com"}, {}},
       "2 passwords (for a.com, b.com, synced)"},
      {{0, 2, 0, false, {}, {"x.com", "y.com"}},
       "2 passwords in your account (for x.com, y.com)"},
      {{0, 0, 1, false, {}, {}}, signin_data_str(1)},
      {{0, 0, 1, true, {}, {}}, signin_data_str(1)},
      {{0, 0, 2, false, {}, {}}, signin_data_str(2)},
      {{0, 0, 2, true, {}, {}}, signin_data_str(2)},
      {{1, 0, 2, false, {"a.de"}, {}},
       "1 password (for a.de); " + signin_data_str(2)},
      {{2, 0, 1, false, {"a.de", "b.de"}, {}},
       "2 passwords (for a.de, b.de); " + signin_data_str(1)},
      {{2, 0, 3, true, {"a.de", "b.de"}, {}},
       "2 passwords (for a.de, b.de, synced); " + signin_data_str(3)},
      {{4, 0, 2, false, {"a.de", "b.de"}, {}},
       "4 passwords (for a.de, b.de, and 2 more); " + signin_data_str(2)},
      {{6, 0, 0, true, {"a.de", "b.de", "c.de", "d.de", "e.de", "f.de"}, {}},
       "6 passwords (for a.de, b.de, and 4 more, synced)"},
      {{2, 1, 1, false, {"a.de", "b.de"}, {"c.de"}},
       "2 passwords (for a.de, b.de); 1 password in your account (for "
       "c.de); " +
           signin_data_str(1)},
  };
  for (const auto& test_case : kTestCases) {
    auto& input = test_case.input;
    browsing_data::SigninDataCounter::SigninDataResult result(
        &counter, input.num_passwords, input.num_account_passwords,
        input.num_webauthn_credentials, input.sync_enabled,
        input.domain_examples, input.account_domain_examples);
    SCOPED_TRACE(base::StringPrintf(
        "Test params: %d password(s), %d account password(s), %d data, %d sync",
        input.num_passwords, input.num_account_passwords,
        input.num_webauthn_credentials, input.sync_enabled));
    std::string output = base::UTF16ToASCII(
        GetChromeCounterTextFromResult(&result, GetProfile()));
    EXPECT_EQ(test_case.expected_output, output);
  }

  password_store->ShutdownOnUIThread();
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(BrowsingDataCounterUtilsTest, TabsCounterResult) {
  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());
  browsing_data::ClearBrowsingDataTab tab =
      browsing_data::ClearBrowsingDataTab::ADVANCED;

  // Test the output for various forms of CacheResults.
  const struct TestCase {
    int tab_count;
    int window_count;
    std::string expected_output;
  } kTestCases[] = {
      {0, 0, "None"},
      {1, 0, "1 tab on this device"},
      {5, 1, "5 tabs on this device"},
      {5, 2, "5 tabs from 2 windows on this device"},
  };

  for (const TestCase& test_case : kTestCases) {
    TabsCounter counter(GetProfile());
    counter.Init(GetProfile()->GetPrefs(), tab,
                 browsing_data::BrowsingDataCounter::ResultCallback());
    TabsCounter::TabsResult result(&counter, test_case.tab_count,
                                   test_case.window_count);
    SCOPED_TRACE(
        base::StringPrintf("Test params: %d tab_count, %d window_count.",
                           test_case.tab_count, test_case.window_count));

    std::u16string output =
        GetChromeCounterTextFromResult(&result, GetProfile());
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class CookieBrowsingDataCounterUtilsTest : public BrowsingDataCounterUtilsTest {
 public:
  enum class SigninState {
    kSignedOut,
    kAccountAware,
    kImplicitSignin,  // Legacy Dice automatic signin.
    kExplicitSignin,
    kSigninPending,
    kSyncing,
    kSyncPaused
  };

  struct TestCase {
    SigninState signin_state = SigninState::kSignedOut;
    bool expects_exception_text = false;
  };

  void SetSignedOutState(syncer::TestSyncService* test_sync_service) {
    test_sync_service->SetSignedOut();
  }

  void SetAccountAwareState(signin::IdentityTestEnvironment* identity_test_env,
                            syncer::TestSyncService* test_sync_service) {
    identity_test_env->MakeAccountAvailable("user@gmail.com");
    test_sync_service->SetSignedOut();
    ASSERT_FALSE(identity_test_env->identity_manager()->HasPrimaryAccount(
        signin::ConsentLevel::kSignin));
    ASSERT_EQ(1u, identity_test_env->identity_manager()
                      ->GetAccountsWithRefreshTokens()
                      .size());
  }

  CoreAccountInfo SetSignedInState(
      signin::ConsentLevel consent_level,
      signin::IdentityTestEnvironment* identity_test_env,
      syncer::TestSyncService* test_sync_service,
      PrefService* prefs,
      bool explicit_signin) {
    CoreAccountInfo account_info =
        identity_test_env->MakePrimaryAccountAvailable("user@gmail.com",
                                                       consent_level);
    test_sync_service->SetSignedIn(consent_level, account_info);
    prefs->SetBoolean(prefs::kExplicitBrowserSignin, explicit_signin);
    return account_info;
  }

  void SetSigninPendingState(signin::IdentityTestEnvironment* identity_test_env,
                             syncer::TestSyncService* test_sync_service,
                             PrefService* prefs) {
    CoreAccountInfo account_info =
        SetSignedInState(signin::ConsentLevel::kSignin, identity_test_env,
                         test_sync_service, prefs,
                         /*explicit_signin=*/true);
    identity_test_env->UpdatePersistentErrorOfRefreshTokenForAccount(
        account_info.account_id,
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER));
    ASSERT_TRUE(
        identity_test_env->identity_manager()
            ->GetErrorStateOfRefreshTokenForAccount(account_info.account_id)
            .IsPersistentError());
  }

  void SetSyncPausedState(signin::IdentityTestEnvironment* identity_test_env,
                          syncer::TestSyncService* test_sync_service,
                          PrefService* prefs) {
    CoreAccountInfo account_info =
        SetSignedInState(signin::ConsentLevel::kSync, identity_test_env,
                         test_sync_service, prefs,
                         /*explicit_signin=*/true);
    identity_test_env->UpdatePersistentErrorOfRefreshTokenForAccount(
        account_info.account_id,
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER));
    ASSERT_TRUE(
        identity_test_env->identity_manager()
            ->GetErrorStateOfRefreshTokenForAccount(account_info.account_id)
            .IsPersistentError());
  }

  void VerifyTestCase(const TestCase& test_case) {
    SCOPED_TRACE(base::StringPrintf("Test params: %d signin_state.",
                                    static_cast<int>(test_case.signin_state)));
    // Setup the signin state.
    std::unique_ptr<TestingProfile> testing_profile =
        IdentityTestEnvironmentProfileAdaptor::
            CreateProfileForIdentityTestEnvironment();
    auto identity_test_env_adaptor =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            testing_profile.get());
    signin::IdentityTestEnvironment* identity_test_env =
        identity_test_env_adaptor->identity_test_env();
    syncer::TestSyncService* test_sync_service =
        SyncServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
            testing_profile.get(), base::BindOnce([](content::BrowserContext*) {
              return std::make_unique<syncer::TestSyncService>();
            }));

    switch (test_case.signin_state) {
      case SigninState::kSignedOut:
        SetSignedOutState(test_sync_service);
        break;
      case SigninState::kAccountAware:
        SetAccountAwareState(identity_test_env, test_sync_service);
        break;
      case SigninState::kExplicitSignin:
        SetSignedInState(signin::ConsentLevel::kSignin, identity_test_env,
                         test_sync_service, testing_profile->GetPrefs(),
                         /*explicit_signin=*/true);
        break;
      case SigninState::kImplicitSignin:
        SetSignedInState(signin::ConsentLevel::kSignin, identity_test_env,
                         test_sync_service, testing_profile->GetPrefs(),
                         /*explicit_signin=*/false);
        break;
      case SigninState::kSigninPending:
        SetSigninPendingState(identity_test_env, test_sync_service,
                              testing_profile->GetPrefs());
        break;
      case SigninState::kSyncing:
        SetSignedInState(signin::ConsentLevel::kSync, identity_test_env,
                         test_sync_service, testing_profile->GetPrefs(),
                         /*explicit_signin=*/true);
        break;
      case SigninState::kSyncPaused:
        SetSyncPausedState(identity_test_env, test_sync_service,
                           testing_profile->GetPrefs());
        break;
    }

    // Run the test case.
    SiteDataCounter counter(testing_profile.get());
    counter.Init(testing_profile->GetPrefs(),
                 browsing_data::ClearBrowsingDataTab::ADVANCED,
                 browsing_data::BrowsingDataCounter::ResultCallback());

    browsing_data::BrowsingDataCounter::FinishedResult result0(&counter, 0);
    EXPECT_EQ(GetChromeCounterTextFromResult(&result0, testing_profile.get()),
              u"None");

    browsing_data::BrowsingDataCounter::FinishedResult result1(&counter, 1);
    browsing_data::BrowsingDataCounter::FinishedResult result42(&counter, 42);
    std::u16string output1 =
        GetChromeCounterTextFromResult(&result1, testing_profile.get());
    std::u16string output42 =
        GetChromeCounterTextFromResult(&result42, testing_profile.get());
    if (test_case.expects_exception_text) {
      EXPECT_EQ(output1,
                u"From 1 site (you'll stay signed in to your Google Account)");
      EXPECT_EQ(
          output42,
          u"From 42 sites (you'll stay signed in to your Google Account)");
    } else {
      EXPECT_EQ(output1, u"From 1 site ");
      EXPECT_EQ(output42, u"From 42 sites ");
    }
  }
};

TEST_F(CookieBrowsingDataCounterUtilsTest,
       CookieCounterResultExplicitSigninDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      switches::kExplicitBrowserSigninUIOnDesktop);
  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());

  // Test the output for various forms of cookie results. Some signin states do
  // not exist when explicit signin is off.
  const struct TestCase kTestCases[] = {
      {.signin_state = SigninState::kSignedOut,
       .expects_exception_text = false},
      {.signin_state = SigninState::kImplicitSignin,
       .expects_exception_text = false},
      {.signin_state = SigninState::kSyncing, .expects_exception_text = true},
      {.signin_state = SigninState::kSyncPaused,
       .expects_exception_text = false},
  };

  for (const TestCase& test_case : kTestCases) {
    VerifyTestCase(test_case);
  }
}

TEST_F(CookieBrowsingDataCounterUtilsTest, CookieCounterResult) {
  scoped_feature_list_.InitAndEnableFeature(
      switches::kExplicitBrowserSigninUIOnDesktop);
  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());

  // Test the output for various forms of cookie results.
  const struct TestCase kTestCases[] = {
      {.signin_state = SigninState::kSignedOut,
       .expects_exception_text = false},
      {.signin_state = SigninState::kAccountAware,
       .expects_exception_text = false},
      {.signin_state = SigninState::kImplicitSignin,
       .expects_exception_text = false},
      {.signin_state = SigninState::kExplicitSignin,
       .expects_exception_text = true},
      {.signin_state = SigninState::kSigninPending,
       .expects_exception_text = false},
      {.signin_state = SigninState::kSyncing, .expects_exception_text = true},
      {.signin_state = SigninState::kSyncPaused,
       .expects_exception_text = false},
  };

  for (const TestCase& test_case : kTestCases) {
    VerifyTestCase(test_case);
  }
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace browsing_data_counter_utils
