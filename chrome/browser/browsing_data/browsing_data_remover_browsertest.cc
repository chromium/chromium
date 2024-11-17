// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_samples.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browsing_data/browsing_data_remover_browsertest_base.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/media/clear_key_cdm_test_helper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/history/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/nacl/common/buildflags.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "media/mojo/services/webrtc_video_perf_history.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#if BUILDFLAG(IS_MAC)
#include "base/threading/platform_thread.h"
#endif
#include "base/memory/scoped_refptr.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#endif

using content::BrowserThread;
using content::BrowsingDataFilterBuilder;

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

namespace {
static const char* kExampleHost = "example.com";
static const char* kLocalHost = "localhost";
static const base::Time kStartTime = base::Time::Now();
static const base::Time kLastHourTime = kStartTime - base::Hours(1);

// This enum is used in the place of base::Time because using base::Time
// as a test param causes problems on Fuchsia. See https://crbug.com/1308948 for
// details.
enum TimeEnum {
  kDefault,
  kStart,
  kLastHour,
  kMax,
};

base::Time TimeEnumToTime(TimeEnum time) {
  switch (time) {
    case TimeEnum::kStart:
      return kStartTime;
    case TimeEnum::kLastHour:
      return kLastHourTime;
    case TimeEnum::kMax:
      return base::Time::Max();
    default:
      return base::Time();
  }
}

std::vector<std::string> GetHistogramSuffixes(
    const base::HistogramTester& tester,
    const std::string& prefix) {
  std::vector<std::string> types;
  for (const auto& entry : tester.GetTotalCountsForPrefix(prefix)) {
    types.push_back(entry.first.substr(prefix.length()));
  }
  return types;
}

void AppendRange(std::vector<std::string>& target,
                 const std::vector<std::string_view>& append) {
  // Use std append_range() when c++23 is available.
  target.insert(target.end(), append.begin(), append.end());
}

}  // namespace

class BrowsingDataRemoverBrowserTest
    : public BrowsingDataRemoverBrowserTestBase {
 public:
  BrowsingDataRemoverBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    enabled_features.push_back(media::kExternalClearKeyForTesting);
#endif
    // TODO(crbug.com/333756088): WebSQL is disabled everywhere by default as of
    // M119 (crbug/695592) except on Android WebView. This is enabled for
    // Android only to indirectly cover WebSQL deletions on WebView.
    enabled_features.push_back(blink::features::kWebSQLAccess);
    InitFeatureLists(std::move(enabled_features), std::move(disabled_features));
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    crosapi::mojom::BrowserInitParamsPtr init_params =
        crosapi::mojom::BrowserInitParams::New();
    std::string device_account_email = "primaryaccount@gmail.com";
    account_manager::AccountKey key(
        signin::GetTestGaiaIdForEmail(device_account_email),
        ::account_manager::AccountType::kGaia);
    init_params->device_account =
        account_manager::ToMojoAccount({key, device_account_email});
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }
#endif

  void SetUpOnMainThread() override {
    BrowsingDataRemoverBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule(kExampleHost, "127.0.0.1");

    // Explicitly disable session restore. Otherwise tests that restart the
    // browser can get tab data persisted across sessions when we thought we
    // deleted it.
    SessionStartupPref::SetStartupPref(
        GetProfile()->GetPrefs(),
        SessionStartupPref(SessionStartupPref::DEFAULT));
  }
  void RemoveAndWait(uint64_t remove_mask) {
    RemoveAndWait(remove_mask, TimeEnum::kDefault, TimeEnum::kMax);
  }

  void RemoveAndWait(uint64_t remove_mask, TimeEnum delete_begin) {
    RemoveAndWait(remove_mask, delete_begin, TimeEnum::kMax);
  }

  void RemoveAndWait(uint64_t remove_mask,
                     TimeEnum delete_begin,
                     TimeEnum delete_end) {
    content::BrowsingDataRemover* remover =
        GetBrowser()->profile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveAndReply(
        TimeEnumToTime(delete_begin), TimeEnumToTime(delete_end), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  void RemoveWithFilterAndWait(
      uint64_t remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
    content::BrowsingDataRemover* remover =
        GetBrowser()->profile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter_builder), &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  // Test a data type by creating a value and checking it is counted by the
  // cookie counter. Then it deletes the value and checks that it has been
  // deleted and the cookie counter is back to zero.
  void TestSiteData(const std::string& type, TimeEnum delete_begin) {
    EXPECT_EQ(0, GetSiteDataCount());
    GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));

    EXPECT_EQ(0, GetSiteDataCount());
    ExpectTotalModelCount(0);
    EXPECT_FALSE(HasDataForType(type));

    SetDataForType(type);
    EXPECT_EQ(1, GetSiteDataCount());
    // TODO(crbug.com/40218898): Use a different approach to determine presence
    // of data that does not depend on UI code and has a better resolution when
    // 3PSP is fully enabled. ExpectTotalModelCount(1) is not always true
    // here.
    EXPECT_TRUE(HasDataForType(type));

    RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
                  delete_begin);
    EXPECT_EQ(0, GetSiteDataCount());
    ExpectTotalModelCount(0);
    EXPECT_FALSE(HasDataForType(type));
  }

  // Test that storage systems like filesystem and websql, where just an access
  // creates an empty store, are counted and deleted correctly.
  void TestEmptySiteData(const std::string& type, TimeEnum delete_begin) {
    EXPECT_EQ(0, GetSiteDataCount());
    ExpectTotalModelCount(0);
    GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));
    EXPECT_EQ(0, GetSiteDataCount());
    ExpectTotalModelCount(0);
    // Opening a store of this type creates a site data entry.
    EXPECT_FALSE(HasDataForType(type));
    EXPECT_EQ(1, GetSiteDataCount());
    // TODO(crbug.com/40218898): Use a different approach to determine presence
    // of data that does not depend on UI code and has a better resolution when
    // 3PSP is fully enabled. ExpectTotalModelCount(1) is not always true
    // here.
    RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
                  delete_begin);

    EXPECT_EQ(0, GetSiteDataCount());
    ExpectTotalModelCount(0);
  }

  inline void ExpectTotalModelCount(size_t expected) {
    std::unique_ptr<BrowsingDataModel> browsing_data_model =
        GetBrowsingDataModel(GetProfile());

    EXPECT_EQ(expected, browsing_data_model->size());
  }

  void OnVideoDecodePerfInfo(base::RunLoop* run_loop,
                             bool* out_is_smooth,
                             bool* out_is_power_efficient,
                             bool is_smooth,
                             bool is_power_efficient) {
    *out_is_smooth = is_smooth;
    *out_is_power_efficient = is_power_efficient;
    run_loop->QuitWhenIdle();
  }

  network::mojom::NetworkContext* network_context() const {
    return GetBrowser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext();
  }

  void ClearSiteDataAndWait(
      const url::Origin& origin,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key,
      const std::optional<blink::StorageKey>& storage_key,
      const std::set<std::string>& storage_buckets_to_remove) {
    bool partitioned_state_allowed_only =
        cookie_partition_key.has_value() &&
        !origin.DomainIs(cookie_partition_key->site()
                             .registrable_domain_or_host_for_testing());
    base::RunLoop loop;
    content::ClearSiteData(
        GetBrowser()->profile()->GetWeakPtr(),
        /*storage_partition_config=*/std::nullopt,
        /*origin=*/origin, content::ClearSiteDataTypeSet::All(),
        /*storage_buckets_to_remove=*/storage_buckets_to_remove,
        /*avoid_closing_connections=*/true,
        /*cookie_partition_key=*/cookie_partition_key,
        /*storage_key=*/storage_key,
        /*partitioned_state_allowed_only=*/partitioned_state_allowed_only,
        /*callback=*/loop.QuitClosure());
    loop.Run();
  }

 private:
  void OnCacheSizeResult(
      base::RunLoop* run_loop,
      browsing_data::BrowsingDataCounter::ResultInt* out_size,
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    if (!result->Finished())
      return;

    *out_size =
        static_cast<browsing_data::BrowsingDataCounter::FinishedResult*>(
            result.get())
            ->Value();
    run_loop->Quit();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    // Testing MediaLicenses requires additional command line parameters as
    // it uses the External Clear Key CDM.
    RegisterClearKeyCdm(command_line);
#endif
  }
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Same as BrowsingDataRemoverBrowserTest, but forces Dice to be enabled.
class DiceBrowsingDataRemoverBrowserTest
    : public BrowsingDataRemoverBrowserTest {
 public:
  AccountInfo AddAccountToProfile(const std::string& account_id,
                                  Profile* profile,
                                  bool is_primary) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    if (is_primary) {
      DCHECK(!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
      return signin::MakePrimaryAccountAvailable(identity_manager,
                                                 account_id + "@gmail.com",
                                                 signin::ConsentLevel::kSync);
    }
    auto account_info =
        signin::MakeAccountAvailable(identity_manager, account_id);
    DCHECK(
        identity_manager->HasAccountWithRefreshToken(account_info.account_id));
    return account_info;
  }
};
#endif

// Test BrowsingDataRemover for downloads.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Download) {
  DownloadAnItem();
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  VerifyDownloadCount(0u);
}

// Test that the salt for media device IDs is reset when cookies are cleared.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, MediaDeviceIdSalt) {
  content::RenderFrameHost* frame_host = GetBrowser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  url::Origin origin = frame_host->GetLastCommittedOrigin();
  net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromOrigin(origin);
  blink::StorageKey storage_key = blink::StorageKey::CreateFirstParty(origin);

  base::test::TestFuture<bool, const std::string&> future;
  content::GetContentClientForTesting()->browser()->GetMediaDeviceIDSalt(
      frame_host, site_for_cookies, storage_key, future.GetCallback());
  std::string original_salt = future.Get<1>();

  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);

  future.Clear();
  content::GetContentClientForTesting()->browser()->GetMediaDeviceIDSalt(
      frame_host, site_for_cookies, storage_key, future.GetCallback());
  std::string new_salt = future.Get<1>();

  EXPECT_NE(original_salt, new_salt);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Test that Sync is paused when cookies are cleared.
IN_PROC_BROWSER_TEST_F(DiceBrowsingDataRemoverBrowserTest, SyncToken) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account and a secondary account.
  const char kPrimaryAccountId[] = "primary_account_id";
  AccountInfo primary_account =
      AddAccountToProfile(kPrimaryAccountId, profile, /*is_primary=*/true);
  const char kSecondaryAccountId[] = "secondary_account_id";
  AccountInfo secondary_account =
      AddAccountToProfile(kSecondaryAccountId, profile, /*is_primary=*/false);
  // Clear cookies.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  // Check that the Sync account was not removed and Sync was paused.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(primary_account.account_id));
  EXPECT_EQ(
      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_REJECTED_BY_CLIENT,
      identity_manager
          ->GetErrorStateOfRefreshTokenForAccount(primary_account.account_id)
          .GetInvalidGaiaCredentialsReason());
  // Check that the secondary token was revoked.
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      secondary_account.account_id));
}

// Test that Sync is not paused when cookies are cleared, if synced data is
// being deleted.
IN_PROC_BROWSER_TEST_F(DiceBrowsingDataRemoverBrowserTest,
                       SyncTokenScopedDeletion) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account and a secondary account.
  const char kPrimaryAccountId[] = "primary_account_id";
  AccountInfo primary_account =
      AddAccountToProfile(kPrimaryAccountId, profile, /*is_primary=*/true);
  const char kSecondaryAccountId[] = "secondary_account_id";
  AccountInfo secondary_account =
      AddAccountToProfile(kSecondaryAccountId, profile, /*is_primary=*/false);
  // Sync data is being deleted.
  std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion =
      AccountReconcilorFactory::GetForProfile(profile)
          ->GetScopedSyncDataDeletion();
  // Clear cookies.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  // Check that the Sync token was not revoked.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(primary_account.account_id));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account.account_id));
  // Check that the secondary token was revoked.
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      secondary_account.account_id));
}

// Test that Sync is paused when cookies are cleared if Sync was in error, even
// if synced data is being deleted.
IN_PROC_BROWSER_TEST_F(DiceBrowsingDataRemoverBrowserTest, SyncTokenError) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account with authentication error.
  const char kAccountId[] = "account_id";
  AccountInfo primary_account =
      AddAccountToProfile(kAccountId, profile, /*is_primary=*/true);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, primary_account.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // Sync data is being deleted.
  std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion =
      AccountReconcilorFactory::GetForProfile(profile)
          ->GetScopedSyncDataDeletion();
  // Clear cookies.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  // Check that the account was not removed and Sync was paused.
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(primary_account.account_id));
  EXPECT_EQ(
      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_REJECTED_BY_CLIENT,
      identity_manager
          ->GetErrorStateOfRefreshTokenForAccount(primary_account.account_id)
          .GetInvalidGaiaCredentialsReason());
}

// Test that the tokens are revoked when cookies are cleared when there is no
// primary account.
IN_PROC_BROWSER_TEST_F(DiceBrowsingDataRemoverBrowserTest, NoSync) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a non-Sync account.
  const char kAccountId[] = "account_id";
  AccountInfo secondary_account =
      AddAccountToProfile(kAccountId, profile, /*is_primary=*/false);
  // Clear cookies.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  // Check that the account was removed.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      secondary_account.account_id));
}
#endif

// The call to Remove() should crash in debug (DCHECK), but the browser-test
// process model prevents using a death test.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// Test BrowsingDataRemover for prohibited downloads. Note that this only
// really exercises the code in a Release build.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, DownloadProhibited) {
  PrefService* prefs = GetBrowser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  DownloadAnItem();
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  VerifyDownloadCount(1u);
}
#endif

// Verify VideoDecodePerfHistory is cleared when deleting all history from
// beginning of time.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, VideoDecodePerfHistory) {
  media::VideoDecodePerfHistory* video_decode_perf_history =
      GetBrowser()->profile()->GetVideoDecodePerfHistory();

  // Save a video decode record. Note: we avoid using a web page to generate the
  // stats as this takes at least 5 seconds and even then is not a guarantee
  // depending on scheduler. Manual injection is quick and non-flaky.
  const media::VideoCodecProfile kProfile = media::VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;
  const int kFramesDecoded = 1000;
  const int kFramesDropped = .9 * kFramesDecoded;
  const int kFramesPowerEfficient = 0;
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));
  const bool kIsTopFrame = true;
  const uint64_t kPlayerId = 1234u;

  media::mojom::PredictionFeatures prediction_features;
  prediction_features.profile = kProfile;
  prediction_features.video_size = kSize;
  prediction_features.frames_per_sec = kFrameRate;

  media::mojom::PredictionTargets prediction_targets;
  prediction_targets.frames_decoded = kFramesDecoded;
  prediction_targets.frames_dropped = kFramesDropped;
  prediction_targets.frames_power_efficient = kFramesPowerEfficient;

  {
    base::RunLoop run_loop;
    video_decode_perf_history->GetSaveCallback().Run(
        ukm::kInvalidSourceId, media::learning::FeatureValue(0), kIsTopFrame,
        prediction_features, prediction_targets, kPlayerId,
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  // Verify history exists.
  // Expect |is_smooth| = false and |is_power_efficient| = false given that 90%
  // of recorded frames were dropped and 0 were power efficient.
  bool is_smooth = true;
  bool is_power_efficient = true;
  {
    base::RunLoop run_loop;
    video_decode_perf_history->GetPerfInfo(
        media::mojom::PredictionFeatures::New(prediction_features),
        base::BindOnce(&BrowsingDataRemoverBrowserTest::OnVideoDecodePerfInfo,
                       base::Unretained(this), &run_loop, &is_smooth,
                       &is_power_efficient));
    run_loop.Run();
  }
  EXPECT_FALSE(is_smooth);
  EXPECT_FALSE(is_power_efficient);

  // Clear history.
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_HISTORY);

  // Verify history no longer exists. Both |is_smooth| and |is_power_efficient|
  // should now report true because the VideoDecodePerfHistory optimistically
  // returns true when it has no data.
  {
    base::RunLoop run_loop;
    video_decode_perf_history->GetPerfInfo(
        media::mojom::PredictionFeatures::New(prediction_features),
        base::BindOnce(&BrowsingDataRemoverBrowserTest::OnVideoDecodePerfInfo,
                       base::Unretained(this), &run_loop, &is_smooth,
                       &is_power_efficient));
    run_loop.Run();
  }
  EXPECT_TRUE(is_smooth);
  EXPECT_TRUE(is_power_efficient);
}

// Verify WebrtcVideoPerfHistory is cleared when deleting all history from
// beginning of time.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, WebrtcVideoPerfHistory) {
  media::WebrtcVideoPerfHistory* webrtc_video_perf_history =
      GetBrowser()->profile()->GetWebrtcVideoPerfHistory();

  // Save a video decode record. Note: we avoid using a web page to generate the
  // stats as this takes at least 5 seconds and even then is not a guarantee
  // depending on scheduler. Manual injection is quick and non-flaky.
  const media::VideoCodecProfile kProfile = media::VP9PROFILE_PROFILE0;
  const int kVideoPixels = 1920 * 1080;
  const int kFrameRate = 30;

  const int kFramesProcessed = 1000;
  const int kKeyFramesProcessed = 11;
  const float kP99ProcessingTimeMs = 100.0;

  media::mojom::WebrtcPredictionFeatures features;
  features.is_decode_stats = true;
  features.profile = kProfile;
  features.video_pixels = kVideoPixels;
  features.hardware_accelerated = false;

  media::mojom::WebrtcVideoStats video_stats;
  video_stats.frames_processed = kFramesProcessed;
  video_stats.key_frames_processed = kKeyFramesProcessed;
  video_stats.p99_processing_time_ms = kP99ProcessingTimeMs;

  {
    base::RunLoop run_loop;
    webrtc_video_perf_history->GetSaveCallback().Run(features, video_stats,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify history exists.
  // Expect |is_smooth| = false given that the 99th percentile processing time
  // is 100 ms.
  {
    base::RunLoop run_loop;
    webrtc_video_perf_history->GetPerfInfo(
        media::mojom::WebrtcPredictionFeatures::New(features), kFrameRate,
        base::BindLambdaForTesting([&](bool smooth) {
          EXPECT_FALSE(smooth);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  auto filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddRegisterableDomain("example.com");
  RemoveWithFilterAndWait(chrome_browsing_data_remover::FILTERABLE_DATA_TYPES,
                          std::move(filter_builder));

  // This data type doesn't implement per-origin deletion so just test that
  // nothing got removed.
  {
    base::RunLoop run_loop;
    webrtc_video_perf_history->GetPerfInfo(
        media::mojom::WebrtcPredictionFeatures::New(features), kFrameRate,
        base::BindLambdaForTesting([&](bool smooth) {
          EXPECT_FALSE(smooth);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Clear history.
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_HISTORY);

  // Verify history no longer exists. |is_smooth| should now report true because
  // the WebrtcVideoPerfHistory optimistically returns true when it has no data.
  {
    base::RunLoop run_loop;
    webrtc_video_perf_history->GetPerfInfo(
        media::mojom::WebrtcPredictionFeatures::New(features), kFrameRate,
        base::BindLambdaForTesting([&](bool smooth) {
          EXPECT_TRUE(smooth);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

// TODO(crbug.com/333756088): WebSQL is disabled everywhere except Android
// WebView.
#if !BUILDFLAG(IS_ANDROID)
#define MAYBE_Database DISABLED_Database
#else
#define MAYBE_Database Database
#endif
// Verify can modify database after deleting it.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, MAYBE_Database) {
  GURL url = embedded_test_server()->GetURL("/simple_database.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));

  RunScriptAndCheckResult("createTable()", "done");
  RunScriptAndCheckResult("insertRecord('text')", "done");
  RunScriptAndCheckResult("getRecords()", "text");

  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));
  RunScriptAndCheckResult("createTable()", "done");
  RunScriptAndCheckResult("insertRecord('text2')", "done");
  RunScriptAndCheckResult("getRecords()", "text2");
}

// Verifies that cache deletion finishes successfully. Completes deletion of
// cache should leave it empty, and partial deletion should leave nonzero
// amount of data. Note that this tests the integration of BrowsingDataRemover
// with ConditionalCacheDeletionHelper. Whether ConditionalCacheDeletionHelper
// actually deletes the correct entries is tested
// in ConditionalCacheDeletionHelperBrowsertest.
// TODO(crbug.com/41373874): check the cache size instead of stopping the server
// and loading the request again.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Cache) {
  // Load several resources.
  GURL url1 = embedded_test_server()->GetURL("/cachetime");
  GURL url2 = embedded_test_server()->GetURL(kExampleHost, "/cachetime");
  ASSERT_FALSE(url::IsSameOriginWith(url1, url2));

  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Check that the cache has been populated by revisiting these pages with the
  // server stopped.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Partially delete cache data. Delete data for localhost, which is the origin
  // of |url1|, but not for |kExampleHost|, which is the origin of |url2|.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(url::Origin::Create(url1));
  RemoveWithFilterAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE,
                          std::move(filter_builder));

  // After the partial deletion, the cache should be smaller but still nonempty.
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Another partial deletion with the same filter should have no effect.
  filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(url::Origin::Create(url1));
  RemoveWithFilterAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE,
                          std::move(filter_builder));
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Delete the remaining data.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE);

  // The cache should be empty.
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url2));
}

// Crashes the network service while clearing the HTTP cache to make sure the
// clear operation does complete.
// Note that there is a race between crashing the network service and clearing
// the cache, so the test might flakily fail if the tested behavior does not
// work.
// TODO(crbug.com/40563720): test retry behavior by validating the cache is
// empty after the crash.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       ClearCacheAndNetworkServiceCrashes) {
  if (!content::IsOutOfProcessNetworkService())
    return;

  // Clear the cached data with a task posted to crash the network service.
  // The task should be run while waiting for the cache clearing operation to
  // complete, hopefully it happens before the cache has been cleared.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&content::BrowserTestBase::SimulateNetworkServiceCrash,
                     base::Unretained(this)));

  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE);
}

// Verifies that the network quality prefs are cleared.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, VerifyNQECacheCleared) {
  base::HistogramTester histogram_tester;
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE);

  // Wait until there is at least one sample in NQE.PrefsSizeOnClearing.
  bool histogram_populated = false;
  for (size_t attempt = 0; attempt < 3; ++attempt) {
    const std::vector<base::Bucket> buckets =
        histogram_tester.GetAllSamples("NQE.PrefsSizeOnClearing");
    for (const auto& bucket : buckets) {
      if (bucket.count > 0) {
        histogram_populated = true;
        break;
      }
    }
    if (histogram_populated)
      break;

    // Retry fetching the histogram since it's not populated yet.
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }

  histogram_tester.ExpectTotalCount("NQE.PrefsSizeOnClearing", 1);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       ExternalProtocolHandlerPerOriginPrefs) {
  Profile* profile = GetBrowser()->profile();
  url::Origin test_origin = url::Origin::Create(GURL("https://example.test/"));
  const std::string serialized_test_origin = test_origin.Serialize();
  base::Value::Dict allowed_protocols_for_origin;
  allowed_protocols_for_origin.Set("tel", true);
  base::Value::Dict origin_pref;
  origin_pref.Set(serialized_test_origin,
                  std::move(allowed_protocols_for_origin));
  profile->GetPrefs()->SetDict(prefs::kProtocolHandlerPerOriginAllowedProtocols,
                               std::move(origin_pref));
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("tel", &test_origin, profile);
  ASSERT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA);
  block_state =
      ExternalProtocolHandler::GetBlockState("tel", &test_origin, profile);
  ASSERT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, HistoryDeletion) {
  const std::string kType = "History";
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  // Create a new tab to avoid confusion from having a NTP navigation entry.
  ui_test_utils::NavigateToURLWithDisposition(
      GetBrowser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_FALSE(HasDataForType(kType));
  SetDataForType(kType);
  EXPECT_TRUE(HasDataForType(kType));
  // Remove history from navigation to site_data.html.
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_HISTORY);
  EXPECT_FALSE(HasDataForType(kType));
  SetDataForType(kType);
  EXPECT_TRUE(HasDataForType(kType));
  // Remove history from previous pushState() call in setHistory().
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_HISTORY);
  EXPECT_FALSE(HasDataForType(kType));
}

// ChromeOS users cannot sign out, their account preferences can never be
// cleared.
#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       ClearingCookiesAlsoClearsPasswordAccountStorageOptIn) {
  const char kTestEmail[] = "foo@gmail.com";
  PrefService* prefs = GetProfile()->GetPrefs();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                      signin::ConsentLevel::kSignin);

  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    password_manager::features_util::OptOutOfAccountStorage(prefs,
                                                            sync_service);
  } else {
    password_manager::features_util::OptInToAccountStorage(prefs, sync_service);
  }
  ASSERT_EQ(password_manager::features_util::IsOptedInForAccountStorage(
                prefs, sync_service),
            !switches::IsExplicitBrowserSigninUIOnDesktopEnabled());

  signin::ClearPrimaryAccount(identity_manager);
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA);
  signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                      signin::ConsentLevel::kSignin);

  EXPECT_EQ(password_manager::features_util::IsOptedInForAccountStorage(
                prefs, sync_service),
            switches::IsExplicitBrowserSigninUIOnDesktopEnabled());
}

IN_PROC_BROWSER_TEST_F(
    BrowsingDataRemoverBrowserTest,
    ClearingCookiesWithFilterAlsoClearsPasswordAccountStorageOptIn) {
  const char kTestEmail[] = "foo@gmail.com";
  PrefService* prefs = GetProfile()->GetPrefs();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                      signin::ConsentLevel::kSignin);

  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    password_manager::features_util::OptOutOfAccountStorage(prefs,
                                                            sync_service);
  } else {
    password_manager::features_util::OptInToAccountStorage(prefs, sync_service);
  }
  ASSERT_EQ(password_manager::features_util::IsOptedInForAccountStorage(
                prefs, sync_service),
            !switches::IsExplicitBrowserSigninUIOnDesktopEnabled());

  // Clearing cookies for some random domain should have no effect on the
  // opt-in.
  signin::ClearPrimaryAccount(identity_manager);
  {
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
        BrowsingDataFilterBuilder::Create(
            BrowsingDataFilterBuilder::Mode::kDelete);
    filter_builder->AddRegisterableDomain("example.com");
    RemoveWithFilterAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
                            std::move(filter_builder));
  }
  signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                      signin::ConsentLevel::kSignin);
  EXPECT_EQ(password_manager::features_util::IsOptedInForAccountStorage(
                prefs, sync_service),
            !switches::IsExplicitBrowserSigninUIOnDesktopEnabled());

  // Clearing cookies for google.com should clear the opt-in.
  signin::ClearPrimaryAccount(identity_manager);
  {
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
        BrowsingDataFilterBuilder::Create(
            BrowsingDataFilterBuilder::Mode::kDelete);
    filter_builder->AddRegisterableDomain("google.com");
    RemoveWithFilterAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
                            std::move(filter_builder));
  }
  signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                      signin::ConsentLevel::kSignin);
  EXPECT_EQ(password_manager::features_util::IsOptedInForAccountStorage(
                prefs, sync_service),
            switches::IsExplicitBrowserSigninUIOnDesktopEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, ClearSiteData) {
  const char kTestEmail[] = "foo@gmail.com";
  PrefService* prefs = GetProfile()->GetPrefs();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                      signin::ConsentLevel::kSignin);

  const GURL kFirstPartyURL("https://google.com");
  const GURL kCrossSiteURL("https://example.com");

  const struct {
    const url::Origin origin;
    const std::optional<net::CookiePartitionKey> cookie_partition_key;
    const std::optional<blink::StorageKey> storage_key;
    bool expects_keep_optin_pref;
  } test_cases[] = {
      {
          url::Origin::Create(kFirstPartyURL),
          std::nullopt,
          std::nullopt,
          false,
      },
      {
          url::Origin::Create(kCrossSiteURL),
          std::nullopt,
          std::nullopt,
          true,
      },
      {
          url::Origin::Create(kFirstPartyURL),
          net::CookiePartitionKey::FromURLForTesting(kFirstPartyURL),
          std::nullopt,
          false,
      },
      {
          url::Origin::Create(kFirstPartyURL),
          net::CookiePartitionKey::FromURLForTesting(kFirstPartyURL),
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(kFirstPartyURL)),
          false,
      },
      {
          url::Origin::Create(kFirstPartyURL),
          net::CookiePartitionKey::FromURLForTesting(kCrossSiteURL),
          std::nullopt,
          true,
      },
      {
          url::Origin::Create(kFirstPartyURL),
          net::CookiePartitionKey::FromURLForTesting(kCrossSiteURL),
          blink::StorageKey::Create(
              url::Origin::Create(kCrossSiteURL),
              net::SchemefulSite(url::Origin::Create(kFirstPartyURL)),
              blink::mojom::AncestorChainBit::kCrossSite),
          true,
      },
  };
  for (size_t i = 0; i < std::size(test_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test case %zu", i));
    const auto& test_case = test_cases[i];

    if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
      password_manager::features_util::OptOutOfAccountStorage(prefs,
                                                              sync_service);
    } else {
      password_manager::features_util::OptInToAccountStorage(prefs,
                                                             sync_service);
    }
    ASSERT_EQ(password_manager::features_util::IsOptedInForAccountStorage(
                  prefs, sync_service),
              !switches::IsExplicitBrowserSigninUIOnDesktopEnabled());
    signin::ClearPrimaryAccount(identity_manager);
    ClearSiteDataAndWait(test_case.origin, test_case.cookie_partition_key,
                         test_case.storage_key, {});
    signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                        signin::ConsentLevel::kSignin);

    if (test_case.expects_keep_optin_pref) {
      EXPECT_EQ(password_manager::features_util::IsOptedInForAccountStorage(
                    prefs, sync_service),
                !switches::IsExplicitBrowserSigninUIOnDesktopEnabled());
    } else {
      EXPECT_EQ(password_manager::features_util::IsOptedInForAccountStorage(
                    prefs, sync_service),
                switches::IsExplicitBrowserSigninUIOnDesktopEnabled());
    }
  }
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

// Storage Buckets

class BrowsingDataRemoverStorageBucketsBrowserTest
    : public BrowsingDataRemoverBrowserTest {
 public:
  BrowsingDataRemoverStorageBucketsBrowserTest() {
    features_.InitWithFeatures({blink::features::kStorageBuckets,
                                net::features::kThirdPartyStoragePartitioning},
                               {});
  }

  void ClearSiteDataAndWait(
      const url::Origin& origin,
      const std::optional<blink::StorageKey>& storage_key,
      const std::set<std::string>& storage_buckets_to_remove) {
    base::RunLoop loop;
    content::ClearSiteDataTypeSet clear_site_data_types =
        content::ClearSiteDataTypeSet::All();
    // We're clearing some storage buckets and not all of them.
    clear_site_data_types.Remove(content::ClearSiteDataType::kStorage);
    content::ClearSiteData(
        GetBrowser()->profile()->GetWeakPtr(),
        /*storage_partition_config=*/std::nullopt,
        /*origin=*/origin, clear_site_data_types,
        /*storage_buckets_to_remove=*/storage_buckets_to_remove,
        /*avoid_closing_connections=*/true,
        /*cookie_partition_key=*/std::nullopt,
        /*storage_key=*/storage_key,
        /*partitioned_state_allowed_only=*/false,
        /*callback=*/loop.QuitClosure());
    loop.Run();
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverStorageBucketsBrowserTest,
                       ClearSiteDataStorageBuckets) {
  GURL url("https://example.com");
  url::Origin origin = url::Origin::Create(url);
  const auto storage_key = blink::StorageKey::CreateFirstParty(origin);

  storage::QuotaManager* quota_manager =
      GetBrowser()->profile()->GetDefaultStoragePartition()->GetQuotaManager();

  auto* quota_manager_proxy = quota_manager->proxy();

  quota_manager_proxy->CreateBucketForTesting(
      storage_key, "drafts", blink::mojom::StorageType::kTemporary,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](storage::QuotaErrorOr<storage::BucketInfo> error_or_bucket_info) {
          }));
  quota_manager_proxy->CreateBucketForTesting(
      storage_key, "inbox", blink::mojom::StorageType::kTemporary,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](storage::QuotaErrorOr<storage::BucketInfo> error_or_bucket_info) {
          }));
  quota_manager_proxy->CreateBucketForTesting(
      storage_key, "attachments", blink::mojom::StorageType::kTemporary,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](storage::QuotaErrorOr<storage::BucketInfo> error_or_bucket_info) {
          }));

  ClearSiteDataAndWait(origin, storage_key, {"drafts", "attachments"});

  quota_manager_proxy->GetBucketsForStorageKey(
      storage_key, blink::mojom::StorageType::kTemporary,
      /*delete_expired*/ false, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce([](storage::QuotaErrorOr<std::set<storage::BucketInfo>>
                            error_or_buckets) {
        EXPECT_EQ(1u, error_or_buckets.value().size());
      }));
}

const char kDelegateHistogramPrefix[] =
    "History.ClearBrowsingData.Duration.ChromeTask.";
const char kImplHistogramPrefix[] = "History.ClearBrowsingData.Duration.Task.";

// Add data types here that support filtering and only delete data that matches
// the BrowsingDataFilterBuilder.
const std::vector<std::string_view> kSupportsOriginFilteringImpl{
    "AuthCache",           "EmbedderData",     "HttpCache",
    "NetworkErrorLogging", "PreflightCache",   "ReportingCache",
    "SharedDictionary",    "StoragePartition", "Synchronous",
    "TrustTokens",
};
const std::vector<std::string_view> kSupportsOriginFilteringDelegate{
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
    "CdmLicenses",
#endif
    "Cookies",           "DisableAutoSigninForProfilePasswords",
    "DomainReliability", "MediaDeviceSalts",
    "Synchronous",
};

// This test ensures that all deletions that are part of FILTERABLE_DATA_TYPES
// fully support the BrowsingDataFilterBuilder if they are running for
// origin-specific deletions. Ideally, every web-visible data type should
// support filtering. If you implemented filtering already, just add your type
// to kSupportsOriginFiltering above.
//
// If it is not important that your data is cleared with per-origin deletions,
// you can add your type to kDoesNotSupportOriginFiltering and ensure that
// deletions are only performed when the filter builder
// MatchesMostOriginsAndDomains().
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, FullyFilteredDataTypes) {
  base::HistogramTester tester;
  auto filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddRegisterableDomain("example.com");
  RemoveWithFilterAndWait(chrome_browsing_data_remover::FILTERABLE_DATA_TYPES,
                          std::move(filter_builder));

  EXPECT_THAT(GetHistogramSuffixes(tester, kImplHistogramPrefix),
              UnorderedElementsAreArray(kSupportsOriginFilteringImpl));
  EXPECT_THAT(GetHistogramSuffixes(tester, kDelegateHistogramPrefix),
              UnorderedElementsAreArray(kSupportsOriginFilteringDelegate));
}

// Add data types here that do not support the BrowsingDataFilterBuilder.
// These deletions should only run when the mode of the filter builder is
// "kPreserve" and MatchesMostOriginsAndDomains() is true. Otherwise data for
// these types will be cleared when per-origin deletions like those from the
// Clear-Site-Data header are performed.
const std::vector<std::string_view> kDoesNotSupportOriginFilteringImpl{
    "CodeCaches",
    "NetworkHistory",
};
const std::vector<std::string_view> kDoesNotSupportOriginFilteringDelegate{
    "FaviconCacheExpiration",
#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
    "UserDataSnapshot",
#endif
    "WebrtcEventLogs",
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "TpmAttestationKeys",
#endif
#if BUILDFLAG(ENABLE_NACL)
    "NaclCache",
    "PnaclCache",
#endif
};

// See comment on FullyFilteredDataTypes test for advice when this test fails.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, AllFilterableDataTypes) {
  base::HistogramTester tester;
  auto filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kPreserve);
  filter_builder->AddRegisterableDomain("example.com");
  RemoveWithFilterAndWait(chrome_browsing_data_remover::FILTERABLE_DATA_TYPES,
                          std::move(filter_builder));

  std::vector<std::string> all_impl_types;
  AppendRange(all_impl_types, kSupportsOriginFilteringImpl);
  AppendRange(all_impl_types, kDoesNotSupportOriginFilteringImpl);
  EXPECT_THAT(GetHistogramSuffixes(tester, kImplHistogramPrefix),
              UnorderedElementsAreArray(all_impl_types));

  std::vector<std::string> all_delegate_types;
  AppendRange(all_delegate_types, kSupportsOriginFilteringDelegate);
  AppendRange(all_delegate_types, kDoesNotSupportOriginFilteringDelegate);
  EXPECT_THAT(GetHistogramSuffixes(tester, kDelegateHistogramPrefix),
              UnorderedElementsAreArray(all_delegate_types));
}

// Parameterized to run tests for different deletion time ranges.
class BrowsingDataRemoverBrowserTestP
    : public BrowsingDataRemoverBrowserTest,
      public testing::WithParamInterface<TimeEnum> {};

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, CookieDeletion) {
  TestSiteData("Cookie", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       CookieIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("Cookie", GetParam());
}

// Regression test for https://crbug.com/1216406.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       BrowserContextDestructionVsCookieRemoval) {
  // Open an incognito browser.
  UseIncognitoBrowser();

  // Set a cookie.
  const char kDataType[] = "Cookie";
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));
  SetDataForType(kDataType);
  EXPECT_EQ(1, GetSiteDataCount());
  ExpectTotalModelCount(1);
  EXPECT_TRUE(HasDataForType(kDataType));

  // Start data removal.  This will CreateTaskCompletionClosureForMojo and
  // register it as a completion callback for mojo calls to NetworkContext
  // and other StorageParition-owned mojo::Remote(s).
  //
  // kRemoveMask contains:
  // - DATA_TYPE_SITE_DATA - cargo-culted default from other tests
  // - DEFERRED_COOKIE_DELETION_DATA_TYPES - to get non-empty result from
  //   ChromeBrowsingDataRemoverDelegate::GetDomainsForDeferredCookieDeletion
  //   (which is needed to touch StoragePartition in
  //   BrowsingDataRemoverImpl::OnTaskComplete when it is called later,
  //   after starting destruction of the BrowserContext - see the description
  //   of the next test step below).
  constexpr uint64_t kRemoveMask =
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA |
      chrome_browsing_data_remover::DEFERRED_COOKIE_DELETION_DATA_TYPES;
  content::BrowserContext* browser_context = GetBrowser()->profile();
  content::BrowsingDataRemover* remover =
      browser_context->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(),       // delete_begin
      base::Time::Max(),  // delete_end
      kRemoveMask, content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      &completion_observer);

  // Close the incognito browser.  This will tear down its
  // Profile/BrowserContext, which will tear down the StoragePartition, which
  // will tear down some mojo::Remote(s), which will end up running the closures
  // returned from CreateTaskCompletionClosureForMojo (see the previous test
  // step), which will run BrowsingDataRemoverImpl::OnTaskComplete.  In
  // https://crbug.com/1216406 OnTaskComplete would attempt to use its
  // `browser_context_` (half-way destructed at this point) to get a
  // StoragePartition and this would lead to DumpWithoutCrashing initially (and
  // potentially crashes down the line).
  CloseBrowserSynchronously(GetBrowser());

  // Verify that the completion observer will get notified, even if there might
  // have been a failure with the removal.
  completion_observer.BlockUntilCompletion();

  // Expect that removing the cookies failed, because the StoragePartition has
  // been already gone by the time BrowsingDataRemoverImpl::OnTaskComplete run.
  EXPECT_TRUE(content::StoragePartition::REMOVE_DATA_MASK_COOKIES &
              completion_observer.failed_data_types());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, SessionCookieDeletion) {
  TestSiteData("SessionCookie", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, LocalStorageDeletion) {
  TestSiteData("LocalStorage", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       LocalStorageIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("LocalStorage", GetParam());
}

// TODO(crbug.com/41348517): DISABLED until session storage is working
// correctly. Add Incognito variant when this is re-enabled.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       DISABLED_SessionStorageDeletion) {
  TestSiteData("SessionStorage", GetParam());
}

// SessionStorage is not supported by site data counting and the cookie tree
// model but we can test the web visible behavior.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       SessionStorageDeletionWebOnly) {
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));
  const std::string type = "SessionStorage";
  EXPECT_FALSE(HasDataForType(type));
  SetDataForType(type);
  EXPECT_TRUE(HasDataForType(type));
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA, GetParam());
  EXPECT_FALSE(HasDataForType(type));
}

// Test that session storage is not counted until crbug.com/772337 is fixed.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, SessionStorageCounting) {
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(0);
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(0);
  SetDataForType("SessionStorage");
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(0);
  EXPECT_TRUE(HasDataForType("SessionStorage"));
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, ServiceWorkerDeletion) {
  TestSiteData("ServiceWorker", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       ServiceWorkerIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("ServiceWorker", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, CacheStorageDeletion) {
  TestSiteData("CacheStorage", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       CacheStorageIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("CacheStorage", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, FileSystemDeletion) {
  TestSiteData("FileSystem", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       FileSystemIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("FileSystem", GetParam());
}

// Test that empty filesystems are deleted correctly.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       EmptyFileSystemDeletion) {
  TestEmptySiteData("FileSystem", GetParam());
}

// Test that empty filesystems are deleted correctly in incognito mode.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       EmptyFileSystemIncognitoDeletion) {
  UseIncognitoBrowser();
  TestEmptySiteData("FileSystem", GetParam());
}

// TODO(crbug.com/333756088): WebSQL is disabled everywhere except Android
// WebView.
#if !BUILDFLAG(IS_ANDROID)
#define MAYBE_WebSqlDeletion DISABLED_WebSqlDeletion
#else
#define MAYBE_WebSqlDeletion WebSqlDeletion
#endif
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, MAYBE_WebSqlDeletion) {
  TestSiteData("WebSql", GetParam());
}

// TODO(crbug.com/333756088): WebSQL is disabled everywhere except Android
// WebView.
#if !BUILDFLAG(IS_ANDROID)
#define MAYBE_WebSqlIncognitoDeletion DISABLED_WebSqlIncognitoDeletion
#else
#define MAYBE_WebSqlIncognitoDeletion WebSqlIncognitoDeletion
#endif
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       MAYBE_WebSqlIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("WebSql", GetParam());
}

// TODO(crbug.com/333756088): WebSQL is disabled everywhere except Android
// WebView.
#if !BUILDFLAG(IS_ANDROID)
#define MAYBE_EmptyWebSqlDeletion DISABLED_EmptyWebSqlDeletion
#else
#define MAYBE_EmptyWebSqlDeletion EmptyWebSqlDeletion
#endif
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       MAYBE_EmptyWebSqlDeletion) {
  TestEmptySiteData("WebSql", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, IndexedDbDeletion) {
  TestSiteData("IndexedDb", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       IndexedDbIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("IndexedDb", GetParam());
}

// Test that empty indexed dbs are deleted correctly.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, EmptyIndexedDb) {
  TestEmptySiteData("IndexedDb", GetParam());
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Test Media Licenses by creating one and checking it is counted by the
// cookie counter. Then delete it and check that the cookie counter is back
// to zero.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, MediaLicenseDeletion) {
  const std::string kMediaLicenseType = "MediaLicense";
  const TimeEnum delete_begin = GetParam();

  EXPECT_EQ(0, GetSiteDataCount());
  GURL url =
      embedded_test_server()->GetURL("/browsing_data/media_license.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(0);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  SetDataForType(kMediaLicenseType);
  EXPECT_EQ(1, GetSiteDataCount());
  ExpectTotalModelCount(1);
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));

  // Try to remove the Media Licenses using a time frame up until an hour ago,
  // which should not remove the recently created Media License.
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA, delete_begin,
                TimeEnum::kLastHour);
  EXPECT_EQ(1, GetSiteDataCount());
  ExpectTotalModelCount(1);
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));

  // Now try with a time range that includes the current time, which should
  // clear the Media License created for this test.
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA, delete_begin,
                TimeEnum::kMax);
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(0);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));
}

// Create and save a media license (which will be deleted in the following
// test).
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_MediaLicenseTimedDeletion) {
  const std::string kMediaLicenseType = "MediaLicense";

  EXPECT_EQ(0, GetSiteDataCount());

  GURL url =
      embedded_test_server()->GetURL("/browsing_data/media_license.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(0);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  SetDataForType(kMediaLicenseType);
  EXPECT_EQ(1, GetSiteDataCount());
  ExpectTotalModelCount(1);
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));
}

// Create and save a second media license, and then verify that timed deletion
// selects the correct license to delete.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       MediaLicenseTimedDeletion) {
  const std::string kMediaLicenseType = "MediaLicense";

  // As the PRE_ test should run first, there should be one media license
  // still stored. The time of it's creation should be sometime before
  // this test starts. This license will be for a different origin, and so wont
  // affect HasDataForType, but it still exists.
  LOG(INFO) << "MediaLicenseTimedDeletion starting @ " << kStartTime;
  EXPECT_EQ(1, GetSiteDataCount());

  GURL url =
      embedded_test_server()->GetURL("/browsing_data/media_license.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

#if BUILDFLAG(IS_MAC)
  // On some Macs the file system uses second granularity. So before
  // creating the second license, delay for 1 second so that the new
  // license's time is not the same second as |kStartTime|.
  base::PlatformThread::Sleep(base::Seconds(1));
#endif

  // This test should use a different domain than the PRE_ test, so there
  // should be no existing media license for it.
  // Note that checking HasDataForType() may result in an empty file being
  // created. Deleting licenses checks for any file within the time range
  // specified in order to delete all the files for the domain, so this may
  // cause problems (especially with Macs that use second granularity).
  // http://crbug.com/909829.
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  SetDataForType(kMediaLicenseType);
  ExpectTotalModelCount(1);
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));

  // As Clear Browsing Data typically deletes recent data (e.g. last hour,
  // last day, etc.), try to remove the Media Licenses created since the
  // the start of this test, which should only delete the just created
  // media license, and leave the one created by the PRE_ test.
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
                TimeEnum::kStart);
  EXPECT_EQ(1, GetSiteDataCount());
  ExpectTotalModelCount(1);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  // Now try with a time range that includes all time, which should
  // clear the media license created by the PRE_ test.
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA);
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(0);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       MediaLicenseDeletionWithFilter) {
  const std::string kMediaLicenseType = "MediaLicense";

  GURL url =
      embedded_test_server()->GetURL("/browsing_data/media_license.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ExpectTotalModelCount(0);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  SetDataForType(kMediaLicenseType);

  ExpectTotalModelCount(1);
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));

  // Try to remove the Media Licenses using a deletelist that doesn't include
  // the current URL. Media License should not be deleted.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(
      url::Origin::CreateFromNormalizedTuple("https", "test-origin", 443));
  RemoveWithFilterAndWait(
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES,
      std::move(filter_builder));
  ExpectTotalModelCount(1);

  // Now try with a preservelist that includes the current URL. Media License
  // should not be deleted.
  filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kPreserve);
  filter_builder->AddOrigin(url::Origin::Create(url));
  RemoveWithFilterAndWait(
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES,
      std::move(filter_builder));
  ExpectTotalModelCount(1);

  // Now try with a deletelist that includes the current URL. Media License
  // should be deleted this time.
  filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(url::Origin::Create(url));
  RemoveWithFilterAndWait(
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES,
      std::move(filter_builder));
  ExpectTotalModelCount(0);
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

const std::vector<std::string> kStorageTypes{
    "Cookie",    "LocalStorage",  "FileSystem",   "SessionStorage",
    "IndexedDb", "ServiceWorker", "CacheStorage", "MediaLicense",
// TODO(crbug.com/333756088): WebSQL is disabled everywhere except Android
// WebView.
#if BUILDFLAG(IS_ANDROID)
    "WebSql",
#endif
};

// Test that storage doesn't leave any traces on disk.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_PRE_StorageRemovedFromDisk) {
  // Checking leveldb content fails in most cases. See
  // https://crbug.com/1238325.
  ASSERT_EQ(0, CheckUserDirectoryForString(kLocalHost, {},
                                           /*check_leveldb_content=*/false));
  ASSERT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(0);

  // To use secure-only features on a host name, we need an https server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  base::FilePath path;
  base::PathService::Get(content::DIR_TEST_DATA, &path);
  https_server.ServeFilesFromDirectory(path);
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL(kLocalHost, "/browsing_data/site_data.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));

  for (const std::string& type : kStorageTypes) {
    SetDataForType(type);
    EXPECT_TRUE(HasDataForType(type));
  }
  // TODO(crbug.com/40577815): Add more datatypes for testing. E.g.
  // notifications, payment handler, content settings, autofill, ...?
}

// Restart after creating the data to ensure that everything was written to
// disk.
//
// This depends on session restore being explicitly disabled by the test harness
// above. Otherwise, we'll restore the tabs, delete the data on disk, and the
// still-open tabs can get re-persisted.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_StorageRemovedFromDisk) {
  EXPECT_EQ(1, GetSiteDataCount());
  ExpectTotalModelCount(1);
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA |
                content::BrowsingDataRemover::DATA_TYPE_CACHE |
                chrome_browsing_data_remover::DATA_TYPE_HISTORY |
                chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS);
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(0);
}

// Check if any data remains after a deletion and a Chrome restart to force
// all writes to be finished.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, StorageRemovedFromDisk) {
  // Deletions should remove all traces of browsing data from disk
  // but there are a few bugs that need to be fixed.
  // Any addition to this list must have an associated TODO.
  static const std::vector<std::string> ignore_file_patterns = {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // TODO(crbug.com/40577815): Many leveldb files remain on ChromeOS. I
      // couldn't
      // reproduce this in manual testing, so it might be a timing issue when
      // Chrome is closed after the second test?
      "[0-9]{6}",
#endif
  };
  int found = CheckUserDirectoryForString(kLocalHost, ignore_file_patterns,
                                          /*check_leveldb_content=*/false);
  EXPECT_EQ(0, found) << "A non-ignored file contains the hostname.";
}

const std::vector<std::string> kSessionOnlyStorageTestTypes{
    "Cookie",    "LocalStorage",  "FileSystem",   "SessionStorage",
    "IndexedDb", "ServiceWorker", "CacheStorage", "MediaLicense",
// TODO(crbug.com/333756088): WebSQL is disabled everywhere except Android
// WebView.
#if BUILDFLAG(IS_ANDROID)
    "WebSql",
#endif
};

// Test that storage gets deleted if marked as SessionOnly.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_SessionOnlyStorageRemoved) {
  ExpectTotalModelCount(0);
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));

  for (const std::string& type : kSessionOnlyStorageTestTypes) {
    SetDataForType(type);
    EXPECT_TRUE(HasDataForType(type));
  }

  ExpectTotalModelCount(1);
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                 CONTENT_SETTING_SESSION_ONLY);
}

// TODO(crbug.com/40925336): Test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SessionOnlyStorageRemoved DISABLED_SessionOnlyStorageRemoved
#else
#define MAYBE_SessionOnlyStorageRemoved SessionOnlyStorageRemoved
#endif
// Restart to delete session only storage.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       MAYBE_SessionOnlyStorageRemoved) {
  // All cookies should have been deleted.
  ExpectTotalModelCount(0);
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));
  for (const std::string& type : kSessionOnlyStorageTestTypes) {
    EXPECT_FALSE(HasDataForType(type));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test that removing passwords, when System-proxy is enabled on Chrome OS,
// sends a request to System-proxy to clear the cached user credentials.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       SystemProxyClearsUserCredentials) {
  ash::SystemProxyManager::Get()->SetSystemProxyEnabledForTest(true);
  EXPECT_EQ(0, ash::SystemProxyClient::Get()
                   ->GetTestInterface()
                   ->GetClearUserCredentialsCount());
  RemoveAndWait(chrome_browsing_data_remover::DATA_TYPE_PASSWORDS);

  EXPECT_EQ(1, ash::SystemProxyClient::Get()
                   ->GetTestInterface()
                   ->GetClearUserCredentialsCount());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       RelatedWebsiteSetsDeletion) {
  const GURL kPrimaryUrl("https://subdomain.example.com:112");
  const GURL kSecondaryUrl("https://subidubi.testsite.com:55");

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  // Setting the HostContentSettingsMap's clock to test last_modified of
  // RuleMetaData below.
  base::SimpleTestClock test_clock;
  settings_map->SetClockForTesting(&test_clock);
  test_clock.SetNow(base::Time::Now());

  // Expected setting for the default grant.
  const ContentSettingPatternSource kExpectedSettingDefault(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_ASK),
      content_settings::ProviderType::kDefaultProvider,
      /*incognito=*/false);

  // Check that there are only default grants.
  ASSERT_THAT(
      settings_map->GetSettingsForOneType(ContentSettingsType::STORAGE_ACCESS),
      UnorderedElementsAre(kExpectedSettingDefault));
  ASSERT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS),
              UnorderedElementsAre(kExpectedSettingDefault));

  // Set RWS grants.
  content_settings::ContentSettingConstraints constraints;
  constraints.set_session_model(content_settings::mojom::SessionModel::DURABLE);
  constraints.set_decided_by_related_website_sets(true);
  settings_map->SetContentSettingDefaultScope(
      kPrimaryUrl, kSecondaryUrl, ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW, constraints);
  settings_map->SetContentSettingDefaultScope(
      kPrimaryUrl, kSecondaryUrl, ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW, constraints);

  // Check that the grants were set.
  content_settings::RuleMetaData expected_metadata;
  expected_metadata.SetFromConstraints(constraints);
  expected_metadata.set_last_modified(test_clock.Now());
  EXPECT_THAT(
      settings_map->GetSettingsForOneType(ContentSettingsType::STORAGE_ACCESS),
      UnorderedElementsAre(
          kExpectedSettingDefault,
          ContentSettingPatternSource(
              ContentSettingsPattern::FromURLToSchemefulSitePattern(
                  GURL(kPrimaryUrl)),  // https://[*.]example.com
              ContentSettingsPattern::FromURLToSchemefulSitePattern(
                  GURL(kSecondaryUrl)),  // https://[*.]testsite.com
              content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
              content_settings::ProviderType::kPrefProvider,
              /*incognito=*/false, expected_metadata)));
  EXPECT_THAT(
      settings_map->GetSettingsForOneType(
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS),
      UnorderedElementsAre(
          kExpectedSettingDefault,
          ContentSettingPatternSource(
              ContentSettingsPattern::FromURLNoWildcard(
                  GURL(kPrimaryUrl)),  // https://subdomain.example.com:112
              ContentSettingsPattern::FromURLToSchemefulSitePattern(
                  GURL(kSecondaryUrl)),
              content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
              content_settings::ProviderType::kPrefProvider,
              /*incognito=*/false, expected_metadata)));

  // Remove Related Website Sets storage grants.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(url::Origin::Create(kPrimaryUrl));
  RemoveWithFilterAndWait(
      content::BrowsingDataRemover::DATA_TYPE_RELATED_WEBSITE_SETS_PERMISSIONS,
      std::move(filter_builder));

  // Check that there's only the default grant left.
  EXPECT_THAT(
      settings_map->GetSettingsForOneType(ContentSettingsType::STORAGE_ACCESS),
      UnorderedElementsAre(kExpectedSettingDefault));
  EXPECT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS),
              UnorderedElementsAre(kExpectedSettingDefault));
}

// Some storage backend use a different code path for full deletions and
// partial deletions, so we need to test both.
INSTANTIATE_TEST_SUITE_P(
    All,
    BrowsingDataRemoverBrowserTestP,
    ::testing::Values(TimeEnum::kDefault, TimeEnum::kLastHour),
    [](const ::testing::TestParamInfo<
        BrowsingDataRemoverBrowserTestP::ParamType>& info) {
      switch (info.param) {
        case TimeEnum::kDefault:
          return "kDefault";
        case TimeEnum::kLastHour:
          return "kLastHour";
        case TimeEnum::kStart:
          return "kStart";
        case TimeEnum::kMax:
          return "kMax";
      }
    });
