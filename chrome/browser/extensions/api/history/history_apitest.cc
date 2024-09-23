// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

namespace {

class AddSyncedVisitTask : public history::HistoryDBTask {
 public:
  AddSyncedVisitTask(base::RunLoop* run_loop,
                     const GURL& url,
                     const history::VisitRow& visit)
      : run_loop_(run_loop), url_(url), visit_(visit) {}

  AddSyncedVisitTask(const AddSyncedVisitTask&) = delete;
  AddSyncedVisitTask& operator=(const AddSyncedVisitTask&) = delete;

  ~AddSyncedVisitTask() override = default;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    history::VisitID visit_id = backend->AddSyncedVisit(
        url_, u"Title", /*hidden=*/false, visit_, std::nullopt, std::nullopt);
    EXPECT_NE(visit_id, history::kInvalidVisitID);
    return true;
  }

  void DoneRunOnMainThread() override { run_loop_->QuitWhenIdle(); }

 private:
  raw_ptr<base::RunLoop> run_loop_;

  GURL url_;
  history::VisitRow visit_;
};

}  // namespace

namespace extensions {

using ContextType = ExtensionBrowserTest::ContextType;

class HistoryApiTest : public ExtensionApiTest,
                       public testing::WithParamInterface<ContextType> {
 public:
  HistoryApiTest() : ExtensionApiTest(GetParam()) {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    host_resolver()->AddRule("www.a.com", "127.0.0.1");
    host_resolver()->AddRule("www.b.com", "127.0.0.1");
  }

  static std::string ExecuteScript(const ExtensionId& extension_id,
                                   content::BrowserContext* context,
                                   const std::string& script) {
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        context, extension_id, script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    EXPECT_TRUE(result.is_string());
    return result.is_string() ? result.GetString() : std::string();
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         HistoryApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         HistoryApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

class SyncEnabledHistoryApiTest : public HistoryApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    HistoryApiTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &SyncEnabledHistoryApiTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Set up a fake SyncService that'll pretend Sync is enabled (without
    // actually talking to the server, or syncing anything). This is required
    // for tests exercising "foreign" (aka synced) visits - without this, the
    // HistoryBackend will notice that Sync isn't enabled and delete all foreign
    // visits.
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
  }

  base::CallbackListSubscription create_services_subscription_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         SyncEnabledHistoryApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         SyncEnabledHistoryApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(HistoryApiTest, MiscSearch) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/misc_search")) << message_;
}

IN_PROC_BROWSER_TEST_P(HistoryApiTest, TimedSearch) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/timed_search")) << message_;
}

IN_PROC_BROWSER_TEST_P(HistoryApiTest, Delete) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/delete")) << message_;
}

IN_PROC_BROWSER_TEST_P(HistoryApiTest, DeleteProhibited) {
  browser()->profile()->GetPrefs()->
      SetBoolean(prefs::kAllowDeletingBrowserHistory, false);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/delete_prohibited"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(HistoryApiTest, GetVisits) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/get_visits")) << message_;
}

IN_PROC_BROWSER_TEST_P(SyncEnabledHistoryApiTest, GetVisits_Foreign) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Setup: Add a foreign (aka synced) history entry to the DB.
  history::VisitRow visit;
  visit.visit_time = base::Time::Now() - base::Minutes(1);
  visit.originator_cache_guid = "some_originator";
  visit.transition = ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                               ui::PAGE_TRANSITION_CHAIN_START |
                                               ui::PAGE_TRANSITION_CHAIN_END);
  visit.is_known_to_sync = true;

  base::CancelableTaskTracker tracker;
  base::RunLoop run_loop;
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<AddSyncedVisitTask>(
          &run_loop, GURL("https://www.synced.com/"), visit),
      &tracker);
  run_loop.Run();

  static constexpr char kManifest[] =
      R"({
        "name": "chrome.history",
        "version": "0.1",
        "manifest_version": 2,
        "permissions": ["history"],
        "background": {
          "scripts": ["get_visits_foreign.js"],
          "persistent": true
        }
      })";
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
        function getVisits() {
          var query = {text: ''};
          chrome.history.search(query, function(results) {
            chrome.test.assertEq(1, results.length);
            chrome.test.assertEq('https://www.synced.com/', results[0].url);

            var id = results[0].id;
            chrome.history.getVisits(
                {url: results[0].url}, function(results) {
                  chrome.test.assertEq(1, results.length);
                  chrome.test.assertEq(id, results[0].id);
                  // The visit is *not* local!
                  chrome.test.assertFalse(results[0].isLocal);

                  chrome.test.succeed();
                });
          });
        }
      ]);)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("get_visits_foreign.js"), kBackgroundJs);

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

IN_PROC_BROWSER_TEST_P(HistoryApiTest, SearchAfterAdd) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("history/regular/search_after_add")) << message_;
}

// Test when History API is used from incognito mode, it has access to the
// regular mode history and actual incognito navigation has no effect on it.
IN_PROC_BROWSER_TEST_P(HistoryApiTest, Incognito) {
  // TODO(crbug.com/40937027): Convert test to use HTTPS and then remove.
  ScopedAllowHttpForHostnamesForTesting allow_http({"www.b.com"},
                                                   profile()->GetPrefs());

  ASSERT_TRUE(StartEmbeddedTestServer());
  // Setup.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  Profile* incognito_profile = incognito_browser->profile();
  ExtensionTestMessageListener regular_listener("regular ready");
  ExtensionTestMessageListener incognito_listener("incognito ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("history/incognito"),
                    {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(regular_listener.WaitUntilSatisfied());
  ASSERT_TRUE(incognito_listener.WaitUntilSatisfied());

  const ExtensionId& extension_id = extension->id();

  // Check if history is empty in regular mode.
  EXPECT_EQ("0",
            ExecuteScript(extension_id, profile(), "countItemsInHistory()"));

  // Insert an item in incognito mode.
  EXPECT_EQ("success",
            ExecuteScript(extension_id, incognito_profile, "addItem()"));

  // Check history in incognito mode.
  EXPECT_EQ("1", ExecuteScript(extension_id, incognito_profile,
                               "countItemsInHistory()"));

  // Check history in regular mode.
  EXPECT_EQ("1",
            ExecuteScript(extension_id, profile(), "countItemsInHistory()"));

  // Perform navigation in incognito mode.
  const GURL b_com =
      embedded_test_server()->GetURL("www.b.com", "/simple.html");
  content::TestNavigationObserver incognito_observer(
      incognito_browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, b_com));
  EXPECT_TRUE(incognito_observer.last_navigation_succeeded());

  // Check history in regular mode is not modified by incognito navigation.
  EXPECT_EQ("1",
            ExecuteScript(extension_id, profile(), "countItemsInHistory()"));

  // Check that history in incognito mode is not modified by navigation as
  // incognito navigations are not recorded in history.
  EXPECT_EQ("1", ExecuteScript(extension_id, incognito_profile,
                               "countItemsInHistory()"));

  // Perform navigation in regular mode.
  content::TestNavigationObserver regular_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_com));
  EXPECT_TRUE(regular_observer.last_navigation_succeeded());

  // Check history in regular mode is modified by navigation.
  EXPECT_EQ("2",
            ExecuteScript(extension_id, profile(), "countItemsInHistory()"));

  // Check history in incognito mode is modified by navigation.
  EXPECT_EQ("2", ExecuteScript(extension_id, incognito_profile,
                               "countItemsInHistory()"));
}

}  // namespace extensions
