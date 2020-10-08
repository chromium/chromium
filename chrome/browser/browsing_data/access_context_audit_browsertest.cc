// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace {

// Use host names that are explicitly included in test certificates.
constexpr char kTopLevelHost[] = "a.test";
constexpr char kEmbeddedHost[] = "b.test";

// The set of origin keyed storage APIs accessed by both the top level and
// embedded pages in testing.
const std::set<AccessContextAuditDatabase::StorageAPIType>&
    kOriginStorageTypes = {
        AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
        AccessContextAuditDatabase::StorageAPIType::kFileSystem,
        AccessContextAuditDatabase::StorageAPIType::kWebDatabase,
        AccessContextAuditDatabase::StorageAPIType::kServiceWorker,
        AccessContextAuditDatabase::StorageAPIType::kCacheStorage,
        AccessContextAuditDatabase::StorageAPIType::kIndexedDB};

// The number of non-expired cookies which are accessed by the top level and
// embedded pages.
const unsigned kTopLevelPageCookieCount = 1u;
const unsigned kEmbeddedPageCookieCount = 2u;

std::string GetPathWithHostAndPortReplaced(const std::string& original_path,
                                           net::HostPortPair host_port_pair) {
  base::StringPairs replacement_text = {
      {"REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()}};
  return net::test_server::GetFilePathWithReplacements(original_path,
                                                       replacement_text);
}

// Checks if the cookie defined by |name|, |domain| and |path| is present in
// |cookies|, and that the record associating access to it to |top_frame_origin|
// is present in |record_list|. If |compare_host_only| is set, only the host
// portion of |top_frame_origin| will be used for comparison.
void CheckContainsCookieAndRecord(
    const std::vector<net::CanonicalCookie>& cookies,
    const std::vector<AccessContextAuditDatabase::AccessRecord>& record_list,
    const url::Origin& top_frame_origin,
    const std::string& name,
    const std::string& domain,
    const std::string& path,
    bool compare_host_only = false) {
  EXPECT_NE(
      std::find_if(
          record_list.begin(), record_list.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return record.type ==
                       AccessContextAuditDatabase::StorageAPIType::kCookie &&
                   (compare_host_only
                        ? record.top_frame_origin.host() ==
                              top_frame_origin.host()
                        : record.top_frame_origin == top_frame_origin) &&
                   record.name == name && record.domain == domain &&
                   record.path == path;
          }),
      record_list.end());

  EXPECT_NE(std::find_if(cookies.begin(), cookies.end(),
                         [=](const net::CanonicalCookie& cookie) {
                           return cookie.Name() == name &&
                                  cookie.Domain() == domain &&
                                  cookie.Path() == path;
                         }),
            cookies.end());
}

// Check that |record_list| contains a record indicating |origin| accessed
// storage while navigated to |top_frame_origin| for each storage type in
// |types|. If |compare_host_only| is set, only the host portion of the origins
// is used for comparison.
void CheckContainsOriginStorageRecords(
    const std::vector<AccessContextAuditDatabase::AccessRecord>& record_list,
    const std::set<AccessContextAuditDatabase::StorageAPIType>& types,
    const url::Origin& origin,
    const url::Origin& top_frame_origin,
    bool compare_host_only = false) {
  for (auto type : types) {
    EXPECT_NE(std::find_if(
                  record_list.begin(), record_list.end(),
                  [=](const AccessContextAuditDatabase::AccessRecord& record) {
                    return record.type == type &&
                           (compare_host_only
                                ? record.top_frame_origin.host() ==
                                          top_frame_origin.host() &&
                                      record.origin.host() == origin.host()
                                : record.top_frame_origin == top_frame_origin &&
                                      record.origin == origin);
                  }),
              record_list.end());
  }
}

// Calls the accessStorage javascript function and awaits its completion for
// each frame in the active web contents for |browser|.
void EnsurePageAccessedStorage(Browser* browser) {
  auto frames =
      browser->tab_strip_model()->GetActiveWebContents()->GetAllFrames();
  for (auto* frame : frames) {
    ASSERT_TRUE(content::EvalJs(
                    frame, "(async () => { return await accessStorage();})()")
                    .value.GetBool());
  }
}

}  // namespace

class CookiesTreeObserver : public CookiesTreeModel::Observer {
 public:
  void AwaitTreeModelEndBatch() {
    run_loop = std::make_unique<base::RunLoop>();
    run_loop->Run();
  }

  void TreeModelEndBatch(CookiesTreeModel* model) override { run_loop->Quit(); }

  void TreeNodesAdded(ui::TreeModel* model,
                      ui::TreeModelNode* parent,
                      size_t start,
                      size_t count) override {}
  void TreeNodesRemoved(ui::TreeModel* model,
                        ui::TreeModelNode* parent,
                        size_t start,
                        size_t count) override {}
  void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) override {
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop;
};

class AccessContextAuditBrowserTest : public InProcessBrowserTest {
 public:
  AccessContextAuditBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kClientStorageAccessContextAuditing);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    top_level_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    embedded_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    top_level_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_.Start());
    ASSERT_TRUE(top_level_.Start());
  }

  std::vector<AccessContextAuditDatabase::AccessRecord> GetAllAccessRecords() {
    base::RunLoop run_loop;
    std::vector<AccessContextAuditDatabase::AccessRecord> records_out;
    AccessContextAuditServiceFactory::GetForProfile(browser()->profile())
        ->GetAllAccessRecords(base::BindLambdaForTesting(
            [&](std::vector<AccessContextAuditDatabase::AccessRecord> records) {
              records_out = records;
              run_loop.QuitWhenIdle();
            }));
    run_loop.Run();
    return records_out;
  }

  std::vector<net::CanonicalCookie> GetAllCookies() {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies_out;
    content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
        ->GetCookieManagerForBrowserProcess()
        ->GetAllCookies(base::BindLambdaForTesting(
            [&](const std::vector<net::CanonicalCookie>& cookies) {
              cookies_out = cookies;
              run_loop.QuitWhenIdle();
            }));
    run_loop.Run();
    return cookies_out;
  }

  // Navigate to a page that accesses cookies and storage APIs and also embeds
  // a site which also accesses cookies and storage APIs.
  void NavigateToTopLevelPage() {
    ui_test_utils::NavigateToURL(browser(), top_level_url());
    base::RunLoop().RunUntilIdle();
    EnsurePageAccessedStorage(browser());
  }

  // Navigate directly to the embedded page.
  void NavigateToEmbeddedPage() {
    ui_test_utils::NavigateToURL(browser(), embedded_url());
    base::RunLoop().RunUntilIdle();
    EnsurePageAccessedStorage(browser());
  }

  url::Origin top_level_origin() {
    return url::Origin::Create(top_level_.GetURL(kTopLevelHost, "/"));
  }

  url::Origin embedded_origin() {
    return url::Origin::Create(embedded_.GetURL(kEmbeddedHost, "/"));
  }

  GURL top_level_url() {
    std::string replacement_path = GetPathWithHostAndPortReplaced(
        "/browsing_data/embeds_storage_accessor.html",
        net::HostPortPair::FromURL(embedded_.GetURL(kEmbeddedHost, "/")));
    return top_level_.GetURL(kTopLevelHost, replacement_path);
  }

  GURL embedded_url() {
    return embedded_.GetURL(kEmbeddedHost,
                            "/browsing_data/storage_accessor.html");
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer top_level_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer embedded_{net::EmbeddedTestServer::TYPE_HTTPS};

  std::vector<AccessContextAuditDatabase::AccessRecord> records_;
};

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, PRE_PRE_RemoveRecords) {
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // Check storage accesses have been correctly recorded.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();

  // Each of the embedded page's cookies and origin storage APIs will be
  // accessed in two contexts, while the top level page's will only be
  // acceesed in one.
  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size();
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);

  CheckContainsCookieAndRecord(cookies, records, top_level_origin(), "embedder",
                               kTopLevelHost, "/");
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "persistent", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "session_only", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "persistent", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "session_only", kEmbeddedHost, "/");
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    top_level_origin(), top_level_origin());
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    embedded_origin(), top_level_origin());
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    embedded_origin(), embedded_origin());
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, PRE_RemoveRecords) {
  // Check that only persistent records have been persisted across restart.
  // Unfortunately the correct top frame origin is lost in the test as the
  // embedded test servers will have changed port, so only the host can be
  // reliably compared.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();

  // A single non-persistent cookie on the TopLevel page should have been
  // removed.
  unsigned expected_cookie_records =
      2 * (kEmbeddedPageCookieCount - 1) + kTopLevelPageCookieCount;
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size();
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount - 1);

  CheckContainsCookieAndRecord(cookies, records, top_level_origin(), "embedder",
                               kTopLevelHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);

  // All origin keyed storage types should persist across restart.
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    top_level_origin(), top_level_origin(),
                                    /* compare_host_only */ true);
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    embedded_origin(), top_level_origin(),
                                    /* compare_host_only */ true);
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    embedded_origin(), embedded_origin(),
                                    /* compare_host_only */ true);
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, RemoveRecords) {
  // Immediately remove all records and ensure no record remains.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(base::Time(), base::Time::Max(),
                          ChromeBrowsingDataRemoverDelegate::ALL_DATA_TYPES,
                          ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES,
                          &completion_observer);
  completion_observer.BlockUntilCompletion();

  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 0u);
  EXPECT_EQ(cookies.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, PRE_CheckSessionOnly) {
  // Check that a content setting of SESSION_ONLY results in records being
  // cleared across browser restart.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                ContentSetting::CONTENT_SETTING_SESSION_ONLY);

  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // Confirm that all accesses are initially recorded.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size();
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, CheckSessionOnly) {
  // Confirm all records have been removed.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 0u);
  EXPECT_EQ(cookies.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, RemoveHistory) {
  // Check that removing all history entries for an origin also removes all
  // records where that origin is the top frame origin.
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size();
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);

  // Remove the history entry for the navigation to the page which embeds
  // storage_accessor.html.
  auto* history_service = HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
  history_service->DeleteURLs({top_level_url()});
  base::RunLoop run_loop;
  history_service->FlushForTest(run_loop.QuitClosure());
  run_loop.Run();

  // Check that only records for accesses which occurred during the direct
  // navigation to storage_accessor.html exist.
  records = GetAllAccessRecords();
  cookies = GetAllCookies();
  expected_cookie_records = kEmbeddedPageCookieCount;
  expected_origin_storage_records = kOriginStorageTypes.size();

  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "persistent", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "session_only", kEmbeddedHost, "/");
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    embedded_origin(), embedded_origin());

  // The actual cookie set by the top level page should remain present, as
  // the history deletion should only affect the access record.
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);
  EXPECT_NE(std::find_if(cookies.begin(), cookies.end(),
                         [=](const net::CanonicalCookie& cookie) {
                           return cookie.Name() == "embedder" &&
                                  cookie.Domain() == kTopLevelHost &&
                                  cookie.Path() == "/";
                         }),
            cookies.end());
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, TreeModelDeletion) {
  // Check that removing cookies and storage API usage via the CookiesTreeModel
  // also removes the associated access records.
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  auto cookies = GetAllCookies();
  auto records = GetAllAccessRecords();
  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size();
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);

  auto tree_model = CookiesTreeModel::CreateForProfile(browser()->profile());
  CookiesTreeObserver observer;
  tree_model->AddCookiesTreeObserver(&observer);
  observer.AwaitTreeModelEndBatch();

  tree_model->DeleteAllStoredObjects();

  // The CookiesTreeModel does not provide a notification upon completion of
  // actual storage deletions. There is no graceful way to ensure that the
  // cookie deletions have actually occurred and been reported to the audit
  // service by the cookie manager.
  while (GetAllAccessRecords().size()) {
    base::RunLoop().RunUntilIdle();
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  records = GetAllAccessRecords();
  cookies = GetAllCookies();

  EXPECT_EQ(records.size(), 0u);
  EXPECT_EQ(cookies.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, MultipleAccesses) {
  // Ensure that renavigating to a page in the same tab correctly re-records
  // accesses.
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  AccessContextAuditServiceFactory::GetForProfile(browser()->profile())
      ->SetClockForTesting(&clock);

  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // Check all records have the initial access time.
  auto records = GetAllAccessRecords();
  for (const auto& record : records)
    EXPECT_EQ(record.last_access_time, clock.Now());

  // Renavigate to the same pages, this should update the access times on all
  // records.
  clock.Advance(base::TimeDelta::FromHours(1));
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // All records should now have the updated time.
  records = GetAllAccessRecords();
  for (const auto& record : records)
    EXPECT_EQ(record.last_access_time, clock.Now());
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, TabClosed) {
  // Ensure closing a tab correctly flushes access records.
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // Close the previous tab, but keep the browser active to ensure the profile
  // does not begin destruction.
  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabStripModel::CLOSE_NONE);

  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size();
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);

  CheckContainsCookieAndRecord(cookies, records, top_level_origin(), "embedder",
                               kTopLevelHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "session_only", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "session_only", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    top_level_origin(), top_level_origin(),
                                    /* compare__host_only */ true);
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    embedded_origin(), top_level_origin(),
                                    /* compare__host_only */ true);
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    embedded_origin(), embedded_origin(),
                                    /* compare_host_only */ true);
}

class AccessContextAuditSessionRestoreBrowserTest
    : public AccessContextAuditBrowserTest {
 public:
  void SetUpOnMainThread() override {
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
    AccessContextAuditBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(AccessContextAuditSessionRestoreBrowserTest,
                       PRE_RestoreSession) {
  // Navigate to test URLS which set a mixture of persistent and non-persistent
  // cookies.
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // Confirm that all accesses are initially recorded.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();

  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size();
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditSessionRestoreBrowserTest,
                       RestoreSession) {
  // Check all access records have been correctly persisted across restarts.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();

  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size();
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);

  CheckContainsCookieAndRecord(cookies, records, top_level_origin(), "embedder",
                               kTopLevelHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "session_only", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "session_only", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    top_level_origin(), top_level_origin(),
                                    /* compare__host_only */ true);
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    embedded_origin(), top_level_origin(),
                                    /* compare__host_only */ true);
  CheckContainsOriginStorageRecords(records, kOriginStorageTypes,
                                    embedded_origin(), embedded_origin(),
                                    /* compare_host_only */ true);
}
