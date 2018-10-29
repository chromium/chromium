// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {
namespace declarative_net_request {
namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

// Returns true if |window.scriptExecuted| is true for the given frame.
bool WasFrameWithScriptLoaded(content::RenderFrameHost* rfh) {
  if (!rfh)
    return false;
  bool script_resource_was_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      rfh, "domAutomationController.send(!!window.scriptExecuted)",
      &script_resource_was_loaded));
  return script_resource_was_loaded;
}

// Used to monitor requests that reach the RulesetManager.
class URLRequestMonitor : public RulesetManager::TestObserver {
 public:
  explicit URLRequestMonitor(GURL url) : url_(std::move(url)) {}

  // This is called from both the UI and IO thread. Clients should ensure
  // there's no race.
  bool GetAndResetRequestSeen(bool new_val) {
    bool return_val = request_seen_;
    request_seen_ = new_val;
    return return_val;
  }

 private:
  // RulesetManager::TestObserver implementation.
  void OnEvaluateRequest(const WebRequestInfo& request,
                         bool is_incognito_context) override {
    if (request.url == url_)
      GetAndResetRequestSeen(true);
  }

  GURL url_;
  bool request_seen_ = false;

  DISALLOW_COPY_AND_ASSIGN(URLRequestMonitor);
};

// Used to wait till the number of rulesets managed by the RulesetManager reach
// a certain count.
class RulesetCountWaiter : public RulesetManager::TestObserver {
 public:
  RulesetCountWaiter() = default;

  void WaitForRulesetCount(size_t count) {
    {
      base::AutoLock lock(lock_);
      ASSERT_FALSE(expected_count_);
      if (current_count_ == count)
        return;
      expected_count_ = count;
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    run_loop_->Run();
  }

 private:
  // RulesetManager::TestObserver implementation.
  void OnRulesetCountChanged(size_t count) override {
    base::AutoLock lock(lock_);
    current_count_ = count;
    if (expected_count_ != count)
      return;

    ASSERT_TRUE(run_loop_.get());

    // The run-loop has either started or a task on the UI thread to start it is
    // underway. RunLoop::Quit is thread-safe and should post a task to the UI
    // thread to quit the run-loop.
    run_loop_->Quit();
    expected_count_.reset();
  }

  // Accessed on both the UI and IO threads. Access is synchronized using
  // |lock_|.
  size_t current_count_ = 0;
  base::Optional<size_t> expected_count_;
  std::unique_ptr<base::RunLoop> run_loop_;

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(RulesetCountWaiter);
};

// Helper to set (and reset on destruction) the given
// RulesetManager::TestObserver on the IO thread. Lifetime of |observer| should
// be managed by clients.
class ScopedRulesetManagerTestObserver {
 public:
  ScopedRulesetManagerTestObserver(RulesetManager::TestObserver* observer,
                                   scoped_refptr<InfoMap> info_map)
      : info_map_(std::move(info_map)) {
    SetRulesetManagerTestObserver(observer);
  }

  ~ScopedRulesetManagerTestObserver() {
    SetRulesetManagerTestObserver(nullptr);
  }

 private:
  void SetRulesetManagerTestObserver(RulesetManager::TestObserver* observer) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            [](RulesetManager::TestObserver* observer, InfoMap* info_map) {
              info_map->GetRulesetManager()->SetObserverForTest(observer);
            },
            observer, base::RetainedRef(info_map_)));
    content::RunAllTasksUntilIdle();
  }

  scoped_refptr<InfoMap> info_map_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRulesetManagerTestObserver);
};

// Helper to wait for warnings thrown for a given extension.
class WarningServiceObserver : public WarningService::Observer {
 public:
  WarningServiceObserver(WarningService* warning_service,
                         const ExtensionId& extension_id)
      : observer_(this), extension_id_(extension_id) {
    observer_.Add(warning_service);
  }

  // Should only be called once per WarningServiceObserver lifetime.
  void WaitForWarning() { run_loop_.Run(); }

 private:
  // WarningService::TestObserver override:
  void ExtensionWarningsChanged(
      const ExtensionIdSet& affected_extensions) override {
    if (!base::ContainsKey(affected_extensions, extension_id_))
      return;

    run_loop_.Quit();
  }

  ScopedObserver<WarningService, WarningService::Observer> observer_;
  const ExtensionId extension_id_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WarningServiceObserver);
};

class DeclarativeNetRequestBrowserTest
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<ExtensionLoadType> {
 public:
  DeclarativeNetRequestBrowserTest() {}

  // ExtensionBrowserTest overrides:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    base::FilePath test_root_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
    test_root_path = test_root_path.AppendASCII("extensions")
                         .AppendASCII("declarative_net_request");
    embedded_test_server()->ServeFilesFromDirectory(test_root_path);

    embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&DeclarativeNetRequestBrowserTest::MonitorRequest,
                            base::Unretained(this)));

    content::SetupCrossSiteRedirector(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // At this point, the notification service is initialized but the profile
    // and extensions have not. Initialize |background_page_ready_listener_| to
    // listen for messages from extensions.
    CHECK(content::NotificationService::current());

    background_page_ready_listener_ =
        std::make_unique<ExtensionTestMessageListener>("ready",
                                                       false /*will_reply*/);

    ExtensionBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }

 protected:
  content::WebContents* web_contents(Browser* browser) const {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* web_contents() const { return web_contents(browser()); }

  content::RenderFrameHost* GetMainFrame(Browser* browser) const {
    return web_contents(browser)->GetMainFrame();
  }

  content::RenderFrameHost* GetMainFrame() const {
    return GetMainFrame(browser());
  }

  content::RenderFrameHost* GetFrameByName(const std::string& name) const {
    return content::FrameMatchingPredicate(
        web_contents(), base::Bind(&content::FrameMatchesName, name));
  }

  content::PageType GetPageType(Browser* browser) const {
    return web_contents(browser)
        ->GetController()
        .GetLastCommittedEntry()
        ->GetPageType();
  }

  content::PageType GetPageType() const { return GetPageType(browser()); }

  // Sets whether the extension should have a background script.
  void set_has_background_script(bool has_background_script) {
    has_background_script_ = has_background_script;
  }

  // Loads an extension with the given declarative |rules| in the given
  // |directory|. Generates a fatal failure if the extension failed to load.
  // |hosts| specifies the host permissions, the extensions should
  // have.
  void LoadExtensionWithRules(const std::vector<TestRule>& rules,
                              const std::string& directory,
                              const std::vector<std::string>& hosts) {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    base::HistogramTester tester;

    base::FilePath extension_dir = temp_dir_.GetPath().AppendASCII(directory);
    EXPECT_TRUE(base::CreateDirectory(extension_dir));

    WriteManifestAndRuleset(extension_dir, kJSONRulesetFilepath,
                            kJSONRulesFilename, rules, hosts,
                            has_background_script_);

    background_page_ready_listener_->Reset();
    const Extension* extension = nullptr;
    switch (GetParam()) {
      case ExtensionLoadType::PACKED:
        extension = InstallExtensionWithPermissionsGranted(
            extension_dir, 1 /* expected_change */);
        break;
      case ExtensionLoadType::UNPACKED:
        extension = LoadExtension(extension_dir);
        break;
    }

    ASSERT_TRUE(extension);

    // Ensure the ruleset is also loaded on the IO thread.
    content::RunAllTasksUntilIdle();

    tester.ExpectTotalCount(kIndexRulesTimeHistogram, 1);
    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram, 1);
    tester.ExpectUniqueSample(kManifestRulesCountHistogram,
                              rules.size() /*sample*/, 1 /*count*/);
    tester.ExpectTotalCount(
        "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime", 1);
    tester.ExpectUniqueSample(
        "Extensions.DeclarativeNetRequest.LoadRulesetResult",
        RulesetMatcher::kLoadSuccess /*sample*/, 1 /*count*/);

    EXPECT_TRUE(HasValidIndexedRuleset(*extension, profile()));

    // Wait for the background page to load if needed.
    if (has_background_script_)
      WaitForBackgroundScriptToLoad(extension->id());

    // Ensure no load errors were reported.
    EXPECT_TRUE(LoadErrorReporter::GetInstance()->GetErrors()->empty());
  }

  void LoadExtensionWithRules(const std::vector<TestRule>& rules) {
    LoadExtensionWithRules(rules, "test_extension", {} /* hosts */);
  }

  void WaitForBackgroundScriptToLoad(const ExtensionId& extension_id) {
    ASSERT_TRUE(background_page_ready_listener_->WaitUntilSatisfied());
    ASSERT_EQ(extension_id,
              background_page_ready_listener_->extension_id_for_message());
    background_page_ready_listener_->Reset();
  }

  void AddAllowedPages(const ExtensionId& extension_id,
                       const std::vector<std::string>& patterns) {
    UpdateAllowedPages(extension_id, patterns, "addAllowedPages");
  }

  void RemoveAllowedPages(const ExtensionId& extension_id,
                          const std::vector<std::string>& patterns) {
    UpdateAllowedPages(extension_id, patterns, "removeAllowedPages");
  }

  // Verifies that the result of getAllowedPages call is the same as
  // |expected_patterns|.
  void VerifyGetAllowedPages(const ExtensionId& extension_id,
                             const std::set<std::string>& expected_patterns) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.getAllowedPages(function(patterns) {
        window.domAutomationController.send(chrome.runtime.lastError
                                                ? 'error'
                                                : JSON.stringify(patterns));
      });
    )";

    const std::string result =
        ExecuteScriptInBackgroundPage(extension_id, kScript);
    ASSERT_NE("error", result);

    // Parse |result| as a list and deserialize it to a set of strings.
    std::unique_ptr<base::Value> value =
        JSONStringValueDeserializer(result).Deserialize(
            nullptr /*error_code*/, nullptr /*error_message*/);
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->is_list());
    std::set<std::string> patterns;
    for (const auto& pattern_value : value->GetList()) {
      ASSERT_TRUE(pattern_value.is_string());
      patterns.insert(pattern_value.GetString());
    }

    EXPECT_EQ(expected_patterns, patterns);
  }

  std::set<GURL> GetAndResetRequestsToServer() {
    base::AutoLock lock(requests_to_server_lock_);
    std::set<GURL> results = requests_to_server_;
    requests_to_server_.clear();
    return results;
  }

 private:
  // Handler to monitor the requests which reach the EmbeddedTestServer. This
  // will be run on the EmbeddedTestServer's IO thread.
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock lock(requests_to_server_lock_);
    requests_to_server_.insert(request.GetURL());
  }

  void UpdateAllowedPages(const ExtensionId& extension_id,
                          const std::vector<std::string>& patterns,
                          const std::string& function_name) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.%s(%s, function() {
        window.domAutomationController.send(chrome.runtime.lastError
                                                ? 'error'
                                                : 'success');
      });
    )";

    // Serialize |patterns| to JSON.
    std::unique_ptr<base::ListValue> list = ToListValue(patterns);
    std::string json_string;
    ASSERT_TRUE(JSONStringValueSerializer(&json_string).Serialize(*list));

    EXPECT_EQ("success", ExecuteScriptInBackgroundPage(
                             extension_id,
                             base::StringPrintf(kScript, function_name.c_str(),
                                                json_string.c_str())));
  }

  base::ScopedTempDir temp_dir_;
  bool has_background_script_ = false;
  std::unique_ptr<ExtensionTestMessageListener> background_page_ready_listener_;

  // Requests observed by the EmbeddedTestServer. This is accessed on both the
  // UI and the EmbeddedTestServer's IO thread. Access is protected by
  // |requests_to_server_lock_|.
  std::set<GURL> requests_to_server_;

  base::Lock requests_to_server_lock_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestBrowserTest);
};

using DeclarativeNetRequestBrowserTest_Packed =
    DeclarativeNetRequestBrowserTest;

// Tests the "urlFilter" property of a declarative rule condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_UrlFilter) {
  struct {
    std::string url_filter;
    int id;
  } rules_data[] = {
      {"pages_with_script/*ex", 1},
      {"||a.b.com", 2},
      {"|http://*.us", 3},
      {"pages_with_script/page2.html|", 4},
      {"|http://msn*/pages_with_script/page.html|", 5},
      {"%20", 6},     // Block any urls with space.
      {"%C3%A9", 7},  // Percent-encoded non-ascii character é.
      // Internationalized domain "ⱴase.com" in punycode.
      {"|http://xn--ase-7z0b.com", 8},
  };

  // Rule |i| is the rule with id |i|.
  struct {
    std::string hostname;
    std::string path;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"example.com", "/pages_with_script/index.html", false},  // Rule 1
      {"example.com", "/pages_with_script/page.html", true},
      {"a.b.com", "/pages_with_script/page.html", false},    // Rule 2
      {"c.a.b.com", "/pages_with_script/page.html", false},  // Rule 2
      {"b.com", "/pages_with_script/page.html", true},
      {"example.us", "/pages_with_script/page.html", false},  // Rule 3
      {"example.jp", "/pages_with_script/page.html", true},
      {"example.jp", "/pages_with_script/page2.html", false},  // Rule 4
      {"example.jp", "/pages_with_script/page2.html?q=hello", true},
      {"msn.com", "/pages_with_script/page.html", false},  // Rule 5
      {"msn.com", "/pages_with_script/page.html?q=hello", true},
      {"a.msn.com", "/pages_with_script/page.html", true},
      {"abc.com", "/pages_with_script/page.html?q=hi bye", false},    // Rule 6
      {"abc.com", "/pages_with_script/page.html?q=hi%20bye", false},  // Rule 6
      {"abc.com", "/pages_with_script/page.html?q=hibye", true},
      {"abc.com",
       "/pages_with_script/page.html?q=" + base::WideToUTF8(L"\u00E9"),
       false},  // Rule 7
      {base::WideToUTF8(L"\x2c74"
                        L"ase.com"),
       "/pages_with_script/page.html", false},  // Rule 8
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.id = rule_data.id;
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Verify that the extension correctly intercepts network requests.
  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests the matching behavior of the separator ('^') character.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_Separator) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter =
      std::string("pages_with_script/page2.html?q=bye^");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // '^' (Separator character) matches anything except a letter, a digit or
  // one of the following: _ - . %.
  struct {
    std::string url_path;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"/pages_with_script/page2.html?q=bye&x=1", false},
      {"/pages_with_script/page2.html?q=bye#x=1", false},
      {"/pages_with_script/page2.html?q=bye:", false},
      {"/pages_with_script/page2.html?q=bye3", true},
      {"/pages_with_script/page2.html?q=byea", true},
      {"/pages_with_script/page2.html?q=bye_", true},
      {"/pages_with_script/page2.html?q=bye-", true},
      {"/pages_with_script/page2.html?q=bye%", true},
      {"/pages_with_script/page2.html?q=bye.", true},
  };

  for (const auto& test_case : test_cases) {
    GURL url = embedded_test_server()->GetURL("google.com", test_case.url_path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests that the separator character '^' correctly matches the end of the url.
// TODO(crbug.com/772260): Enable once the bug is fixed.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       DISABLED_BlockRequests_SeparatorMatchesEndOfURL) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("page2.html^");

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  GURL url = embedded_test_server()->GetURL("google.com",
                                            "/pages_with_script/page2.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());
}

// Tests case sensitive url filters.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_IsUrlFilterCaseSensitive) {
  struct {
    std::string url_filter;
    size_t id;
    bool is_url_filter_case_sensitive;
  } rules_data[] = {{"pages_with_script/index.html?q=hello", 1, false},
                    {"pages_with_script/page.html?q=hello", 2, true}};

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.condition->is_url_filter_case_sensitive =
        rule_data.is_url_filter_case_sensitive;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Rule |i| is the rule with id |i|.
  struct {
    std::string hostname;
    std::string path;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"example.com", "/pages_with_script/index.html?q=hello",
       false},  // Rule 1
      {"example.com", "/pages_with_script/index.html?q=HELLO",
       false},                                                         // Rule 1
      {"example.com", "/pages_with_script/page.html?q=hello", false},  // Rule 2
      {"example.com", "/pages_with_script/page.html?q=HELLO", true},
  };

  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests the "domains" and "excludedDomains" property of a declarative rule
// condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_Domains) {
  struct {
    std::string url_filter;
    size_t id;
    std::vector<std::string> domains;
    std::vector<std::string> excluded_domains;
  } rules_data[] = {{"child_frame.html?frame=1",
                     1,
                     {"x.com", "xn--36c-tfa.com" /* punycode for 36°c.com */},
                     {"a.x.com"}},
                    {"child_frame.html?frame=2", 2, {}, {"a.y.com"}}};

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;

    // An empty list is not allowed for the "domains" property.
    if (!rule_data.domains.empty())
      rule.condition->domains = rule_data.domains;

    rule.condition->excluded_domains = rule_data.excluded_domains;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Rule |i| is the rule with id |i|.
  struct {
    std::string main_frame_hostname;
    bool expect_frame_1_loaded;
    bool expect_frame_2_loaded;
  } test_cases[] = {
      {"x.com", false /* Rule 1 */, false /* Rule 2 */},
      {base::WideToUTF8(L"36\x00b0"
                        L"c.com" /* 36°c.com */),
       false /*Rule 1*/, false /*Rule 2*/},
      {"b.x.com", false /* Rule 1 */, false /* Rule 2 */},
      {"a.x.com", true, false /* Rule 2 */},
      {"b.a.x.com", true, false /* Rule 2 */},
      {"y.com", true, false /* Rule 2*/},
      {"a.y.com", true, true},
      {"b.a.y.com", true, true},
      {"google.com", true, false /* Rule 2 */},
  };

  for (const auto& test_case : test_cases) {
    GURL url = embedded_test_server()->GetURL(test_case.main_frame_hostname,
                                              "/page_with_two_frames.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    content::RenderFrameHost* main_frame = GetMainFrame();
    EXPECT_TRUE(WasFrameWithScriptLoaded(main_frame));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

    content::RenderFrameHost* child_1 = content::ChildFrameAt(main_frame, 0);
    content::RenderFrameHost* child_2 = content::ChildFrameAt(main_frame, 1);

    EXPECT_EQ(test_case.expect_frame_1_loaded,
              WasFrameWithScriptLoaded(child_1));
    EXPECT_EQ(test_case.expect_frame_2_loaded,
              WasFrameWithScriptLoaded(child_2));
  }
}

// Tests the "domainType" property of a declarative rule condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_DomainType) {
  struct {
    std::string url_filter;
    size_t id;
    base::Optional<std::string> domain_type;
  } rules_data[] = {
      {"child_frame.html?case=1", 1, std::string("firstParty")},
      {"child_frame.html?case=2", 2, std::string("thirdParty")},
      {"child_frame.html?case=3", 3, base::nullopt},
  };

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.condition->domain_type = rule_data.domain_type;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  GURL url =
      embedded_test_server()->GetURL("example.com", "/domain_type_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  ASSERT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

  // The loaded page will consist of four pairs of iframes named
  // first_party_[1..4] and third_party_[1..4], with url path
  // child_frame.html?case=[1..4] respectively.
  struct {
    std::string child_frame_name;
    bool expect_frame_loaded;
  } cases[] = {
      // Url matched by rule 1. Only the first party frame is blocked.
      {"first_party_1", false},
      {"third_party_1", true},
      // Url matched by rule 2. Only the third party frame is blocked.
      {"first_party_2", true},
      {"third_party_2", false},
      // Url matched by rule 3. Both the first and third party frames are
      // blocked.
      {"first_party_3", false},
      {"third_party_3", false},
      // No matching rule.
      {"first_party_4", true},
      {"third_party_4", true},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf("Testing child frame named %s",
                                    test_case.child_frame_name.c_str()));

    content::RenderFrameHost* child =
        GetFrameByName(test_case.child_frame_name);
    EXPECT_TRUE(child);
    EXPECT_EQ(test_case.expect_frame_loaded, WasFrameWithScriptLoaded(child));
  }
}

// Tests allowing rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, Allow) {
  const int kNumRequests = 6;

  TestRule rule = CreateGenericRule();
  int id = kMinValidID;

  // Block all main-frame requests ending with numbers 1 to |kNumRequests|.
  std::vector<TestRule> rules;
  for (int i = 1; i <= kNumRequests; ++i) {
    rule.id = id++;
    rule.condition->url_filter = base::StringPrintf("num=%d|", i);
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rules.push_back(rule);
  }

  // Allow all main-frame requests ending with even numbers from 1 to
  // |kNumRequests|.
  for (int i = 2; i <= kNumRequests; i += 2) {
    rule.id = id++;
    rule.condition->url_filter = base::StringPrintf("num=%d|", i);
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = std::string("allow");
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  for (int i = 1; i <= kNumRequests; ++i) {
    GURL url = embedded_test_server()->GetURL(
        "example.com",
        base::StringPrintf("/pages_with_script/page.html?num=%d", i));
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);

    // All requests ending with odd numbers should be blocked.
    const bool page_should_load = (i % 2 == 0);
    EXPECT_EQ(page_should_load, WasFrameWithScriptLoaded(GetMainFrame()));
    content::PageType expected_page_type =
        page_should_load ? content::PAGE_TYPE_NORMAL : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests that the extension ruleset is active only when the extension is
// enabled.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       Enable_Disable_Reload_Uninstall) {
  // Block all main frame requests to example.com
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  const ExtensionId extension_id = last_loaded_extension_id();

  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  auto test_extension_enabled = [&](bool expected_enabled) {
    // Wait for any pending actions caused by extension state change.
    content::RunAllTasksUntilIdle();
    EXPECT_EQ(expected_enabled,
              ExtensionRegistry::Get(profile())->enabled_extensions().Contains(
                  extension_id));

    ui_test_utils::NavigateToURL(browser(), url);

    // If the extension is enabled, the |url| should be blocked.
    EXPECT_EQ(!expected_enabled, WasFrameWithScriptLoaded(GetMainFrame()));
    content::PageType expected_page_type =
        expected_enabled ? content::PAGE_TYPE_ERROR : content::PAGE_TYPE_NORMAL;
    EXPECT_EQ(expected_page_type, GetPageType());
  };

  {
    SCOPED_TRACE("Testing extension after load");
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing DisableExtension");
    DisableExtension(extension_id);
    test_extension_enabled(false);
  }

  {
    SCOPED_TRACE("Testing EnableExtension");
    EnableExtension(extension_id);
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing ReloadExtension");
    ReloadExtension(extension_id);
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing UninstallExtension");
    UninstallExtension(extension_id);
    test_extension_enabled(false);
  }
}

// Tests that multiple enabled extensions with declarative rulesets having
// blocking rules behave correctly.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_MultipleExtensions) {
  struct {
    std::string url_filter;
    int id;
    bool add_to_first_extension;
    bool add_to_second_extension;
  } rules_data[] = {
      {"block_both.com", 1, true, true},
      {"block_first.com", 2, true, false},
      {"block_second.com", 3, false, true},
  };

  std::vector<TestRule> rules_1, rules_2;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.id = rule_data.id;
    if (rule_data.add_to_first_extension)
      rules_1.push_back(rule);
    if (rule_data.add_to_second_extension)
      rules_2.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules_1, "extension_1", {URLPattern::kAllUrlsPattern}));
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules_2, "extension_2", {URLPattern::kAllUrlsPattern}));

  struct {
    std::string host;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"block_both.com", false},
      {"block_first.com", false},
      {"block_second.com", false},
      {"block_none.com", true},
  };

  for (const auto& test_case : test_cases) {
    GURL url = embedded_test_server()->GetURL(test_case.host,
                                              "/pages_with_script/page.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));
    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests that multiple enabled extensions with declarative rulesets having
// redirect rules behave correctly with preference given to more recently
// installed extensions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       RedirectRequests_MultipleExtensions) {
  const int kNumExtensions = 4;

  auto redirect_url_for_extension_number = [this](size_t num) {
    return embedded_test_server()
        ->GetURL(std::to_string(num) + ".com", "/pages_with_script/index.html")
        .spec();
  };

  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.condition->url_filter = std::string("example.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});

  base::Time last_extension_install_time = base::Time::Min();

  // Add |kNumExtensions| each redirecting example.com to a different redirect
  // url.
  for (size_t i = 1; i <= kNumExtensions; ++i) {
    rule.action->redirect_url = redirect_url_for_extension_number(i);
    ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
        {rule}, std::to_string(i), {URLPattern::kAllUrlsPattern}));

    // Verify that the install time of this extension is greater than the last
    // extension.
    base::Time install_time = ExtensionPrefs::Get(profile())->GetInstallTime(
        last_loaded_extension_id());
    EXPECT_GT(install_time, last_extension_install_time);
    last_extension_install_time = install_time;
  }

  // Load a url on example.com. It should be redirected to the redirect url
  // corresponding to the most recently installed extension.
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  GURL final_url = web_contents()->GetLastCommittedURL();
  EXPECT_EQ(GURL(redirect_url_for_extension_number(kNumExtensions)), final_url);
}

// Tests a combination of blocking and redirect rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, BlockAndRedirect) {
  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()
        ->GetURL(hostname, "/pages_with_script/index.html")
        .spec();
  };

  struct {
    std::string url_filter;
    int id;
    std::string action_type;
    base::Optional<std::string> redirect_url;
  } rules_data[] = {
      {"example.com", 1, "redirect", get_url_for_host("yahoo.com")},
      {"yahoo.com", 2, "redirect", get_url_for_host("google.com")},
      {"abc.com", 3, "redirect", get_url_for_host("def.com")},
      {"def.com", 4, "block", base::nullopt},
      {"def.com", 5, "redirect", get_url_for_host("xyz.com")},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.priority = kMinValidPriority;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = rule_data.action_type;
    rule.action->redirect_url = rule_data.redirect_url;
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  struct {
    std::string hostname;
    bool expected_main_frame_loaded;
    base::Optional<std::string> expected_final_hostname;
    base::Optional<size_t> expected_redirect_chain_length;
  } test_cases[] = {
      // example.com -> yahoo.com -> google.com.
      {"example.com", true, std::string("google.com"), 3},
      // yahoo.com -> google.com.
      {"yahoo.com", true, std::string("google.com"), 2},
      // abc.com -> def.com (blocked).
      // Note def.com won't be redirected since blocking rules are given
      // priority over redirect rules.
      {"abc.com", false, base::nullopt, base::nullopt},
      // def.com (blocked).
      {"def.com", false, base::nullopt, base::nullopt},
  };

  for (const auto& test_case : test_cases) {
    std::string url = get_url_for_host(test_case.hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.c_str()));

    ui_test_utils::NavigateToURL(browser(), GURL(url));

    if (!test_case.expected_main_frame_loaded) {
      EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
      EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());
    } else {
      EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
      EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

      GURL final_url = web_contents()->GetLastCommittedURL();
      EXPECT_EQ(GURL(get_url_for_host(*test_case.expected_final_hostname)),
                final_url);

      EXPECT_EQ(*test_case.expected_redirect_chain_length,
                web_contents()
                    ->GetController()
                    .GetLastCommittedEntry()
                    ->GetRedirectChain()
                    .size());
    }
  }
}

// Tests that the redirect url within an extension ruleset is chosen based on
// the highest priority matching rule.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, RedirectPriority) {
  const size_t kNumPatternTypes = 7;

  auto hostname_for_number = [](size_t num) {
    return std::to_string(num) + ".com";
  };

  auto redirect_url_for_priority = [this,
                                    hostname_for_number](size_t priority) {
    return embedded_test_server()
        ->GetURL(hostname_for_number(priority + 1),
                 "/pages_with_script/index.html")
        .spec();
  };

  std::vector<TestRule> rules;
  TestRule rule = CreateGenericRule();
  int id = kMinValidID;
  for (size_t i = 1; i <= kNumPatternTypes; i++) {
    // For pattern type |i|, add |i| rules with priority from 1 to |i|, each
    // with a different redirect url.
    std::string pattern = hostname_for_number(i) + "*page.html";

    for (size_t j = 1; j <= i; j++) {
      rule.id = id++;
      rule.priority = j;
      rule.action->type = std::string("redirect");
      rule.action->redirect_url = redirect_url_for_priority(j);
      rule.condition->url_filter = pattern;
      rule.condition->resource_types = std::vector<std::string>({"main_frame"});
      rules.push_back(rule);
    }
  }

  // Shuffle the rules to ensure that the order in which rules are added has no
  // effect on the test.
  base::RandomShuffle(rules.begin(), rules.end());
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  for (size_t i = 0; i <= kNumPatternTypes + 1; ++i) {
    GURL url = embedded_test_server()->GetURL(hostname_for_number(i),
                                              "/pages_with_script/page.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
    GURL final_url = web_contents()->GetLastCommittedURL();

    bool expect_redirection = i >= 1 && i <= kNumPatternTypes;
    if (expect_redirection) {
      // For pattern type |i|, the highest prioirity for any rule was |i|.
      EXPECT_EQ(GURL(redirect_url_for_priority(i)), final_url);
    } else {
      EXPECT_EQ(url, final_url);
    }
  }
}

// Tests that only extensions enabled in incognito mode affect network requests
// from an incognito context.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_Incognito) {
  // Block all main-frame requests to example.com.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  ExtensionId extension_id = last_loaded_extension_id();
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  Browser* incognito_browser = CreateIncognitoBrowser();

  auto test_enabled_in_incognito = [&](bool expected_enabled_in_incognito) {
    // Wait for any pending actions caused by extension state change.
    content::RunAllTasksUntilIdle();

    EXPECT_EQ(expected_enabled_in_incognito,
              util::IsIncognitoEnabled(extension_id, profile()));

    // The url should be blocked in normal context.
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
    EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());

    // In incognito context, the url should be blocked if the extension is
    // enabled in incognito mode.
    ui_test_utils::NavigateToURL(incognito_browser, url);
    EXPECT_EQ(!expected_enabled_in_incognito,
              WasFrameWithScriptLoaded(GetMainFrame(incognito_browser)));
    content::PageType expected_page_type = expected_enabled_in_incognito
                                               ? content::PAGE_TYPE_ERROR
                                               : content::PAGE_TYPE_NORMAL;
    EXPECT_EQ(expected_page_type, GetPageType(incognito_browser));
  };

  {
    // By default, the extension will be disabled in incognito.
    SCOPED_TRACE("Testing extension after load");
    test_enabled_in_incognito(false);
  }

  {
    // Enable the extension in incognito mode.
    SCOPED_TRACE("Testing extension after enabling it in incognito");
    util::SetIsIncognitoEnabled(extension_id, profile(), true /*enabled*/);
    test_enabled_in_incognito(true);
  }

  // Disable the extension in incognito mode.
  {
    SCOPED_TRACE("Testing extension after disabling it in incognito");
    util::SetIsIncognitoEnabled(extension_id, profile(), false /*enabled*/);
    test_enabled_in_incognito(false);
  }
}

// Ensure extensions can't intercept chrome:// urls, even after explicitly
// requesting access to <all_urls>.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, ChromeURLS) {
  // Have the extension block all chrome:// urls.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("chrome://");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  std::vector<const char*> test_urls = {
      chrome::kChromeUIFlagsURL, chrome::kChromeUIAboutURL,
      chrome::kChromeUIExtensionsURL, chrome::kChromeUIVersionURL};

  for (const char* url : test_urls) {
    ui_test_utils::NavigateToURL(browser(), GURL(url));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  }
}

// Test that a packed extension with a DNR ruleset behaves correctly after
// browser restart.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       PRE_BrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetParam());

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       BrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetParam());

  // Ensure that the DNR extension enabled in previous browser session still
  // correctly blocks network requests.
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
}

// Ensure that Blink's in-memory cache is cleared on adding/removing rulesets.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, RendererCacheCleared) {
  // Set-up an observer for RulesetMatcher to monitor requests to
  // script.js.
  URLRequestMonitor script_monitor(
      embedded_test_server()->GetURL("example.com", "/cached/script.js"));
  ScopedRulesetManagerTestObserver scoped_observer(
      &script_monitor,
      base::WrapRefCounted(ExtensionSystem::Get(profile())->info_map()));

  GURL url = embedded_test_server()->GetURL(
      "example.com", "/cached/page_with_cacheable_script.html");

  // With no extension loaded, the request to the script should succeed.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

  // NOTE: When the Network Service is enabled, the RulesetMatcher will not see
  // network requests if no rulesets are active.
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      script_monitor.GetAndResetRequestSeen(false));

  // Another request to |url| should not cause a network request for
  // script.js since it will be served by the renderer's in-memory
  // cache.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_FALSE(script_monitor.GetAndResetRequestSeen(false));

  // Now block requests to script.js.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("script.js");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Adding an extension ruleset should have cleared the renderer's in-memory
  // cache. Hence the browser process will observe the request to
  // script.js and block it.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_TRUE(script_monitor.GetAndResetRequestSeen(false));

  // Disable the extension and wait for the ruleset to be unloaded on the IO
  // thread.
  DisableExtension(last_loaded_extension_id());
  content::RunAllTasksUntilIdle();

  // Disabling the extension should cause the request to succeed again. The
  // request for the script will again be observed by the browser since it's not
  // in the renderer's in-memory cache.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      script_monitor.GetAndResetRequestSeen(false));
}

// Tests that proxy requests aren't intercepted. See https://crbug.com/794674.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       PacRequestsBypassRules) {
  // Load the extension.
  std::vector<TestRule> rules;
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*pac");
  rule.id = 1;
  rules.push_back(rule);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Configure a PAC script. Need to do this after the extension is loaded, so
  // that the PAC isn't already loaded by the time the extension starts
  // affecting requests.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->Set(proxy_config::prefs::kProxy,
                    ProxyConfigDictionary::CreatePacScript(
                        embedded_test_server()->GetURL("/self.pac").spec(),
                        true /* pac_mandatory */));
  // Flush the proxy configuration change over the Mojo pipe to avoid any races.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->FlushProxyConfigMonitorForTesting();

  // Verify that the extension can't intercept the network request.
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/pages_with_script/page.html"));
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
}

// Ensure that an extension can't intercept requests on the chrome-extension
// scheme.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       InterceptExtensionScheme) {
  // Load two extensions. One blocks all urls, and the other blocks urls with
  // "google.com" as a substring.
  std::vector<TestRule> rules_1;
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  rules_1.push_back(rule);

  std::vector<TestRule> rules_2;
  rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rules_2.push_back(rule);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules_1, "extension_1", {URLPattern::kAllUrlsPattern}));
  const std::string extension_id_1 = last_loaded_extension_id();

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules_2, "extension_2", {URLPattern::kAllUrlsPattern}));
  const std::string extension_id_2 = last_loaded_extension_id();

  auto get_manifest_url = [](const ExtensionId& extension_id) {
    return GURL(base::StringPrintf("%s://%s/manifest.json",
                                   extensions::kExtensionScheme,
                                   extension_id.c_str()));
  };

  // Extension 1 should not be able to block the request to its own
  // manifest.json or that of the Extension 2, even with "<all_urls>" host
  // permissions.
  ui_test_utils::NavigateToURL(browser(), get_manifest_url(extension_id_1));
  GURL final_url = web_contents()->GetLastCommittedURL();
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  ui_test_utils::NavigateToURL(browser(), get_manifest_url(extension_id_2));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
}

// Tests the page allowing API.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       PRE_PageAllowingAPI_PersistedAcrossSessions) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser sessions.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetParam());

  set_has_background_script(true);
  LoadExtensionWithRules({});

  const Extension* dnr_extension = extension_service()->GetExtensionById(
      last_loaded_extension_id(), false /*include_disabled*/);
  ASSERT_TRUE(dnr_extension);
  EXPECT_EQ("Test extension", dnr_extension->name());

  constexpr char kGoogleDotCom[] = "https://www.google.com/";

  // Allow |kGoogleDotCom|.
  AddAllowedPages(dnr_extension->id(), {kGoogleDotCom});

  // Ensure that the page was allowed.
  VerifyGetAllowedPages(dnr_extension->id(), {kGoogleDotCom});
}

// Tests that the pages allowed using the page allowing API are
// persisted across browser sessions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       PageAllowingAPI_PersistedAcrossSessions) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser sessions.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetParam());

  // Retrieve the extension installed in the previous browser session.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  const Extension* dnr_extension = nullptr;
  for (const scoped_refptr<const Extension>& extension :
       registry->enabled_extensions()) {
    if (extension->name() == "Test extension") {
      dnr_extension = extension.get();
      break;
    }
  }
  ASSERT_TRUE(dnr_extension);

  // Ensure the background page is ready before dispatching the script to it.
  WaitForBackgroundScriptToLoad(dnr_extension->id());

  constexpr char kGoogleDotCom[] = "https://www.google.com/";

  VerifyGetAllowedPages(dnr_extension->id(), {kGoogleDotCom});

  // Remove |kGoogleDotCom| from the set of allowed pages.
  RemoveAllowedPages(dnr_extension->id(), {kGoogleDotCom});

  VerifyGetAllowedPages(dnr_extension->id(), {});
}

// Tests that the page allowing API performs pattern matching correctly.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       PageAllowingAPI_Patterns) {
  // Load an extension which blocks requests to "script.js".
  set_has_background_script(true);
  std::vector<TestRule> rules;
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("script.js");
  rules.push_back(rule);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // We'll be allowing these patterns subsequently.
  const std::vector<std::string> allowed_page_patterns = {
      "http://google.com:*/pages_with_script/page*.html",
      "http://*/*index.html",
      "http://example.com:*/pages_with_script/page2.html"};

  struct TestCase {
    std::string hostname;
    std::string path;
    bool expect_script_allowed_with_rules;
  } test_cases[] = {
      {"yahoo.com", "/pages_with_script/page.html", false},
      {"yahoo.com", "/pages_with_script/index.html", true},
      {"example.com", "/pages_with_script/page.html", false},
      {"example.com", "/pages_with_script/page2.html", true},
      {"google.com", "/pages_with_script/page.html", true},
      {"google.com", "/pages_with_script/page2.html", true},
  };

  const GURL script_url =
      embedded_test_server()->GetURL("/pages_with_script/script.js");
  auto verify_script_load = [this, script_url](bool expect_script_load) {
    // The page should have loaded correctly.
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    EXPECT_EQ(expect_script_load, WasFrameWithScriptLoaded(GetMainFrame()));

    // The EmbeddedTestServer sees requests after the hostname has been
    // resolved.
    bool did_see_script_request =
        base::ContainsKey(GetAndResetRequestsToServer(), script_url);
    EXPECT_EQ(expect_script_load, did_see_script_request);
  };

  // "script.js" should have been blocked for all pages initially.
  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s with no page allowing rules",
                                    url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);

    verify_script_load(false);
  }

  // Now allow |allowed_page_patterns| and ensure the test expectations
  // are met.
  AddAllowedPages(last_loaded_extension_id(), allowed_page_patterns);
  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s with page allowing rules",
                                    url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);

    verify_script_load(test_case.expect_script_allowed_with_rules);
  }

  // Now remove the |allowed_page_patterns| and ensure that all requests to
  // "script.js" are blocked again.
  RemoveAllowedPages(last_loaded_extension_id(), allowed_page_patterns);
  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf(
        "Testing %s with all page allowing rules removed", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);

    verify_script_load(false);
  }
}

// Tests the page allowing API for subresource requests.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       PageAllowingAPI_Resources) {
  // Load an extension which blocks all requests.
  set_has_background_script(true);
  std::vector<TestRule> rules;

  // This blocks all subresource requests. By default the main-frame resource
  // type is excluded.
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.condition->url_filter = std::string("*");
  rules.push_back(rule);

  // Also block main frame requests.
  rule.id = kMinValidID + 1;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rules.push_back(rule);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  const GURL url = embedded_test_server()->GetURL("/allowing_api.html");

  auto verify_page_load = [this](bool expect_load) {
    EXPECT_EQ(expect_load, WasFrameWithScriptLoaded(GetMainFrame()));
    EXPECT_EQ(
        expect_load ? content::PAGE_TYPE_NORMAL : content::PAGE_TYPE_ERROR,
        GetPageType());

    // We will be loading "allowing_api.html" for the test. The following
    // lists the set of requests we expect to see if the page is loaded.
    static const std::vector<std::string> page_resources_path = {
        // Main frame request.
        "/allowing_api.html",

        // Main frame subrources.
        "/subresources/style.css", "/subresources/script.js",
        "/subresources/image.png", "/subresources/ping.mp3",
        "/child_frame_with_subresources.html",

        // Child frame subresources.
        "/iframe_subresources/style.css", "/iframe_subresources/script.js",
        "/iframe_subresources/image.png", "/iframe_subresources/ping.mp3",
        "/iframe_subresources/subframe.html",
    };

    const std::set<GURL> requests_seen = GetAndResetRequestsToServer();

    for (const auto& path : page_resources_path) {
      GURL expected_request_url = embedded_test_server()->GetURL(path);

      // Request to |expected_requested_url| should be seen by the server iff we
      // expect the page to load.
      if (expect_load) {
        EXPECT_TRUE(base::ContainsKey(requests_seen, expected_request_url))
            << expected_request_url.spec()
            << " was not requested from the server.";
      } else {
        EXPECT_FALSE(base::ContainsKey(requests_seen, expected_request_url))
            << expected_request_url.spec() << " request seen unexpectedly.";
      }
    }
  };

  // Initially the request for the page should be blocked. No request should
  // reach the server.
  ui_test_utils::NavigateToURL(browser(), url);
  verify_page_load(false /*expect_load*/);

  // Next we allow |url|. All requests with |url| as the top level frame
  // should be allowed.
  AddAllowedPages(last_loaded_extension_id(), {url.spec()});

  // Ensure that no requests made by the page load are blocked.
  ui_test_utils::NavigateToURL(browser(), url);
  verify_page_load(true /*expect_load*/);

  // Remove |url| from allowed pages.
  RemoveAllowedPages(last_loaded_extension_id(), {url.spec()});

  // Ensure that requests are blocked.
  ui_test_utils::NavigateToURL(browser(), url);
  verify_page_load(false /*expect_load*/);
}

// Ensures that any <img> elements blocked by the API are collapsed.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, ImageCollapsed) {
  // Loads a page with an image and returns whether the image was collapsed.
  auto is_image_collapsed = [this]() {
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("google.com", "/image.html"));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    bool is_image_collapsed = false;
    const std::string script =
        "domAutomationController.send(!!window.imageCollapsed);";
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents(), script,
                                                     &is_image_collapsed));
    return is_image_collapsed;
  };

  // Initially the image shouldn't be collapsed.
  EXPECT_FALSE(is_image_collapsed());

  // Load an extension which blocks all images.
  std::vector<TestRule> rules;
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  rule.condition->resource_types = std::vector<std::string>({"image"});
  rules.push_back(rule);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Now the image request should be blocked and the corresponding DOM element
  // should be collapsed.
  EXPECT_TRUE(is_image_collapsed());
}

// Ensures that any <iframe> elements whose document load is blocked by the API,
// are collapsed.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, IFrameCollapsed) {
  // Verifies whether the frame with name |frame_name| is collapsed.
  auto test_frame_collapse = [this](const std::string& frame_name,
                                    bool expect_collapsed) {
    SCOPED_TRACE(base::StringPrintf("Testing frame %s", frame_name.c_str()));
    content::RenderFrameHost* frame = GetFrameByName(frame_name);
    ASSERT_TRUE(frame);

    EXPECT_EQ(!expect_collapsed, WasFrameWithScriptLoaded(frame));

    constexpr char kScript[] = R"(
        var iframe = document.getElementsByName('%s')[0];
        var collapsed = iframe.clientWidth === 0 && iframe.clientHeight === 0;
        domAutomationController.send(collapsed);
    )";
    bool collapsed = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetMainFrame(), base::StringPrintf(kScript, frame_name.c_str()),
        &collapsed));
    EXPECT_EQ(expect_collapsed, collapsed);
  };

  // Navigates frame with name |frame_name| to |url|.
  auto navigate_frame = [this](const std::string& frame_name, const GURL& url) {
    content::TestNavigationObserver navigation_observer(
        web_contents(), 1 /*number_of_navigations*/);
    ASSERT_TRUE(content::ExecuteScript(
        GetMainFrame(),
        base::StringPrintf("document.getElementsByName('%s')[0].src = '%s';",
                           frame_name.c_str(), url.spec().c_str())));
    navigation_observer.Wait();
  };

  const std::string kFrameName1 = "frame1";
  const std::string kFrameName2 = "frame2";
  const GURL page_url = embedded_test_server()->GetURL(
      "google.com", "/page_with_two_frames.html");

  // Load a page with two iframes (|kFrameName1| and |kFrameName2|). Initially
  // both the frames should be loaded successfully.
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  {
    SCOPED_TRACE("No extension loaded");
    test_frame_collapse(kFrameName1, false);
    test_frame_collapse(kFrameName2, false);
  }

  // Now load an extension which blocks all requests with "frame=1" in its url.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("frame=1");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Reloading the page should cause |kFrameName1| to be collapsed.
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  {
    SCOPED_TRACE("Extension loaded initial");
    test_frame_collapse(kFrameName1, true);
    test_frame_collapse(kFrameName2, false);
  }

  // Now interchange the "src" of the two frames. This should cause
  // |kFrameName2| to be collapsed and |kFrameName1| to be un-collapsed.
  GURL frame_url_1 = GetFrameByName(kFrameName1)->GetLastCommittedURL();
  GURL frame_url_2 = GetFrameByName(kFrameName2)->GetLastCommittedURL();
  navigate_frame(kFrameName1, frame_url_2);
  navigate_frame(kFrameName2, frame_url_1);
  {
    SCOPED_TRACE("Extension loaded src swapped");
    test_frame_collapse(kFrameName1, false);
    test_frame_collapse(kFrameName2, true);
  }
}

// Tests that we correctly reindex a corrupted ruleset. This is only tested for
// packed extensions, since the JSON ruleset is reindexed on each extension
// load for unpacked extensions, so corruption is not an issue.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       CorruptedIndexedRuleset) {
  // Set-up an observer for RulesetMatcher to monitor the number of extension
  // rulesets.
  RulesetCountWaiter ruleset_count_waiter;
  ScopedRulesetManagerTestObserver scoped_observer(
      &ruleset_count_waiter,
      base::WrapRefCounted(ExtensionSystem::Get(profile())->info_map()));

  const GURL url = embedded_test_server()->GetURL(
      "google.com", "/pages_with_script/index.html");

  // Verifies whether |url| was successfully loaded.
  auto verify_page_load = [this, &url](bool success) {
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(success, WasFrameWithScriptLoaded(GetMainFrame()));

    content::PageType expected_page_type =
        success ? content::PAGE_TYPE_NORMAL : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  };

  // Initially no main frame requests should be blocked.
  {
    SCOPED_TRACE("Initial page load");
    verify_page_load(true);
  }

  // Load an extension which blocks all main frame requests.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  ruleset_count_waiter.WaitForRulesetCount(1);

  const ExtensionId extension_id = last_loaded_extension_id();
  const base::FilePath extension_path =
      extension_service()
          ->GetExtensionById(extension_id, false /*include_disabled*/)
          ->path();

  // Loading the extension should cause main frame requests to be blocked.
  {
    SCOPED_TRACE("Page load after loading extension");
    verify_page_load(false);
  }

  // Overwrite the indexed ruleset file with arbitrary data to mimic corruption,
  // while maintaining the correct version header.
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    std::string corrupted_data = GetVersionHeaderForTesting() + "data";
    ASSERT_EQ(static_cast<int>(corrupted_data.size()),
              base::WriteFile(file_util::GetIndexedRulesetPath(extension_path),
                              corrupted_data.c_str(), corrupted_data.size()));
  }

  // The extension should still continue to work since it doesn't need the
  // indexed ruleset while it is loaded.
  verify_page_load(false);

  // Now reload the extension and verify that we detect indexed ruleset
  // corruption and reindex the JSON ruleset.
  {
    DisableExtension(extension_id);
    ruleset_count_waiter.WaitForRulesetCount(0);

    base::HistogramTester tester;
    EnableExtension(extension_id);
    ruleset_count_waiter.WaitForRulesetCount(1);

    // Verify that loading the ruleset would have failed initially due to
    // checksum mismatch and later succeeded.
    EXPECT_EQ(1, tester.GetBucketCount(
                     "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                     RulesetMatcher::LoadRulesetResult::
                         kLoadErrorChecksumMismatch /*sample*/));
    EXPECT_EQ(1,
              tester.GetBucketCount(
                  "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                  RulesetMatcher::LoadRulesetResult::kLoadSuccess /*sample*/));

    // Verify that reindexing succeeded.
    tester.ExpectUniqueSample(
        "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
        true /*sample*/, 1 /*count*/);

    // The reindexing of the ruleset should cause the extension to work
    // correctly.
    SCOPED_TRACE("Page load after ruleset corruption");
    verify_page_load(false);
  }
}

// Tests that we surface a warning to the user if it's ruleset fails to load.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       WarningOnFailedRulesetLoad) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  const ExtensionId extension_id = last_loaded_extension_id();
  const auto* rules_monitor_service = BrowserContextKeyedAPIFactory<
      declarative_net_request::RulesMonitorService>::Get(profile());
  EXPECT_TRUE(rules_monitor_service->HasRegisteredRuleset(extension_id));

  // Mimic extension prefs corruption by overwriting the indexed ruleset
  // checksum.
  const int kInvalidRulesetChecksum = -1;
  ExtensionPrefs::Get(profile())->SetDNRRulesetChecksum(
      extension_id, kInvalidRulesetChecksum);

  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), extension_id);
  DisableExtension(extension_id);
  ASSERT_TRUE(registry_observer.WaitForExtensionUnloaded());
  EXPECT_FALSE(rules_monitor_service->HasRegisteredRuleset(extension_id));

  // Both loading the indexed ruleset and reindexing the ruleset should fail
  // now.
  base::HistogramTester tester;
  WarningService* warning_service = WarningService::Get(profile());
  WarningServiceObserver warning_observer(warning_service, extension_id);
  EnableExtension(extension_id);

  // Wait till we surface a warning.
  warning_observer.WaitForWarning();
  EXPECT_THAT(warning_service->GetWarningTypesAffectingExtension(extension_id),
              ::testing::ElementsAre(Warning::kRulesetFailedToLoad));

  EXPECT_FALSE(rules_monitor_service->HasRegisteredRuleset(extension_id));

  // Verify that loading the ruleset failed due to checksum mismatch.
  EXPECT_EQ(1, tester.GetBucketCount(
                   "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                   RulesetMatcher::LoadRulesetResult::
                       kLoadErrorChecksumMismatch /*sample*/));

  // Verify that re-indexing the ruleset failed.
  tester.ExpectUniqueSample(
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
      false /*sample*/, 1 /*count*/);
}

// Tests that we reindex the extension ruleset in case its ruleset format
// version is not the same as one used by Chrome.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       ReindexOnRulesetVersionMismatch) {
  // Set up an observer for RulesetMatcher to monitor the number of extension
  // rulesets.
  RulesetCountWaiter ruleset_count_waiter;
  ScopedRulesetManagerTestObserver scoped_observer(
      &ruleset_count_waiter,
      base::WrapRefCounted(ExtensionSystem::Get(profile())->info_map()));

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  ruleset_count_waiter.WaitForRulesetCount(1);

  const ExtensionId extension_id = last_loaded_extension_id();
  const auto* rules_monitor_service = BrowserContextKeyedAPIFactory<
      declarative_net_request::RulesMonitorService>::Get(profile());
  EXPECT_TRUE(rules_monitor_service->HasRegisteredRuleset(extension_id));

  DisableExtension(extension_id);
  ruleset_count_waiter.WaitForRulesetCount(0);
  EXPECT_FALSE(rules_monitor_service->HasRegisteredRuleset(extension_id));

  // Now change the current indexed ruleset format version. This should cause a
  // version mismatch when the extension is loaded again, but reindexing should
  // still succeed.
  const int kIndexedRulesetFormatVersion = 100;
  std::string old_version_header = GetVersionHeaderForTesting();
  SetIndexedRulesetFormatVersionForTesting(kIndexedRulesetFormatVersion);
  ASSERT_NE(old_version_header, GetVersionHeaderForTesting());

  base::HistogramTester tester;
  EnableExtension(extension_id);
  ruleset_count_waiter.WaitForRulesetCount(1);
  EXPECT_TRUE(rules_monitor_service->HasRegisteredRuleset(extension_id));

  // Verify that loading the ruleset would have failed initially due to
  // version header mismatch and later succeeded.
  EXPECT_EQ(1, tester.GetBucketCount(
                   "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                   RulesetMatcher::LoadRulesetResult::
                       kLoadErrorVersionMismatch /*sample*/));
  EXPECT_EQ(1, tester.GetBucketCount(
                   "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                   RulesetMatcher::LoadRulesetResult::kLoadSuccess /*sample*/));

  // Verify that reindexing succeeded.
  tester.ExpectUniqueSample(
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
      true /*sample*/, 1 /*count*/);
}

// Tests that redirecting requests using the declarativeNetRequest API works
// with extensions_features::kRuntimeHostPermissions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       WithheldPermissions_Redirect) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

  // Load an extension which redirects all script requests made to
  // "b.com/subresources/not_a_valid_script.js", to
  // "b.com/subresources/script.js".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter =
      std::string("b.com/subresources/not_a_valid_script.js");
  rule.condition->resource_types = std::vector<std::string>({"script"});
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect_url =
      embedded_test_server()->GetURL("b.com", "/subresources/script.js").spec();

  std::vector<std::string> host_permissions = {"*://a.com/", "*://b.com/*"};
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "extension" /* directory */, host_permissions));

  const Extension* extension = extension_service()->GetExtensionById(
      last_loaded_extension_id(), false /*include_disabled*/);
  ASSERT_TRUE(extension);

  auto verify_script_redirected = [this, extension](
                                      const GURL& page_url,
                                      bool expect_script_redirected,
                                      int expected_blocked_actions) {
    ui_test_utils::NavigateToURL(browser(), page_url);

    // The page should have loaded correctly.
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    EXPECT_EQ(expect_script_redirected,
              WasFrameWithScriptLoaded(GetMainFrame()));

    // The EmbeddedTestServer sees requests after the hostname has been
    // resolved.
    const GURL requested_script_url =
        embedded_test_server()->GetURL("/subresources/not_a_valid_script.js");
    const GURL redirected_script_url =
        embedded_test_server()->GetURL("/subresources/script.js");

    std::set<GURL> seen_requests = GetAndResetRequestsToServer();
    EXPECT_EQ(!expect_script_redirected,
              base::ContainsKey(seen_requests, requested_script_url));
    EXPECT_EQ(expect_script_redirected,
              base::ContainsKey(seen_requests, redirected_script_url));

    ExtensionActionRunner* runner =
        ExtensionActionRunner::GetForWebContents(web_contents());
    ASSERT_TRUE(runner);
    EXPECT_EQ(expected_blocked_actions, runner->GetBlockedActions(extension));
  };

  {
    const GURL page_url = embedded_test_server()->GetURL(
        "example.com", "/cross_site_script.html");
    SCOPED_TRACE(
        base::StringPrintf("Navigating to %s", page_url.spec().c_str()));

    // The extension should not redirect the script request. It has access to
    // the |requested_script_url| but not its initiator |page_url|.
    bool expect_script_redirected = false;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_NONE);
  }

  {
    const GURL page_url =
        embedded_test_server()->GetURL("a.com", "/cross_site_script.html");
    SCOPED_TRACE(
        base::StringPrintf("Navigating to %s", page_url.spec().c_str()));

    // The extension should redirect the script request. It has access to both
    // the |requested_script_url| and its initiator |page_url|.
    bool expect_script_redirected = true;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_NONE);
  }

  // Withhold access to all hosts.
  ScriptingPermissionsModifier scripting_modifier(
      profile(), base::WrapRefCounted(extension));
  scripting_modifier.SetWithholdHostPermissions(true);

  {
    const GURL page_url =
        embedded_test_server()->GetURL("a.com", "/cross_site_script.html");
    SCOPED_TRACE(base::StringPrintf("Navigating to %s with all hosts withheld",
                                    page_url.spec().c_str()));

    // The extension should not redirect the script request. It's access to both
    // the |requested_script_url| and its initiator |page_url| is withheld.
    bool expect_script_redirected = false;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_WEB_REQUEST);
  }

  // Grant access to only "b.com".
  scripting_modifier.GrantHostPermission(GURL("http://b.com"));
  {
    const GURL page_url =
        embedded_test_server()->GetURL("a.com", "/cross_site_script.html");
    SCOPED_TRACE(base::StringPrintf("Navigating to %s with a.com withheld",
                                    page_url.spec().c_str()));

    // The extension should not redirect the script request. It has access to
    // the |requested_script_url|, but its access to the request initiator
    // |page_url| is withheld.
    bool expect_script_redirected = false;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_WEB_REQUEST);
  }

  // Grant access to only "a.com".
  scripting_modifier.RemoveAllGrantedHostPermissions();
  scripting_modifier.GrantHostPermission(GURL("http://a.com"));
  {
    const GURL page_url =
        embedded_test_server()->GetURL("a.com", "/cross_site_script.html");
    SCOPED_TRACE(base::StringPrintf("Navigating to %s with b.com withheld",
                                    page_url.spec().c_str()));

    // The extension should redirect the script request. It's access to the
    // |requested_script_url| is withheld, but it has access to its initiator
    // |page_url|.
    bool expect_script_redirected = true;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_NONE);
  }
}

// Ensures that withholding permissions has no effect on blocking requests using
// the declarative net request API.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       WithheldPermissions_Block) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

  // Load an extension with access to <all_urls> which blocks all script
  // requests made on example.com.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  rule.condition->domains = std::vector<std::string>({"example.com"});
  rule.condition->resource_types = std::vector<std::string>({"script"});

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const Extension* extension = extension_service()->GetExtensionById(
      last_loaded_extension_id(), false /*include_disabled*/);
  ASSERT_TRUE(extension);

  EXPECT_TRUE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  // Withhold access to all hosts.
  ScriptingPermissionsModifier scripting_modifier(
      profile(), base::WrapRefCounted(extension));
  scripting_modifier.SetWithholdHostPermissions(true);

  EXPECT_EQ(
      extension->permissions_data()->active_permissions().explicit_hosts(),
      URLPatternSet(
          {URLPattern(URLPattern::SCHEME_CHROMEUI, "chrome://favicon/*")}));

  // Request made by index.html to script.js should be blocked despite the
  // extension having no active host permissions to the request.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/pages_with_script/index.html"));
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));

  // Sanity check that the script.js request is not blocked if does not match a
  // rule.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(
                                   "foo.com", "/pages_with_script/index.html"));
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
}

// Test fixture to verify that host permissions for the request url and the
// request initiator are properly checked when redirecting requests. Loads an
// example.com url with four sub-frames named frame_[1..4] from hosts
// frame_[1..4].com. These subframes point to invalid sources. The initiator for
// these frames will be example.com. Loads an extension which redirects these
// sub-frames to a valid source. Verifies that the correct frames are redirected
// depending on the host permissions for the extension.
class DeclarativeNetRequestHostPermissionsBrowserTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DeclarativeNetRequestHostPermissionsBrowserTest() {}

 protected:
  struct FrameRedirectResult {
    std::string child_frame_name;
    bool expect_frame_redirected;
  };

  void LoadExtensionWithHostPermissions(const std::vector<std::string>& hosts) {
    TestRule rule = CreateGenericRule();
    rule.priority = kMinValidPriority;
    rule.condition->url_filter = std::string("not_a_valid_child_frame.html");
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rule.action->type = std::string("redirect");
    rule.action->redirect_url =
        embedded_test_server()->GetURL("foo.com", "/child_frame.html").spec();

    ASSERT_NO_FATAL_FAILURE(
        LoadExtensionWithRules({rule}, "test_extension", hosts));
  }

  void RunTests(const std::vector<FrameRedirectResult>& expected_results) {
    ASSERT_EQ(4u, expected_results.size());

    GURL url = embedded_test_server()->GetURL("example.com",
                                              "/page_with_four_frames.html");
    ui_test_utils::NavigateToURL(browser(), url);
    ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

    for (const auto& frame_result : expected_results) {
      SCOPED_TRACE(base::StringPrintf("Testing child frame named %s",
                                      frame_result.child_frame_name.c_str()));

      content::RenderFrameHost* child =
          GetFrameByName(frame_result.child_frame_name);
      ASSERT_TRUE(child);
      EXPECT_EQ(frame_result.expect_frame_redirected,
                WasFrameWithScriptLoaded(child));
    }
  }

  std::string GetMatchPatternForDomain(const std::string& domain) const {
    return "*://*." + domain + ".com/*";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestHostPermissionsBrowserTest);
};

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestHostPermissionsBrowserTest,
                       AllURLs1) {
  // All frames should be redirected since the extension has access to all
  // hosts.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithHostPermissions({"<all_urls>"}));
  RunTests({{"frame_1", true},
            {"frame_2", true},
            {"frame_3", true},
            {"frame_4", true}});
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestHostPermissionsBrowserTest,
                       NoPermissions) {
  // The extension has no host permissions. No frames should be redirected.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithHostPermissions({}));
  RunTests({{"frame_1", false},
            {"frame_2", false},
            {"frame_3", false},
            {"frame_4", false}});
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestHostPermissionsBrowserTest,
                       SubframesWithNoInitiatorPermissions) {
  // The extension has access to requests to "frame_1.com" and "frame_2.com",
  // but not the initiator of those requests (example.com). No frames should be
  // redirected.
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithHostPermissions({GetMatchPatternForDomain("frame_1"),
                                        GetMatchPatternForDomain("frame_2")}));
  RunTests({{"frame_1", false},
            {"frame_2", false},
            {"frame_3", false},
            {"frame_4", false}});
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestHostPermissionsBrowserTest,
                       SubframesWithInitiatorPermission) {
  // The extension has access to requests to "frame_1.com" and "frame_4.com",
  // and also the initiator of those requests (example.com). Hence |frame_1| and
  // |frame_4| should be redirected.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithHostPermissions(
      {GetMatchPatternForDomain("frame_1"), GetMatchPatternForDomain("frame_4"),
       GetMatchPatternForDomain("example")}));
  RunTests({{"frame_1", true},
            {"frame_2", false},
            {"frame_3", false},
            {"frame_4", true}});
}

// Fixture to test the "resourceTypes" and "excludedResourceTypes" fields of a
// declarative rule condition.
class DeclarativeNetRequestResourceTypeBrowserTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DeclarativeNetRequestResourceTypeBrowserTest() {}

 protected:
  // TODO(crbug.com/696822): Add tests for "object", "ping", "other", "font",
  // "csp_report".
  enum ResourceTypeMask {
    kNone = 0,
    kSubframe = 1 << 0,
    kStylesheet = 1 << 1,
    kScript = 1 << 2,
    kImage = 1 << 3,
    kXHR = 1 << 4,
    kMedia = 1 << 5,
    kWebSocket = 1 << 6,
    kAll = (1 << 7) - 1
  };

  struct TestCase {
    std::string hostname;
    int blocked_mask;
  };

  void RunTests(const std::vector<TestCase>& test_cases) {
    // Start a web socket test server to test the websocket resource type.
    net::SpawnedTestServer websocket_test_server(
        net::SpawnedTestServer::TYPE_WS, net::GetWebSocketTestDataDirectory());
    ASSERT_TRUE(websocket_test_server.Start());

    // The |websocket_url| will echo the message we send to it.
    GURL websocket_url = websocket_test_server.GetURL("echo-with-no-extension");

    auto execute_script = [](content::RenderFrameHost* frame,
                             const std::string& script) {
      bool subresource_loaded = false;
      EXPECT_TRUE(content::ExecuteScriptAndExtractBool(frame, script,
                                                       &subresource_loaded));
      return subresource_loaded;
    };

    for (const auto& test_case : test_cases) {
      GURL url = embedded_test_server()->GetURL(test_case.hostname,
                                                "/subresources.html");
      SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

      ui_test_utils::NavigateToURL(browser(), url);
      ASSERT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

      content::RenderFrameHost* frame = GetMainFrame();

      // sub-frame.
      EXPECT_EQ(
          !(test_case.blocked_mask & kSubframe),
          execute_script(
              frame, "domAutomationController.send(!!window.frameLoaded);"));

      // stylesheet
      EXPECT_EQ(!(test_case.blocked_mask & kStylesheet),
                execute_script(frame, "testStylesheet();"));

      // script
      EXPECT_EQ(!(test_case.blocked_mask & kScript),
                execute_script(frame, "testScript();"));

      // image
      EXPECT_EQ(!(test_case.blocked_mask & kImage),
                execute_script(frame, "testImage();"));

      // xhr
      EXPECT_EQ(!(test_case.blocked_mask & kXHR),
                execute_script(frame, "testXHR();"));

      // media
      EXPECT_EQ(!(test_case.blocked_mask & kMedia),
                execute_script(frame, "testMedia();"));

      // websocket
      EXPECT_EQ(!(test_case.blocked_mask & kWebSocket),
                execute_script(
                    frame, base::StringPrintf("testWebSocket('%s');",
                                              websocket_url.spec().c_str())));
    }
  }

  // Loads an extension to test blocking different resource types.
  void LoadExtension() {
    struct {
      std::string domain;
      size_t id;
      std::vector<std::string> resource_types;
      std::vector<std::string> excluded_resource_types;
    } rules_data[] = {
        {"block_subframe.com", 1, {"sub_frame"}, {}},
        {"block_stylesheet.com", 2, {"stylesheet"}, {}},
        {"block_script.com", 3, {"script"}, {}},
        {"block_image.com", 4, {"image"}, {}},
        {"block_xhr.com", 5, {"xmlhttprequest"}, {}},
        {"block_media.com", 6, {"media"}, {}},
        {"block_websocket.com", 7, {"websocket"}, {}},
        {"block_image_and_stylesheet.com", 8, {"image", "stylesheet"}, {}},
        {"block_subframe_and_xhr.com", 11, {"sub_frame", "xmlhttprequest"}, {}},
        {"block_all.com", 9, {}, {}},
        {"block_all_but_xhr_and_script.com",
         10,
         {},
         {"xmlhttprequest", "script"}},
    };

    std::vector<TestRule> rules;
    for (const auto& rule_data : rules_data) {
      TestRule rule = CreateGenericRule();

      // The "resourceTypes" property (i.e. |rule.condition->resource_types|)
      // should not be an empty list. It should either be omitted or be a non-
      // empty list.
      if (rule_data.resource_types.empty())
        rule.condition->resource_types = base::nullopt;
      else
        rule.condition->resource_types = rule_data.resource_types;

      rule.condition->excluded_resource_types =
          rule_data.excluded_resource_types;
      rule.id = rule_data.id;
      rule.condition->domains = std::vector<std::string>({rule_data.domain});
      // Don't specify the urlFilter, which should behaves the same as "*".
      rule.condition->url_filter = base::nullopt;
      rules.push_back(rule);
    }
    LoadExtensionWithRules(rules);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestResourceTypeBrowserTest);
};

// These are split into two tests to prevent a timeout. See crbug.com/787957.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestResourceTypeBrowserTest, Test1) {
  ASSERT_NO_FATAL_FAILURE(LoadExtension());
  RunTests({{"block_subframe.com", kSubframe},
            {"block_stylesheet.com", kStylesheet},
            {"block_script.com", kScript},
            {"block_image.com", kImage},
            {"block_xhr.com", kXHR},
            {"block_media.com", kMedia}});
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestResourceTypeBrowserTest, Test2) {
  ASSERT_NO_FATAL_FAILURE(LoadExtension());
  RunTests({{"block_websocket.com", kWebSocket},
            {"block_image_and_stylesheet.com", kImage | kStylesheet},
            {"block_subframe_and_xhr.com", kSubframe | kXHR},
            {"block_all.com", kAll},
            {"block_all_but_xhr_and_script.com", kAll & ~kXHR & ~kScript},
            {"block_none.com", kNone}});
}

INSTANTIATE_TEST_CASE_P(,
                        DeclarativeNetRequestBrowserTest,
                        ::testing::Values(ExtensionLoadType::PACKED,
                                          ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_CASE_P(,
                        DeclarativeNetRequestHostPermissionsBrowserTest,
                        ::testing::Values(ExtensionLoadType::PACKED,
                                          ExtensionLoadType::UNPACKED));
INSTANTIATE_TEST_CASE_P(,
                        DeclarativeNetRequestResourceTypeBrowserTest,
                        ::testing::Values(ExtensionLoadType::PACKED,
                                          ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_CASE_P(,
                        DeclarativeNetRequestBrowserTest_Packed,
                        ::testing::Values(ExtensionLoadType::PACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
