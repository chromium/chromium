// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"

#include "base/base64.h"
#include "base/metrics/field_trial.h"
#include "base/strings/escape.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/hashprefix_realtime/ohttp_key_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/variations/pref_names.h"
#include "components/variations/seed_reader_writer.h"
#include "content/public/test/browser_test.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#endif

namespace safe_browsing {

namespace {
const char kTestOhttpKey[] = "TestOhttpKey";
// Base64-encoded dummy OHTTP key configs from the RFC 9458 Appendix A example,
// needed to pass OhttpKeyService's key config parsing.
constexpr char kEncodedTestOhttpKey[] =
    "AQAgMeHwWnQBAhFSIOmvkY9zhnSuyV9U224E63Baro55gVUACAABAAEAAQAD";
constexpr char kRotatedEncodedTestOhttpKey[] =
    "AgAgMeHwWnQBAhFSIOmvkY9zhnSuyV9U224E63Baro55gVUACAABAAEAAQAD";

GURL GetTestKeyFetchUrl() {
  GURL key_fetch_url(kHashPrefixRealTimeLookupsKeyFetchUrl.Get());
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    key_fetch_url = key_fetch_url.Resolve(
        "?key=" + base::EscapeQueryParamValue(api_key, /*use_plus=*/true));
  }
  return key_fetch_url;
}
}  // namespace

class SafeBrowsingOhttpKeyServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingOhttpKeyServiceTest() {
    // TODO(https://crbug.com/564792943): Update tests so they work with the
    // SeedFile.
    base::FieldTrialList::CreateFieldTrial(variations::kSeedFileTrial,
                                           variations::kControlGroup);
  }
  SafeBrowsingOhttpKeyServiceTest(const SafeBrowsingOhttpKeyServiceTest&) =
      delete;
  SafeBrowsingOhttpKeyServiceTest& operator=(
      const SafeBrowsingOhttpKeyServiceTest&) = delete;

 private:
  OhttpKeyServiceAllowerForTesting allow_ohttp_key_service_;
  hash_realtime_utils::GoogleChromeBrandingPretenderForTesting apply_branding_;
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingOhttpKeyServiceTest,
                       ServiceRespectsLocationChanges) {
  auto* ohttp_key_service =
      OhttpKeyServiceFactory::GetForProfile(browser()->GetProfile());
  ohttp_key_service->set_ohttp_key_for_testing(
      {kTestOhttpKey, base::Time::Now() + base::Days(10)});

  // By default, the service should be enabled.
  base::MockCallback<OhttpKeyService::Callback> success_callback;
  EXPECT_CALL(success_callback,
              Run(testing::Optional(std::string(kTestOhttpKey))))
      .Times(1);
  ohttp_key_service->GetOhttpKey(success_callback.Get());

  // Changing to CN should disable the service.
  g_browser_process->local_state()->SetString(
      variations::prefs::kVariationsCountry, "cn");
  base::MockCallback<OhttpKeyService::Callback> empty_callback;
  EXPECT_CALL(empty_callback, Run(testing::Eq(std::nullopt))).Times(1);
  ohttp_key_service->GetOhttpKey(empty_callback.Get());

  // Changing to US should re-enable the service.
  g_browser_process->local_state()->SetString(
      variations::prefs::kVariationsCountry, "us");
  base::MockCallback<OhttpKeyService::Callback> success_callback2;
  EXPECT_CALL(success_callback2,
              Run(testing::Optional(std::string(kTestOhttpKey))))
      .Times(1);
  ohttp_key_service->GetOhttpKey(success_callback2.Get());
}

class SafeBrowsingOhttpKeyServiceIncognitoLookupTest
    : public SafeBrowsingOhttpKeyServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  SafeBrowsingOhttpKeyServiceIncognitoLookupTest() {
    feature_list_.InitWithFeatureState(
        kHashPrefixRealTimeLookupsOnIncognitoAndGuest, GetParam());
  }

 protected:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SafeBrowsingOhttpKeyServiceTest::SetUpBrowserContextKeyedServices(context);
    g_browser_process->safe_browsing_service()->SetURLLoaderFactoryForTesting(
        test_url_loader_factory_.GetSafeWeakWrapper());
  }

  void TearDownOnMainThread() override {
    g_browser_process->safe_browsing_service()->SetURLLoaderFactoryForTesting(
        nullptr);
    SafeBrowsingOhttpKeyServiceTest::TearDownOnMainThread();
  }

  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SafeBrowsingOhttpKeyServiceIncognitoLookupTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(
    SafeBrowsingOhttpKeyServiceIncognitoLookupTest,
    IncognitoReusesRegularProfilePrefAndKeepsFetchesInMemory) {
  std::string initial_ohttp_key;
  ASSERT_TRUE(base::Base64Decode(kEncodedTestOhttpKey, &initial_ohttp_key));

  Profile* regular_profile = browser()->GetProfile();
  OhttpKeyService* regular_ohttp_key_service =
      OhttpKeyServiceFactory::GetForProfile(regular_profile);
  ASSERT_NE(nullptr, regular_ohttp_key_service);

  // Populate the regular profile's OHTTP key and prefs end-to-end via a network
  // fetch before opening Incognito.
  test_url_loader_factory_.AddResponse(GetTestKeyFetchUrl().spec(),
                                       initial_ohttp_key);
  base::test::TestFuture<std::optional<std::string>> regular_init_future;
  regular_ohttp_key_service->GetOhttpKey(regular_init_future.GetCallback());
  EXPECT_EQ(regular_init_future.Get(), initial_ohttp_key);
  test_url_loader_factory_.ClearResponses();

  PrefService* regular_prefs = regular_profile->GetPrefs();
  ASSERT_EQ(regular_prefs->GetString(prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kEncodedTestOhttpKey);
  base::Time initial_expiration = regular_prefs->GetTime(
      prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime);
  size_t requests_before_incognito = test_url_loader_factory_.total_requests();

  BrowserWindowInterface* incognito_browser =
      CreateIncognitoBrowser(regular_profile);
  ASSERT_TRUE(incognito_browser);
  Profile* incognito_profile = incognito_browser->GetProfile();
  OhttpKeyService* incognito_ohttp_key_service =
      OhttpKeyServiceFactory::GetForProfile(incognito_profile);
  if (!GetParam()) {
    EXPECT_EQ(nullptr, incognito_ohttp_key_service);
    return;
  }

  ASSERT_NE(nullptr, incognito_ohttp_key_service);
  EXPECT_NE(regular_ohttp_key_service, incognito_ohttp_key_service);

  // Flush the posted MaybeStartOrRescheduleAsyncFetch task from OhttpKeyService
  // construction and verify that opening Incognito populated its key from the
  // regular profile's prefs via OverlayUserPrefStore without issuing a network
  // request.
  base::test::TestFuture<void> flush_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  ASSERT_TRUE(flush_future.Wait());
  EXPECT_EQ(test_url_loader_factory_.total_requests(),
            requests_before_incognito);

  base::test::TestFuture<std::optional<std::string>> initial_key_future;
  incognito_ohttp_key_service->GetOhttpKey(initial_key_future.GetCallback());
  EXPECT_EQ(initial_key_future.Get(), initial_ohttp_key);
  EXPECT_EQ(test_url_loader_factory_.total_requests(),
            requests_before_incognito);

  // Simulate a 422 key-related HTTP error during an Incognito lookup, which
  // clears the Incognito service's key and requires fetching a new key.
  incognito_ohttp_key_service->NotifyLookupResponse(
      initial_ohttp_key, net::HTTP_UNPROCESSABLE_CONTENT);
  EXPECT_FALSE(
      incognito_ohttp_key_service->get_ohttp_key_for_testing().has_value());
  EXPECT_TRUE(
      regular_ohttp_key_service->get_ohttp_key_for_testing().has_value());

  std::string rotated_ohttp_key;
  ASSERT_TRUE(
      base::Base64Decode(kRotatedEncodedTestOhttpKey, &rotated_ohttp_key));
  base::test::TestFuture<std::optional<std::string>> rotated_key_future;
  incognito_ohttp_key_service->GetOhttpKey(rotated_key_future.GetCallback());
  test_url_loader_factory_.AddResponse(GetTestKeyFetchUrl().spec(),
                                       rotated_ohttp_key);
  EXPECT_EQ(rotated_key_future.Get(), rotated_ohttp_key);
  EXPECT_EQ(test_url_loader_factory_.total_requests(),
            requests_before_incognito + 1u);

  // Subsequent Incognito lookups continue using the newly fetched key from
  // memory without issuing another network request.
  base::test::TestFuture<std::optional<std::string>> cached_rotated_key_future;
  incognito_ohttp_key_service->GetOhttpKey(
      cached_rotated_key_future.GetCallback());
  EXPECT_EQ(cached_rotated_key_future.Get(), rotated_ohttp_key);
  EXPECT_EQ(test_url_loader_factory_.total_requests(),
            requests_before_incognito + 1u);

  // The rotated key is stored only in Incognito's in-memory overlay pref store;
  // the regular profile's prefs and service remain untouched.
  EXPECT_EQ(incognito_profile->GetPrefs()->GetString(
                prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kRotatedEncodedTestOhttpKey);
  EXPECT_EQ(regular_prefs->GetString(prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kEncodedTestOhttpKey);
  EXPECT_EQ(regular_prefs->GetTime(
                prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime),
            initial_expiration);
  base::test::TestFuture<std::optional<std::string>> regular_key_future;
  regular_ohttp_key_service->GetOhttpKey(regular_key_future.GetCallback());
  EXPECT_EQ(regular_key_future.Get(), initial_ohttp_key);
}

class SafeBrowsingOhttpKeyServiceGuestLookupTest
    : public SafeBrowsingOhttpKeyServiceIncognitoLookupTest {
 protected:
#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SafeBrowsingOhttpKeyServiceIncognitoLookupTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(
        ash::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
  }
#endif
};

INSTANTIATE_TEST_SUITE_P(All,
                         SafeBrowsingOhttpKeyServiceGuestLookupTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SafeBrowsingOhttpKeyServiceGuestLookupTest,
                       GuestModeUsesIsolatedInMemoryService) {
#if !BUILDFLAG(IS_CHROMEOS)
  std::string regular_ohttp_key;
  ASSERT_TRUE(base::Base64Decode(kEncodedTestOhttpKey, &regular_ohttp_key));
  OhttpKeyService* regular_ohttp_key_service =
      OhttpKeyServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(nullptr, regular_ohttp_key_service);
  test_url_loader_factory_.AddResponse(GetTestKeyFetchUrl().spec(),
                                       regular_ohttp_key);
  base::test::TestFuture<std::optional<std::string>> regular_init_future;
  regular_ohttp_key_service->GetOhttpKey(regular_init_future.GetCallback());
  EXPECT_EQ(regular_init_future.Get(), regular_ohttp_key);
  test_url_loader_factory_.ClearResponses();
  ASSERT_EQ(browser()->GetProfile()->GetPrefs()->GetString(
                prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kEncodedTestOhttpKey);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  BrowserWindowInterface* guest_browser = browser();
#else
  BrowserWindowInterface* guest_browser = CreateGuestBrowser();
#endif
  ASSERT_TRUE(guest_browser);
  ASSERT_TRUE(guest_browser->GetProfile()->IsGuestSession());
  ASSERT_TRUE(guest_browser->GetProfile()->IsOffTheRecord());

  OhttpKeyService* guest_ohttp_key_service =
      OhttpKeyServiceFactory::GetForProfile(guest_browser->GetProfile());
  if (!GetParam()) {
    EXPECT_EQ(nullptr, guest_ohttp_key_service);
    return;
  }

  ASSERT_NE(nullptr, guest_ohttp_key_service);
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_NE(regular_ohttp_key_service, guest_ohttp_key_service);
#endif
  EXPECT_FALSE(
      guest_ohttp_key_service->get_ohttp_key_for_testing().has_value());

  std::string fetched_ohttp_key;
  ASSERT_TRUE(
      base::Base64Decode(kRotatedEncodedTestOhttpKey, &fetched_ohttp_key));

  base::test::TestFuture<std::optional<std::string>> future;
  guest_ohttp_key_service->GetOhttpKey(future.GetCallback());
  test_url_loader_factory_.AddResponse(GetTestKeyFetchUrl().spec(),
                                       fetched_ohttp_key);
  EXPECT_EQ(future.Get(), fetched_ohttp_key);

  // The fetched key is stored in the Guest OTR profile's in-memory prefs, while
  // the Guest parent profile's disk prefs and the Regular profile's prefs
  // remain untouched.
  EXPECT_EQ(guest_browser->GetProfile()->GetPrefs()->GetString(
                prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kRotatedEncodedTestOhttpKey);
  EXPECT_EQ(
      guest_browser->GetProfile()->GetOriginalProfile()->GetPrefs()->GetString(
          prefs::kSafeBrowsingHashRealTimeOhttpKey),
      "");
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(browser()->GetProfile()->GetPrefs()->GetString(
                prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kEncodedTestOhttpKey);

  // Disabling Safe Browsing on the Regular profile does not disable the Guest
  // profile's OHTTP key service.
  SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                       SafeBrowsingState::NO_SAFE_BROWSING);
  base::test::TestFuture<std::optional<std::string>> still_enabled_future;
  guest_ohttp_key_service->GetOhttpKey(still_enabled_future.GetCallback());
  EXPECT_EQ(still_enabled_future.Get(), fetched_ohttp_key);
#endif

  // Disabling Safe Browsing on the Guest profile disables its OHTTP key
  // service.
  SetSafeBrowsingState(guest_browser->GetProfile()->GetPrefs(),
                       SafeBrowsingState::NO_SAFE_BROWSING);
  base::test::TestFuture<std::optional<std::string>> disabled_future;
  guest_ohttp_key_service->GetOhttpKey(disabled_future.GetCallback());
  EXPECT_EQ(disabled_future.Get(), std::nullopt);
}

}  // namespace safe_browsing
