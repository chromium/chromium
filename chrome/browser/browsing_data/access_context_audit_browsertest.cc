// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
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
#include "content/public/test/fenced_frame_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#endif  // defined (OS_ANDROID)

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
  EXPECT_TRUE(
      base::ranges::any_of(
          record_list,
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return record.type ==
                       AccessContextAuditDatabase::StorageAPIType::kCookie &&
                   (compare_host_only
                        ? record.top_frame_origin.host() ==
                              top_frame_origin.host()
                        : record.top_frame_origin == top_frame_origin) &&
                   record.name == name && record.domain == domain &&
                   record.path == path;
          }));

  EXPECT_TRUE(
      base::ranges::any_of(cookies, [=](const net::CanonicalCookie& cookie) {
        return cookie.Name() == name && cookie.Domain() == domain &&
               cookie.Path() == path;
      }));
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
    bool found = base::ranges::any_of(
        record_list,
        [=](const AccessContextAuditDatabase::AccessRecord& record) {
          return record.type == type &&
                 (compare_host_only
                      ? record.top_frame_origin.host() ==
                                top_frame_origin.host() &&
                            record.origin.host() == origin.host()
                      : record.top_frame_origin == top_frame_origin &&
                            record.origin == origin);
        });
    // WebSQL in third-party contexts is disabled as of M97.
    EXPECT_EQ(
        found,
        origin == top_frame_origin ||
            type != AccessContextAuditDatabase::StorageAPIType::kWebDatabase);
  }
}

// Calls the accessStorage javascript function and awaits its completion for
// each frame in the active web contents for |browser|.
void EnsurePageAccessedStorage(content::WebContents* web_contents) {
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](content::RenderFrameHost* frame) {
        EXPECT_TRUE(
            content::EvalJs(frame,
                            "(async () => { return await accessStorage();})()")
                .value.GetBool());
      });
}

}  // namespace

class CookiesTreeObserver : public CookiesTreeModel::Observer {
 public:
  void AwaitTreeModelEndBatch() {
    run_loop = std::make_unique<base::RunLoop>();
    run_loop->Run();
  }

  void TreeModelEndBatchDeprecated(CookiesTreeModel* model) override {
    run_loop->Quit();
  }

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

class AccessContextAuditBrowserTest : public PlatformBrowserTest {
 public:
  AccessContextAuditBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kClientStorageAccessContextAuditing);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    top_level_.ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    embedded_.ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    top_level_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_.Start());
    ASSERT_TRUE(top_level_.Start());
  }

  std::vector<AccessContextAuditDatabase::AccessRecord> GetAllAccessRecords() {
    base::RunLoop run_loop;
    std::vector<AccessContextAuditDatabase::AccessRecord> records_out;
    AccessContextAuditServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this))
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
    chrome_test_utils::GetProfile(this)
        ->GetDefaultStoragePartition()
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
    ASSERT_TRUE(content::NavigateToURL(
        chrome_test_utils::GetActiveWebContents(this), top_level_url()));
    base::RunLoop().RunUntilIdle();
    EnsurePageAccessedStorage(chrome_test_utils::GetActiveWebContents(this));
  }

  // Navigate directly to the embedded page.
  void NavigateToEmbeddedPage() {
    ASSERT_TRUE(content::NavigateToURL(
        chrome_test_utils::GetActiveWebContents(this), embedded_url()));
    base::RunLoop().RunUntilIdle();
    EnsurePageAccessedStorage(chrome_test_utils::GetActiveWebContents(this));
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
  // Subtract 1 as third-party context WebSQL is disabled as of M97.
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size() - 1;
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
  // Subtract 1 as third-party context WebSQL is disabled as of M97.
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size() - 1;
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

// TODO(crbug.com/1317431): WebSQL does not work on Fuchsia.
// crbug.com/1429828: Test is flaky on Mac and Linux
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_RemoveRecords DISABLED_RemoveRecords
#else
#define MAYBE_RemoveRecords RemoveRecords
#endif
IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, MAYBE_RemoveRecords) {
  // Immediately remove all records and ensure no record remains.
  content::BrowsingDataRemover* remover =
      chrome_test_utils::GetProfile(this)->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(base::Time(), base::Time::Max(),
                          chrome_browsing_data_remover::ALL_DATA_TYPES,
                          chrome_browsing_data_remover::ALL_ORIGIN_TYPES,
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
  HostContentSettingsMap* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                ContentSetting::CONTENT_SETTING_SESSION_ONLY);

  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // Confirm that all accesses are initially recorded.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  // Subtract 1 as third-party context WebSQL is disabled as of M97.
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size() - 1;
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);
}

// TODO(crbug.com/1317431): WebSQL does not work on Fuchsia.
// crbug.com/1429828: Test is flaky on Mac and Linux
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_CheckSessionOnly DISABLED_CheckSessionOnly
#else
#define MAYBE_CheckSessionOnly CheckSessionOnly
#endif
IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, MAYBE_CheckSessionOnly) {
  // Confirm all records have been removed.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 0u);
  EXPECT_EQ(cookies.size(), 0u);
}

// TODO(crbug.com/1317431): WebSQL does not work on Fuchsia.
// crbug.com/1429828: Test is flaky on Mac and Linux
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_RemoveHistory DISABLED_RemoveHistory
#else
#define MAYBE_RemoveHistory RemoveHistory
#endif
IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, MAYBE_RemoveHistory) {
  // Check that removing all history entries for an origin also removes all
  // records where that origin is the top frame origin.
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  // Subtract 1 as third-party context WebSQL is disabled as of M97.
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size() - 1;
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);

  // Remove the history entry for the navigation to the page which embeds
  // storage_accessor.html.
  auto* history_service = HistoryServiceFactory::GetForProfile(
      chrome_test_utils::GetProfile(this), ServiceAccessType::EXPLICIT_ACCESS);
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
  EXPECT_TRUE(
      base::ranges::any_of(cookies, [=](const net::CanonicalCookie& cookie) {
        return cookie.Name() == "embedder" &&
               cookie.Domain() == kTopLevelHost && cookie.Path() == "/";
      }));
}

// TODO(crbug.com/1435528): Test is no longer needed.
IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest,
                       DISABLED_TreeModelDeletion) {
  // Check that removing cookies and storage API usage via the CookiesTreeModel
  // also removes the associated access records.
  content::CookieChangeObserver cookie_observer(
      chrome_test_utils::GetActiveWebContents(this), 9);
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();
  cookie_observer.Wait();

  auto cookies = GetAllCookies();
  auto records = GetAllAccessRecords();
  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  // Subtract 1 as third-party context WebSQL is disabled as of M97.
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size() - 1;
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);

  auto tree_model = CookiesTreeModel::CreateForProfileDeprecated(
      chrome_test_utils::GetProfile(this));
  {
    CookiesTreeObserver observer;
    tree_model->AddCookiesTreeObserver(&observer);
    observer.AwaitTreeModelEndBatch();
    tree_model->RemoveCookiesTreeObserver(&observer);
  }

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

// TODO(crbug.com/1317431): WebSQL does not work on Fuchsia.
// crbug.com/1429828: Test is flaky on Mac and Linux
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_MultipleAccesses DISABLED_MultipleAccesses
#else
#define MAYBE_MultipleAccesses MultipleAccesses
#endif
IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, MAYBE_MultipleAccesses) {
  // Ensure that renavigating to a page in the same tab correctly re-records
  // accesses.
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  AccessContextAuditServiceFactory::GetForProfile(
      chrome_test_utils::GetProfile(this))
      ->SetClockForTesting(&clock);

  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // Check all records have the initial access time.
  auto records = GetAllAccessRecords();
  for (const auto& record : records)
    EXPECT_EQ(record.last_access_time, clock.Now());

  // Renavigate to the same pages, this should update the access times on all
  // records.
  clock.Advance(base::Hours(1));
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // All records should now have the updated time.
  records = GetAllAccessRecords();
  for (const auto& record : records)
    EXPECT_EQ(record.last_access_time, clock.Now());
}

// TODO(crbug.com/1317431): WebSQL does not work on Fuchsia.
// crbug.com/1429828: Test is flaky on Mac and Linux
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_TabClosed DISABLED_TabClosed
#else
#define MAYBE_TabClosed TabClosed
#endif
IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, MAYBE_TabClosed) {
  // Ensure closing a tab correctly flushes access records.
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();

  // Close the previous tab, keeping the browser active if required to ensure
  // the profile does not begin destruction.
#if BUILDFLAG(IS_ANDROID)
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  tab_model->CloseTabAt(tab_model->GetActiveIndex());
#else
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
#endif  // defined (OS_ANDROID)

  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  // Subtract 1 as third-party context WebSQL is disabled as of M97.
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size() - 1;
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

// Enabling session restore behavior on desktop preserves non-persistent cookies
// when the browser restarts. Android has a superficially similar behavior where
// tabs are re-opened after close, but non-persistent cookies are not preserved,
// making this test only applicable to desktop.
#if !BUILDFLAG(IS_ANDROID)
class AccessContextAuditSessionRestoreBrowserTest
    : public AccessContextAuditBrowserTest {
 public:
  void SetUpOnMainThread() override {
    SessionStartupPref::SetStartupPref(
        chrome_test_utils::GetProfile(this),
        SessionStartupPref(SessionStartupPref::LAST));
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
  // Subtract 1 as third-party context WebSQL is disabled as of M97.
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size() - 1;
  EXPECT_EQ(records.size(),
            expected_cookie_records + expected_origin_storage_records);
  EXPECT_EQ(cookies.size(),
            kEmbeddedPageCookieCount + kTopLevelPageCookieCount);
}

// TODO(crbug.com/1317431): WebSQL does not work on Fuchsia.
// TODO(crbug.com/1431000): Failing consistently on Linux Wayland.
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)
#define MAYBE_RestoreSession DISABLED_RestoreSession
#else
#define MAYBE_RestoreSession RestoreSession
#endif
IN_PROC_BROWSER_TEST_F(AccessContextAuditSessionRestoreBrowserTest,
                       MAYBE_RestoreSession) {
  // Check all access records have been correctly persisted across restarts.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();

  unsigned expected_cookie_records =
      2 * kEmbeddedPageCookieCount + kTopLevelPageCookieCount;
  // Subtract 1 as third-party context WebSQL is disabled as of M97.
  unsigned expected_origin_storage_records = 3 * kOriginStorageTypes.size() - 1;
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
#endif  // !BUILDFLAG(IS_ANDROID)

class AccessContextAuditFencedFrameBrowserTest
    : public AccessContextAuditBrowserTest {
 public:
  AccessContextAuditFencedFrameBrowserTest() = default;
  ~AccessContextAuditFencedFrameBrowserTest() override = default;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

IN_PROC_BROWSER_TEST_F(AccessContextAuditFencedFrameBrowserTest,
                       AccessShouldNotBeRecorded) {
  ASSERT_TRUE(content::NavigateToURL(
      GetWebContents(), top_level_.GetURL(kTopLevelHost, "/empty.html")));
  content::RenderFrameHost* ff = fenced_frame_test_helper().CreateFencedFrame(
      GetWebContents()->GetPrimaryMainFrame(), embedded_url());

  EXPECT_TRUE(
      content::EvalJs(ff, "(async () => { return await accessStorage();})()")
          .value.GetBool());

  auto records = GetAllAccessRecords();
  EXPECT_EQ(records.size(), 0u);
}
