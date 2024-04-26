// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browsing_data/browsing_data_remover_browsertest_base.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/media/clear_key_cdm_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#if BUILDFLAG(IS_MAC)
#include "base/threading/platform_thread.h"
#endif
#include "base/memory/scoped_refptr.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::BrowserThread;
using content::BrowsingDataFilterBuilder;

namespace {
static const char* kExampleHost = "example.com";
static const char* kLocalHost = "localhost";
}  // namespace

class IncognitoBrowsingDataBrowserTest
    : public BrowsingDataRemoverBrowserTestBase {
 public:
  IncognitoBrowsingDataBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {};
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    enabled_features.push_back(media::kExternalClearKeyForTesting);
#endif
    InitFeatureLists(std::move(enabled_features), {});
  }

  void SetUpOnMainThread() override {
    BrowsingDataRemoverBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule(kExampleHost, "127.0.0.1");
    UseIncognitoBrowser();
  }

  Browser* GetRegularBrowser() { return browser(); }

  Browser* GetIncognitoBrowser() { return GetBrowser(); }

  // Test a data type by creating a value in Incognito mode and checking it is
  // counted by the Incognito cookie counter and not by the regular mode one.
  // Then closing Incognito mode and ensuring it is not affecting a new
  // Incognito profile.
  void TestSiteData(const std::string& type) {
    Browser* regular_browser = GetRegularBrowser();
    Browser* incognito_browser = GetIncognitoBrowser();

    EXPECT_TRUE(regular_browser->profile()->IsRegularProfile());
    EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());

    // Ensure there is no prior data.
    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(regular_browser)));
    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(incognito_browser)));
    GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(regular_browser, url));

    // Even after navigation.
    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(incognito_browser)));
    ExpectTotalModelCount(incognito_browser, 0);
    EXPECT_FALSE(HasDataForType(type, GetActiveWebContents(incognito_browser)));

    // Set data type in Incognito mode, ensure only Incognito mode is affected.
    SetDataForType(type, GetActiveWebContents(incognito_browser));
    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(regular_browser)));
    EXPECT_EQ(1, GetSiteDataCount(GetActiveWebContents(incognito_browser)));
    ExpectTotalModelCount(regular_browser, 0);
    // TODO(crbug.com/40218898): Use a different approach to determine presence
    // of data that does not depend on UI code and has a better resolution when
    // 3PSP is fully enabled. ExpectTotalModelCount(incognito_browser, 1);
    // is not always true here.

    EXPECT_FALSE(HasDataForType(type, GetActiveWebContents(regular_browser)));
    EXPECT_TRUE(HasDataForType(type, GetActiveWebContents(incognito_browser)));

    // Restart Incognito and ensure it is empty.
    RestartIncognitoBrowser();
    incognito_browser = GetIncognitoBrowser();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));

    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(incognito_browser)));
    ExpectTotalModelCount(incognito_browser, 0);
    EXPECT_FALSE(HasDataForType(type, GetActiveWebContents(incognito_browser)));
  }

  // Test that storage systems like filesystem, where just an access creates an
  // empty store, are counted and deleted correctly.
  void TestEmptySiteData(const std::string& type) {
    Browser* regular_browser = GetRegularBrowser();
    Browser* incognito_browser = GetIncognitoBrowser();

    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(regular_browser)));
    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(incognito_browser)));
    ExpectTotalModelCount(regular_browser, 0);
    ExpectTotalModelCount(incognito_browser, 0);

    GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));
    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(incognito_browser)));
    ExpectTotalModelCount(incognito_browser, 0);
    // Opening a store of this type creates a site data entry in Incognito only.
    EXPECT_FALSE(HasDataForType(type, GetActiveWebContents(incognito_browser)));
    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(regular_browser)));
    EXPECT_EQ(1, GetSiteDataCount(GetActiveWebContents(incognito_browser)));
    ExpectTotalModelCount(regular_browser, 0);
    // TODO(crbug.com/40218898): Use a different approach to determine presence
    // of data that does not depend on UI code and has a better resolution when
    // 3PSP is fully enabled. ExpectTotalModelCount(incognito_browser, 1);
    // is not always true here.

    // Restart Incognito, ensure there is no residue from previous one.
    RestartIncognitoBrowser();
    incognito_browser = GetIncognitoBrowser();
    EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(incognito_browser)));
    ExpectTotalModelCount(incognito_browser, 0);
  }

  inline void ExpectTotalModelCount(Browser* browser, size_t expected) {
    std::unique_ptr<BrowsingDataModel> browsing_data_model =
        GetBrowsingDataModel(browser->profile());

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

// Test BrowsingDataRemover for downloads.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, Download) {
  DownloadAnItem();
  VerifyDownloadCount(0u, GetRegularBrowser()->profile());

  // Restart Incognito, ensure no residue.
  RestartIncognitoBrowser();
  VerifyDownloadCount(0u, GetIncognitoBrowser()->profile());
}

// Test that the salt for media device IDs is reset between Incognito sessions.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, MediaDeviceIdSalt) {
  auto get_salt = [&]() {
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
    return future.Get<1>();
  };
  std::string original_salt = get_salt();
  RestartIncognitoBrowser();
  std::string new_salt = get_salt();
  EXPECT_NE(original_salt, new_salt);
}

// Verify VideoDecodePerfHistory is cleared after restarting Incognito session
// and is not affecting regular mode.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest,
                       VideoDecodePerfHistory) {
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
        base::BindOnce(&IncognitoBrowsingDataBrowserTest::OnVideoDecodePerfInfo,
                       base::Unretained(this), &run_loop, &is_smooth,
                       &is_power_efficient));
    run_loop.Run();
  }
  EXPECT_FALSE(is_smooth);
  EXPECT_FALSE(is_power_efficient);

  // Verify it has not affected regular mode. Both |is_smooth| and
  // |is_power_efficient| should report true because the VideoDecodePerfHistory
  // optimistically returns true when it has no data.
  media::VideoDecodePerfHistory* regular_mode_video_decode_perf_history =
      GetRegularBrowser()->profile()->GetVideoDecodePerfHistory();
  {
    base::RunLoop run_loop;
    regular_mode_video_decode_perf_history->GetPerfInfo(
        media::mojom::PredictionFeatures::New(prediction_features),
        base::BindOnce(&IncognitoBrowsingDataBrowserTest::OnVideoDecodePerfInfo,
                       base::Unretained(this), &run_loop, &is_smooth,
                       &is_power_efficient));
    run_loop.Run();
  }
  EXPECT_TRUE(is_smooth);
  EXPECT_TRUE(is_power_efficient);

  // Restart Incognito.
  RestartIncognitoBrowser();
  video_decode_perf_history =
      GetBrowser()->profile()->GetVideoDecodePerfHistory();

  // Verify history no longer exists. Both |is_smooth| and |is_power_efficient|
  // should now report true because the VideoDecodePerfHistory optimistically
  // returns true when it has no data.
  {
    base::RunLoop run_loop;
    video_decode_perf_history->GetPerfInfo(
        media::mojom::PredictionFeatures::New(prediction_features),
        base::BindOnce(&IncognitoBrowsingDataBrowserTest::OnVideoDecodePerfInfo,
                       base::Unretained(this), &run_loop, &is_smooth,
                       &is_power_efficient));
    run_loop.Run();
  }
  EXPECT_TRUE(is_smooth);
  EXPECT_TRUE(is_power_efficient);
}

// Verifies that cache is reset after restarting Incognito.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, Cache) {
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

  RestartIncognitoBrowser();

  // The cache should be empty.
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url2));
}

IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest,
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

  // Regular profile should be unaffected.
  block_state = ExternalProtocolHandler::GetBlockState(
      "tel", &test_origin, GetRegularBrowser()->profile());
  ASSERT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);

  RestartIncognitoBrowser();
  profile = GetBrowser()->profile();

  block_state =
      ExternalProtocolHandler::GetBlockState("tel", &test_origin, profile);
  ASSERT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
}

IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, CookieDeletion) {
  TestSiteData("Cookie");
}

IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest,
                       SessionCookieDeletion) {
  TestSiteData("SessionCookie");
}

IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, LocalStorageDeletion) {
  TestSiteData("LocalStorage");
}

// TODO(crbug.com/41348517): DISABLED until session storage is working
// correctly.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest,
                       DISABLED_SessionStorageDeletion) {
  TestSiteData("SessionStorage");
}

// SessionStorage is not supported by site data counting and the cookie tree
// model but we can test the web visible behavior.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest,
                       SessionStorageDeletionWebOnly) {
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));
  const std::string type = "SessionStorage";
  EXPECT_FALSE(HasDataForType(type));
  SetDataForType(type);
  EXPECT_TRUE(HasDataForType(type));

  // No residue in regular mode.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetRegularBrowser(), url));
  EXPECT_FALSE(HasDataForType(type, GetActiveWebContents(GetRegularBrowser())));

  RestartIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));
  EXPECT_FALSE(HasDataForType(type));
}

IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest,
                       ServiceWorkerDeletion) {
  TestSiteData("ServiceWorker");
}

IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, CacheStorageDeletion) {
  TestSiteData("CacheStorage");
}

IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, FileSystemDeletion) {
  TestSiteData("FileSystem");
}

// Test that empty filesystems are deleted correctly.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest,
                       EmptyFileSystemDeletion) {
  TestEmptySiteData("FileSystem");
}

IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, IndexedDbDeletion) {
  TestSiteData("IndexedDb");
}

// Test that empty indexed dbs are deleted correctly.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, EmptyIndexedDb) {
  TestEmptySiteData("IndexedDb");
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Test Media Licenses by creating one and checking it is counted by the
// cookie counter. Then delete it and check that the cookie counter is back
// to zero.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest, MediaLicenseDeletion) {
  const std::string kMediaLicenseType = "MediaLicense";

  EXPECT_EQ(0, GetSiteDataCount());
  GURL url =
      embedded_test_server()->GetURL("/browsing_data/media_license.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));

  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(GetBrowser(), 0);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  // The new media license backend will not store media licenses explicitly
  // within CookieTreeModel, but the data will still be tracked through the
  // quota system.
  SetDataForType(kMediaLicenseType);
  EXPECT_EQ(1, GetSiteDataCount());
  ExpectTotalModelCount(GetBrowser(), 1);
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));

  // No residue in regular mode.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetRegularBrowser(), url));
  EXPECT_FALSE(HasDataForType(kMediaLicenseType,
                              GetActiveWebContents(GetRegularBrowser())));

  // Restart Incognito.
  RestartIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), url));

  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(GetBrowser(), 0);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

const std::vector<std::string> kStorageTypes{
    "Cookie",    "LocalStorage",  "FileSystem",   "SessionStorage",
    "IndexedDb", "ServiceWorker", "CacheStorage", "MediaLicense"};

// Test that storage doesn't leave any traces on disk.
IN_PROC_BROWSER_TEST_F(IncognitoBrowsingDataBrowserTest,
                       StorageDoesntWriteToDisk) {
  // Checking leveldb content fails in most cases. See https://crbug.com/1238325
  ASSERT_EQ(0, CheckUserDirectoryForString(kLocalHost, {},
                                           /*check_leveldb_content=*/false));
  ASSERT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(GetBrowser(), 0);

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

  int found = CheckUserDirectoryForString(kLocalHost, {},
                                          /*check_leveldb_content=*/false);
  EXPECT_EQ(0, found) << "A file contains the hostname.";

  EXPECT_EQ(0, GetSiteDataCount(GetActiveWebContents(GetRegularBrowser())));
  ExpectTotalModelCount(GetRegularBrowser(), 0);

  RestartIncognitoBrowser();

  EXPECT_EQ(0, GetSiteDataCount());
  ExpectTotalModelCount(GetBrowser(), 0);
}
