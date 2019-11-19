// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
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
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/web_request/web_request_api.h"
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
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
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
                                   content::BrowserContext* browser_context)
      : browser_context_(browser_context) {
    SetRulesetManagerTestObserver(observer);
  }

  ~ScopedRulesetManagerTestObserver() {
    SetRulesetManagerTestObserver(nullptr);
  }

 private:
  void SetRulesetManagerTestObserver(RulesetManager::TestObserver* observer) {
    declarative_net_request::RulesMonitorService::Get(browser_context_)
        ->ruleset_manager()
        ->SetObserverForTest(observer);
  }

  content::BrowserContext* browser_context_;

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
    if (!base::Contains(affected_extensions, extension_id_))
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
  DeclarativeNetRequestBrowserTest() {
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
  }

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

  std::string GetPageBody() const {
    std::string result;
    const char* script =
        "domAutomationController.send(document.body.innerText.trim())";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(web_contents(), script,
                                                       &result));
    return result;
  }

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

    // The histograms below are not logged for unpacked extensions.
    if (GetParam() == ExtensionLoadType::PACKED) {
      tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram,
                              1 /* count */);
      tester.ExpectBucketCount(kManifestRulesCountHistogram,
                               rules.size() /*sample*/, 1 /* count */);
    }
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

  // Returns true if the navigation to given |url| is blocked.
  bool IsNavigationBlocked(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    return !WasFrameWithScriptLoaded(GetMainFrame());
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

  void AddDynamicRules(const ExtensionId& extension_id,
                       const std::vector<TestRule>& rules) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.updateDynamicRules([], $1, function () {
        window.domAutomationController.send(chrome.runtime.lastError ?
            chrome.runtime.lastError.message : 'success');
      });
    )";

    // Serialize |rules|.
    ListBuilder builder;
    for (const auto& rule : rules)
      builder.Append(rule.ToValue());

    // A cast is necessary from ListValue to Value, else this fails to compile.
    const std::string script = content::JsReplace(
        kScript, static_cast<const base::Value&>(*builder.Build()));
    ASSERT_EQ("success", ExecuteScriptInBackgroundPage(extension_id, script));
  }

  void RemoveDynamicRules(const ExtensionId& extension_id,
                          const std::vector<int> rule_ids) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.updateDynamicRules($1, [], function () {
        window.domAutomationController.send(chrome.runtime.lastError ?
            chrome.runtime.lastError.message : 'success');
      });
    )";

    // Serialize |rule_ids|.
    ListBuilder builder;
    for (int rule_id : rule_ids)
      builder.Append(rule_id);

    // A cast is necessary from ListValue to Value, else this fails to compile.
    const std::string script = content::JsReplace(
        kScript, static_cast<const base::Value&>(*builder.Build()));
    ASSERT_EQ("success", ExecuteScriptInBackgroundPage(extension_id, script));
  }

  void SetActionsAsBadgeText(const ExtensionId& extension_id, bool pref) {
    const char* pref_string = pref ? "true" : "false";
    static constexpr char kSetActionCountAsBadgeTextScript[] = R"(
      chrome.declarativeNetRequest.setActionCountAsBadgeText(%s);
      window.domAutomationController.send("done");
    )";

    ExecuteScriptInBackgroundPage(
        extension_id,
        base::StringPrintf(kSetActionCountAsBadgeTextScript, pref_string));
  }

  // Navigates frame with name |frame_name| to |url|.
  void NavigateFrame(const std::string& frame_name,
                     const GURL& url,
                     bool use_frame_referrer = true) {
    content::TestNavigationObserver navigation_observer(
        web_contents(), 1 /*number_of_navigations*/);

    const char* referrer_policy = use_frame_referrer ? "origin" : "no-referrer";

    ASSERT_TRUE(content::ExecuteScript(
        GetMainFrame(),
        base::StringPrintf(R"(
          document.getElementsByName('%s')[0].referrerPolicy = '%s';
          document.getElementsByName('%s')[0].src = '%s';)",
                           frame_name.c_str(), referrer_policy,
                           frame_name.c_str(), url.spec().c_str())));
    navigation_observer.Wait();
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
using DeclarativeNetRequestBrowserTest_Unpacked =
    DeclarativeNetRequestBrowserTest;

#if defined(OS_WIN) && !defined(NDEBUG)
// TODO: test times out on win7-debug. http://crbug.com/900447.
#define MAYBE_BlockRequests_UrlFilter DISABLED_BlockRequests_UrlFilter
#else
#define MAYBE_BlockRequests_UrlFilter BlockRequests_UrlFilter
#endif
// Tests the "urlFilter" property of a declarative rule condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       MAYBE_BlockRequests_UrlFilter) {
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

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_SeparatorMatchesEndOfURL) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("page2.html^");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});

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

// Tests allowing rules for blocks.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, AllowBlock) {
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

// Tests allowing rules for redirects.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, AllowRedirect) {
  set_has_background_script(true);

  const GURL static_redirect_url = embedded_test_server()->GetURL(
      "example.com", base::StringPrintf("/pages_with_script/page2.html"));

  const GURL dynamic_redirect_url = embedded_test_server()->GetURL(
      "abc.com", base::StringPrintf("/pages_with_script/page.html"));

  // Create 2 static and 2 dynamic rules.
  struct {
    std::string url_filter;
    int id;
    std::string action_type;
    base::Optional<std::string> redirect_url;
  } rules_data[] = {
      {"google.com", 1, "redirect", static_redirect_url.spec()},
      {"num=1|", 2, "allow", base::nullopt},
      {"1|", 3, "redirect", dynamic_redirect_url.spec()},
      {"num=21|", 4, "allow", base::nullopt},
  };

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.id = rule_data.id;
    rule.priority = kMinValidPriority;
    rule.condition->url_filter = rule_data.url_filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = rule_data.action_type;
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rules.push_back(rule);
  }

  std::vector<TestRule> static_rules;
  static_rules.push_back(rules[0]);
  static_rules.push_back(rules[1]);

  std::vector<TestRule> dynamic_rules;
  dynamic_rules.push_back(rules[2]);
  dynamic_rules.push_back(rules[3]);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      static_rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  auto get_url = [this](int i) {
    return embedded_test_server()->GetURL(
        "google.com",
        base::StringPrintf("/pages_with_script/page.html?num=%d", i));
  };

  struct {
    GURL initial_url;
    GURL expected_final_url;
  } static_test_cases[] = {
      {get_url(0), static_redirect_url},
      {get_url(1), get_url(1)},
  };

  for (const auto& test_case : static_test_cases) {
    GURL url = test_case.initial_url;
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

    GURL final_url = web_contents()->GetLastCommittedURL();
    EXPECT_EQ(test_case.expected_final_url, final_url);
  }

  // Now add dynamic rules. These should override static rules in priority.
  const ExtensionId& extension_id = last_loaded_extension_id();
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, dynamic_rules));

  // Test that rules follow the priority of: dynamic allow > dynamic redirect >
  // static allow > static redirect.
  struct {
    GURL initial_url;
    GURL expected_final_url;
  } dynamic_test_cases[] = {
      {get_url(1), dynamic_redirect_url},
      {get_url(21), get_url(21)},
  };

  for (const auto& test_case : dynamic_test_cases) {
    GURL url = test_case.initial_url;
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

    GURL final_url = web_contents()->GetLastCommittedURL();
    EXPECT_EQ(test_case.expected_final_url, final_url);
  }
}

// Tests that the extension ruleset is active only when the extension is
// enabled.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       Enable_Disable_Reload_Uninstall) {
  set_has_background_script(true);

  // Block all main frame requests to "index.html".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("index.html");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  const ExtensionId extension_id = last_loaded_extension_id();

  // Add dynamic rule to block requests to "page.html".
  rule.condition->url_filter = std::string("page.html");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  GURL static_rule_url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html");
  GURL dynamic_rule_url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page.html");

  auto test_extension_enabled = [&](bool expected_enabled) {
    // Wait for any pending actions caused by extension state change.
    content::RunAllTasksUntilIdle();
    EXPECT_EQ(expected_enabled,
              ExtensionRegistry::Get(profile())->enabled_extensions().Contains(
                  extension_id));

    // If the extension is enabled, both the |static_rule_url| and
    // |dynamic_rule_url| should be blocked.
    EXPECT_EQ(expected_enabled, IsNavigationBlocked(static_rule_url));
    EXPECT_EQ(expected_enabled, IsNavigationBlocked(dynamic_rule_url));
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
    rule.action->redirect.emplace();
    rule.action->redirect->url = redirect_url_for_extension_number(i);
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
      {"ghi*", 6, "redirect", get_url_for_host("ghijk.com")},
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
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  struct {
    std::string hostname;
    bool expected_main_frame_loaded;
    base::Optional<GURL> expected_final_url;
    base::Optional<size_t> expected_redirect_chain_length;
  } test_cases[] = {
      // example.com -> yahoo.com -> google.com.
      {"example.com", true, GURL(get_url_for_host("google.com")), 3},
      // yahoo.com -> google.com.
      {"yahoo.com", true, GURL(get_url_for_host("google.com")), 2},
      // abc.com -> def.com (blocked).
      // Note def.com won't be redirected since blocking rules are given
      // priority over redirect rules.
      {"abc.com", false, base::nullopt, base::nullopt},
      // def.com (blocked).
      {"def.com", false, base::nullopt, base::nullopt},
      // ghi.com -> ghijk.com.
      // Though ghijk.com still matches the redirect rule for |ghi*|, it will
      // not redirect to itself.
      {"ghi.com", true, GURL(get_url_for_host("ghijk.com")), 2},
  };

  for (const auto& test_case : test_cases) {
    std::string url = get_url_for_host(test_case.hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.c_str()));

    ui_test_utils::NavigateToURL(browser(), GURL(url));
    EXPECT_EQ(test_case.expected_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    if (!test_case.expected_final_url) {
      EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());
    } else {
      EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

      GURL final_url = web_contents()->GetLastCommittedURL();
      EXPECT_EQ(*test_case.expected_final_url, final_url);

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
      rule.action->redirect.emplace();
      rule.action->redirect->url = redirect_url_for_priority(j);
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

// Test that upgradeScheme rules will change the scheme of matching requests to
// https.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, UpgradeRules) {
  auto get_url_for_host = [this](std::string hostname, const char* scheme) {
    GURL url = embedded_test_server()->GetURL(hostname,
                                              "/pages_with_script/index.html");

    url::Replacements<char> replacements;
    replacements.SetScheme(scheme, url::Component(0, strlen(scheme)));

    return url.ReplaceComponents(replacements);
  };

  GURL google_url = get_url_for_host("google.com", url::kHttpScheme);
  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    base::Optional<std::string> redirect_url;
  } rules_data[] = {
      {"exa*", 1, 4, "upgradeScheme", base::nullopt},
      {"|http:*yahoo", 2, 100, "redirect", "http://other.com"},
      // Since the test server can only display http requests, redirect all
      // https requests to google.com in the end.
      // TODO(crbug.com/985104): Add a https test server to display https pages
      // so this redirect rule can be removed.
      {"|https*", 3, 6, "redirect", google_url.spec()},
      {"exact.com", 4, 1, "block", base::nullopt},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.priority = rule_data.priority;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = rule_data.action_type;
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  // Now load an extension with another ruleset, except this extension has no
  // host permissions.
  TestRule upgrade_rule = CreateGenericRule();
  upgrade_rule.condition->url_filter = "yahoo";
  upgrade_rule.id = kMinValidID;
  upgrade_rule.priority = kMinValidPriority;
  upgrade_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  upgrade_rule.action->type = "upgradeScheme";

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      std::vector<TestRule>({upgrade_rule}), "test_extension_2", {}));

  struct {
    std::string hostname;
    const char* scheme;
    // |expected_final_url| is null if the request is expected to be blocked.
    base::Optional<GURL> expected_final_url;
  } test_cases[] = {
      {"exact.com", url::kHttpScheme, base::nullopt},
      // http://example.com -> https://example.com/ -> http://google.com
      {"example.com", url::kHttpScheme, google_url},
      // test_extension_2 should upgrade the scheme for http://yahoo.com
      // despite having no host permissions. Note that this request is not
      // matched with test_extension_1's ruleset as test_extension_2 is
      // installed more recently.
      // http://yahoo.com -> https://yahoo.com/ -> http://google.com
      {"yahoo.com", url::kHttpScheme, google_url},
  };

  for (const auto& test_case : test_cases) {
    GURL url = get_url_for_host(test_case.hostname, test_case.scheme);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);

    if (!test_case.expected_final_url) {
      EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());
    } else {
      EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

      const GURL& final_url = web_contents()->GetLastCommittedURL();
      EXPECT_EQ(*test_case.expected_final_url, final_url);
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
  rule.condition->url_filter =
      std::string(content::kChromeUIScheme) + url::kStandardSchemeSeparator;
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

  set_has_background_script(true);

  // Block all main frame requests to "index.html".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("index.html");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  const ExtensionId extension_id = last_loaded_extension_id();

  // Add dynamic rule to block main-frame requests to "page.html".
  rule.condition->url_filter = std::string("page.html");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html")));
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page.html")));
  EXPECT_FALSE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page2.html")));
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       BrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetParam());

  // Ensure that the DNR extension enabled in previous browser session still
  // correctly blocks network requests.
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html")));
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page.html")));
  EXPECT_FALSE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page2.html")));
}

// Ensure that Blink's in-memory cache is cleared on adding/removing rulesets.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, RendererCacheCleared) {
  // Set-up an observer for RulesetMatcher to monitor requests to
  // script.js.
  URLRequestMonitor script_monitor(
      embedded_test_server()->GetURL("example.com", "/cached/script.js"));
  ScopedRulesetManagerTestObserver scoped_observer(&script_monitor, profile());

  GURL url = embedded_test_server()->GetURL(
      "example.com", "/cached/page_with_cacheable_script.html");

  // With no extension loaded, the request to the script should succeed.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

  // NOTE: RulesetMatcher will not see network requests if no rulesets are
  // active.
  bool expect_request_seen =
      base::FeatureList::IsEnabled(
          extensions_features::kForceWebRequestProxyForTest);
  EXPECT_EQ(expect_request_seen, script_monitor.GetAndResetRequestSeen(false));

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
  EXPECT_EQ(expect_request_seen, script_monitor.GetAndResetRequestSeen(false));
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
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("http://does.not.resolve.test/pages_with_script/page.html"));
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

  const Extension* dnr_extension = extension_registry()->GetExtensionById(
      last_loaded_extension_id(), extensions::ExtensionRegistry::ENABLED);
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
        base::Contains(GetAndResetRequestsToServer(), script_url);
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
        EXPECT_TRUE(base::Contains(requests_seen, expected_request_url))
            << expected_request_url.spec()
            << " was not requested from the server.";
      } else {
        EXPECT_FALSE(base::Contains(requests_seen, expected_request_url))
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
  NavigateFrame(kFrameName1, frame_url_2);
  NavigateFrame(kFrameName2, frame_url_1);
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
  ScopedRulesetManagerTestObserver scoped_observer(&ruleset_count_waiter,
                                                   profile());

  set_has_background_script(true);

  // Load an extension which blocks all main-frame requests to "google.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("||google.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  ruleset_count_waiter.WaitForRulesetCount(1);

  const ExtensionId extension_id = last_loaded_extension_id();

  // Add a dynamic rule to block main-frame requests to "example.com".
  rule.condition->url_filter = std::string("||example.com");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  const GURL static_rule_url = embedded_test_server()->GetURL(
      "google.com", "/pages_with_script/index.html");
  const GURL dynamic_rule_url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html");
  const GURL unblocked_url = embedded_test_server()->GetURL(
      "yahoo.com", "/pages_with_script/index.html");

  const Extension* extension = extension_registry()->GetExtensionById(
      extension_id, ExtensionRegistry::ENABLED);
  RulesetSource static_source = RulesetSource::CreateStatic(*extension);
  RulesetSource dynamic_source =
      RulesetSource::CreateDynamic(profile(), *extension);

  // Loading the extension should cause some main frame requests to be blocked.
  {
    SCOPED_TRACE("Page load after loading extension");
    EXPECT_TRUE(IsNavigationBlocked(static_rule_url));
    EXPECT_TRUE(IsNavigationBlocked(dynamic_rule_url));
    EXPECT_FALSE(IsNavigationBlocked(unblocked_url));
  }

  // Helper to overwrite an indexed ruleset file with arbitrary data to mimic
  // corruption, while maintaining the correct version header.
  auto corrupt_file_for_checksum_mismatch =
      [](const base::FilePath& indexed_path) {
        base::ScopedAllowBlockingForTesting scoped_allow_blocking;
        std::string corrupted_data = GetVersionHeaderForTesting() + "data";
        ASSERT_EQ(static_cast<int>(corrupted_data.size()),
                  base::WriteFile(indexed_path, corrupted_data.c_str(),
                                  corrupted_data.size()));
      };

  // Helper to reload the extension and ensure it is working.
  auto test_extension_works_after_reload = [&]() {
    DisableExtension(extension_id);
    ruleset_count_waiter.WaitForRulesetCount(0);

    EnableExtension(extension_id);
    ruleset_count_waiter.WaitForRulesetCount(1);

    EXPECT_TRUE(IsNavigationBlocked(static_rule_url));
    EXPECT_TRUE(IsNavigationBlocked(dynamic_rule_url));
    EXPECT_FALSE(IsNavigationBlocked(unblocked_url));
  };

  const char* kLoadRulesetResultHistogram =
      "Extensions.DeclarativeNetRequest.LoadRulesetResult";
  const char* kReindexHistogram =
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful";

  // Test static ruleset re-indexing.
  {
    SCOPED_TRACE("Static ruleset corruption");
    corrupt_file_for_checksum_mismatch(static_source.indexed_path());

    base::HistogramTester tester;
    test_extension_works_after_reload();

    // Loading the ruleset would have failed initially due to checksum mismatch
    // and later succeeded.
    tester.ExpectBucketCount(kLoadRulesetResultHistogram,
                             RulesetMatcher::LoadRulesetResult::
                                 kLoadErrorChecksumMismatch /* sample */,
                             1 /* count */);
    // Count of 2 because we load both static and dynamic rulesets.
    tester.ExpectBucketCount(
        kLoadRulesetResultHistogram,
        RulesetMatcher::LoadRulesetResult::kLoadSuccess /* sample */,
        2 /* count */);
    // Verify that reindexing of the static ruleset succeeded.
    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 1 /*count*/);
  }

  // Test dynamic ruleset re-indexing.
  {
    SCOPED_TRACE("Dynamic ruleset corruption");
    corrupt_file_for_checksum_mismatch(dynamic_source.indexed_path());

    base::HistogramTester tester;
    test_extension_works_after_reload();

    // Loading the ruleset would have failed initially due to checksum mismatch
    // and later succeeded.
    tester.ExpectBucketCount(kLoadRulesetResultHistogram,
                             RulesetMatcher::LoadRulesetResult::
                                 kLoadErrorChecksumMismatch /* sample */,
                             1 /* count */);
    // Count of 2 because we load both static and dynamic rulesets.
    tester.ExpectBucketCount(
        kLoadRulesetResultHistogram,
        RulesetMatcher::LoadRulesetResult::kLoadSuccess /* sample */,
        2 /* count */);
    // Verify that reindexing of the dynamic ruleset succeeded.
    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 1 /*count*/);
  }

  // Go crazy and corrupt both static and dynamic rulesets.
  {
    SCOPED_TRACE("Static and dynamic ruleset corruption");
    corrupt_file_for_checksum_mismatch(dynamic_source.indexed_path());
    corrupt_file_for_checksum_mismatch(static_source.indexed_path());

    base::HistogramTester tester;
    test_extension_works_after_reload();

    // Loading the ruleset would have failed initially due to checksum mismatch
    // and later succeeded.
    tester.ExpectBucketCount(kLoadRulesetResultHistogram,
                             RulesetMatcher::LoadRulesetResult::
                                 kLoadErrorChecksumMismatch /* sample */,
                             2 /* count */);
    // Count of 2 because we load both static and dynamic rulesets.
    tester.ExpectBucketCount(
        kLoadRulesetResultHistogram,
        RulesetMatcher::LoadRulesetResult::kLoadSuccess /* sample */,
        2 /* count */);
    // Verify that reindexing of both the rulesets succeeded.
    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 2 /*count*/);
  }
}

// Tests that we surface a warning to the user if it's ruleset fails to load.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       WarningOnFailedRulesetLoad) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  const ExtensionId extension_id = last_loaded_extension_id();
  const auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(profile());
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
  set_has_background_script(true);

  // Set up an observer for RulesetMatcher to monitor the number of extension
  // rulesets.
  RulesetCountWaiter ruleset_count_waiter;
  ScopedRulesetManagerTestObserver scoped_observer(&ruleset_count_waiter,
                                                   profile());

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  ruleset_count_waiter.WaitForRulesetCount(1);

  const ExtensionId extension_id = last_loaded_extension_id();
  const auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(profile());
  EXPECT_TRUE(rules_monitor_service->HasRegisteredRuleset(extension_id));

  // Add a dynamic rule.
  AddDynamicRules(extension_id, {rule});

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

  // Verify that loading the static and dynamic rulesets would have failed
  // initially due to version header mismatch and later succeeded.
  EXPECT_EQ(2, tester.GetBucketCount(
                   "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                   RulesetMatcher::LoadRulesetResult::
                       kLoadErrorVersionMismatch /*sample*/));
  EXPECT_EQ(2, tester.GetBucketCount(
                   "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                   RulesetMatcher::LoadRulesetResult::kLoadSuccess /*sample*/));

  // Verify that reindexing succeeded.
  tester.ExpectUniqueSample(
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
      true /*sample*/, 2 /*count*/);
}

// Tests that redirecting requests using the declarativeNetRequest API works
// with runtime host permissions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       WithheldPermissions_Redirect) {
  // Load an extension which redirects all script requests made to
  // "b.com/subresources/not_a_valid_script.js", to
  // "b.com/subresources/script.js".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter =
      std::string("b.com/subresources/not_a_valid_script.js");
  rule.condition->resource_types = std::vector<std::string>({"script"});
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url =
      embedded_test_server()->GetURL("b.com", "/subresources/script.js").spec();

  std::vector<std::string> host_permissions = {"*://a.com/", "*://b.com/*"};
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "extension" /* directory */, host_permissions));

  const Extension* extension = extension_registry()->GetExtensionById(
      last_loaded_extension_id(), ExtensionRegistry::ENABLED);
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
              base::Contains(seen_requests, requested_script_url));
    EXPECT_EQ(expect_script_redirected,
              base::Contains(seen_requests, redirected_script_url));

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
  // Load an extension with access to <all_urls> which blocks all script
  // requests made on example.com.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  rule.condition->domains = std::vector<std::string>({"example.com"});
  rule.condition->resource_types = std::vector<std::string>({"script"});

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const Extension* extension = extension_registry()->GetExtensionById(
      last_loaded_extension_id(), ExtensionRegistry::ENABLED);
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

// Tests the dynamic rule support.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, DynamicRules) {
  set_has_background_script(true);

  // Add an extension which blocks main-frame requests to "yahoo.com".
  TestRule block_static_rule = CreateGenericRule();
  block_static_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  block_static_rule.condition->url_filter = std::string("||yahoo.com");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {block_static_rule}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const char* kUrlPath = "/pages_with_script/index.html";
  GURL yahoo_url = embedded_test_server()->GetURL("yahoo.com", kUrlPath);
  GURL google_url = embedded_test_server()->GetURL("google.com", kUrlPath);
  EXPECT_TRUE(IsNavigationBlocked(yahoo_url));
  EXPECT_FALSE(IsNavigationBlocked(google_url));

  // Add dynamic rules to block "google.com" and redirect pages on "example.com"
  // to |dynamic_redirect_url|.
  TestRule block_dynamic_rule = block_static_rule;
  block_dynamic_rule.condition->url_filter = std::string("||google.com");
  block_dynamic_rule.id = kMinValidID;

  GURL dynamic_redirect_url =
      embedded_test_server()->GetURL("dynamic.com", kUrlPath);
  TestRule redirect_rule = CreateGenericRule();
  redirect_rule.condition->url_filter = std::string("||example.com");
  redirect_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  redirect_rule.priority = kMinValidPriority;
  redirect_rule.action->type = std::string("redirect");
  redirect_rule.action->redirect.emplace();
  redirect_rule.action->redirect->url = dynamic_redirect_url.spec();
  redirect_rule.id = kMinValidID + 1;

  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(last_loaded_extension_id(),
                                          {block_dynamic_rule, redirect_rule}));

  EXPECT_TRUE(IsNavigationBlocked(google_url));
  EXPECT_TRUE(IsNavigationBlocked(yahoo_url));

  // Navigate to a page on "example.com". It should be redirected to
  // |dynamic_redirect_url|.
  GURL example_url = embedded_test_server()->GetURL("example.com", kUrlPath);
  ui_test_utils::NavigateToURL(browser(), example_url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(dynamic_redirect_url, web_contents()->GetLastCommittedURL());

  // Now add a dynamic rule to allow requests to yahoo.com.
  TestRule allow_rule = block_static_rule;
  allow_rule.id = kMinValidID + 2;
  allow_rule.action->type = std::string("allow");
  ASSERT_NO_FATAL_FAILURE(
      AddDynamicRules(last_loaded_extension_id(), {allow_rule}));

  // Dynamic ruleset gets more priority over the static ruleset and yahoo.com is
  // not blocked.
  EXPECT_FALSE(IsNavigationBlocked(yahoo_url));

  // Now remove the |block_rule| and |allow_rule|. Rule ids not present will be
  // ignored.
  ASSERT_NO_FATAL_FAILURE(RemoveDynamicRules(
      last_loaded_extension_id(),
      {*block_dynamic_rule.id, *allow_rule.id, kMinValidID + 100}));
  EXPECT_FALSE(IsNavigationBlocked(google_url));
  EXPECT_TRUE(IsNavigationBlocked(yahoo_url));
}

// Tests removal of the 'Referer' request header.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRefererRequestHeader) {
  auto test_referrer_blocked = [this](bool expected_referrer_blocked) {
    const GURL initial_url =
        embedded_test_server()->GetURL("example.com", "/simulate_click.html");
    const GURL url_with_referrer =
        embedded_test_server()->GetURL("example.com", "/echoheader?referer");
    content::TestNavigationObserver observer(web_contents(),
                                             2 /* number_of_navigations */);
    observer.set_wait_event(
        content::TestNavigationObserver::WaitEvent::kNavigationFinished);
    ui_test_utils::NavigateToURL(browser(), initial_url);
    observer.WaitForNavigationFinished();
    ASSERT_EQ(url_with_referrer, observer.last_navigation_url());

    std::string expected_referrer =
        expected_referrer_blocked ? "None" : initial_url.spec();
    EXPECT_EQ(expected_referrer, GetPageBody());
  };

  test_referrer_blocked(false);

  // Load an extension which blocks the Referer header for requests to
  // "example.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("||example.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = std::string("removeHeaders");
  rule.action->remove_headers_list = std::vector<std::string>({"referer"});

  // Set up an observer for RulesetMatcher to monitor the number of extension
  // rulesets.
  RulesetCountWaiter ruleset_count_waiter;
  ScopedRulesetManagerTestObserver scoped_observer(&ruleset_count_waiter,
                                                   profile());

  EXPECT_FALSE(
      ExtensionWebRequestEventRouter::GetInstance()->HasAnyExtraHeadersListener(
          profile()));
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  ruleset_count_waiter.WaitForRulesetCount(1);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(
      ExtensionWebRequestEventRouter::GetInstance()->HasAnyExtraHeadersListener(
          profile()));
  test_referrer_blocked(true);

  DisableExtension(last_loaded_extension_id());
  ruleset_count_waiter.WaitForRulesetCount(0);
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(
      ExtensionWebRequestEventRouter::GetInstance()->HasAnyExtraHeadersListener(
          profile()));
  test_referrer_blocked(false);
}

// Tests rules using the Redirect dictionary.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, Redirect) {
  TestRule rule1 = CreateGenericRule();
  rule1.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule1.id = kMinValidID;
  rule1.condition->url_filter = std::string("ex");
  rule1.action->type = std::string("redirect");
  rule1.priority = kMinValidPriority;
  rule1.action->redirect.emplace();
  rule1.action->redirect->url =
      embedded_test_server()
          ->GetURL("google.com", "/pages_with_script/index.html")
          .spec();

  TestRule rule2 = CreateGenericRule();
  rule2.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule2.id = kMinValidID + 1;
  rule2.condition->url_filter = std::string("example.com");
  rule2.action->type = std::string("redirect");
  rule2.priority = kMinValidPriority + 1;
  rule2.action->redirect.emplace();
  rule2.action->redirect->extension_path = "/manifest.json?query#fragment";

  TestRule rule3 = CreateGenericRule();
  rule3.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule3.id = kMinValidID + 2;
  rule3.condition->url_filter = std::string("||example.com");
  rule3.action->type = std::string("redirect");
  rule3.priority = kMinValidPriority + 2;
  rule3.action->redirect.emplace();
  rule3.action->redirect->transform.emplace();
  auto& transform = rule3.action->redirect->transform;
  transform->host = "new.host.com";
  transform->path = "/pages_with_script/page.html";
  transform->query = "?new_query";
  transform->fragment = "#new_fragment";

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule1, rule2, rule3}, "test_extension", {URLPattern::kAllUrlsPattern}));

  struct {
    GURL url;
    GURL expected_url;
  } cases[] = {{embedded_test_server()->GetURL("example.com",
                                               "/pages_with_script/index.html"),
                // Because of higher priority, the transform rule is chosen.
                embedded_test_server()->GetURL(
                    "new.host.com",
                    "/pages_with_script/page.html?new_query#new_fragment")},
               // Because of higher priority, the extensionPath rule is chosen.
               {embedded_test_server()->GetURL(
                    "xyz.com", "/pages_with_script/index.html?example.com"),
                GURL("chrome-extension://" + last_loaded_extension_id() +
                     "/manifest.json?query#fragment")},
               {embedded_test_server()->GetURL("ex.com",
                                               "/pages_with_script/index.html"),
                embedded_test_server()->GetURL(
                    "google.com", "/pages_with_script/index.html")}};

  for (const auto& test_case : cases) {
    SCOPED_TRACE("Testing " + test_case.url.spec());
    ui_test_utils::NavigateToURL(browser(), test_case.url);
    EXPECT_EQ(test_case.expected_url, web_contents()->GetLastCommittedURL());
  }
}

// Test that the badge text for an extension will update to reflect the number
// of actions taken on requests matching the extension's ruleset.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionsMatchedCountAsBadgeText) {
  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname,
                                          "/pages_with_script/index.html");
  };

  // This page simulates a user clicking on a link, so that the next page it
  // navigates to has a Referrer header.
  auto get_url_with_referrer = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname, "/simulate_click.html");
  };

  const std::string kFrameName1 = "frame1";
  const GURL page_url = embedded_test_server()->GetURL(
      "norulesmatched.com", "/page_with_two_frames.html");

  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    base::Optional<std::string> redirect_url;
    base::Optional<std::vector<std::string>> remove_headers_list;
  } rules_data[] = {
      {"abc.com", 1, 1, "block", base::nullopt, base::nullopt},
      {"def.com", 2, 1, "redirect", "http://zzz.com", base::nullopt},
      {"jkl.com", 3, 1, "removeHeaders", base::nullopt,
       std::vector<std::string>({"referer"})},
      {"abcd.com", 4, 1, "block", base::nullopt, base::nullopt},
      {"abcd", 5, 1, "allow", base::nullopt, base::nullopt},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.priority = rule_data.priority;
    rule.condition->resource_types =
        std::vector<std::string>({"main_frame", "sub_frame"});
    rule.action->type = rule_data.action_type;
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rule.action->remove_headers_list = rule_data.remove_headers_list;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* dnr_extension = extension_registry()->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);

  ExtensionPrefs::Get(profile())->SetDNRUseActionCountAsBadgeText(extension_id,
                                                                  true);

  ExtensionAction* action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  struct {
    std::string frame_hostname;
    std::string expected_badge_text;
    bool has_referrer_header;
  } test_cases[] = {
      // zzz.com does not match any rules, but we should still display 0 as the
      // badge text as the preference is on.
      {"zzz.com", "0", false},
      // abc.com is blocked by a matching rule and should increment the badge
      // text.
      {"abc.com", "1", false},
      // def.com is redirected by a matching rule and should increment the badge
      // text.
      {"def.com", "2", false},
      // jkl.com matches with a removeHeaders rule, but has no headers.
      // Therefore no action is taken and the badge text stays the same.
      {"jkl.com", "2", false},
      // jkl.com matches with a removeHeaders rule and has a referrer header.
      // Therefore the badge text should be incremented.
      {"jkl.com", "3", true},
      // abcd.com matches both a block rule and an allow rule. Since the allow
      // rule overrides the block rule, no action is taken and the badge text
      // stays the same,
      {"abcd.com", "3", false},
  };

  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

  // Verify that the badge text is 0 when navigation finishes.
  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("0", action->GetDisplayBadgeText(first_tab_id));

  for (const auto& test_case : test_cases) {
    GURL url = test_case.has_referrer_header
                   ? get_url_with_referrer(test_case.frame_hostname)
                   : get_url_for_host(test_case.frame_hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    NavigateFrame(kFrameName1, url, false /* use_frame_referrer */);
    EXPECT_EQ(test_case.expected_badge_text,
              action->GetDisplayBadgeText(first_tab_id));
  }

  std::string first_tab_badge_text = action->GetDisplayBadgeText(first_tab_id);

  const GURL second_tab_url = get_url_for_host("nomatch.com");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), second_tab_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  int second_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("0", action->GetDisplayBadgeText(second_tab_id));

  // Verify that the badge text for the first tab is unaffected.
  EXPECT_EQ(first_tab_badge_text, action->GetDisplayBadgeText(first_tab_id));
}

// Ensure web request events are still dispatched even if DNR blocks/redirects
// the request. (Regression test for crbug.com/999744).
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, WebRequestEvents) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_has_background_script(true);

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "||example.com";
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "test_extension", {URLPattern::kAllUrlsPattern}));

  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/index.html");

  // Set up web request listeners listening to request to |url|.
  const char kWebRequestListenerScript[] = R"(
    let filter = {'urls' : ['%s'], 'types' : ['main_frame']};

    let onBeforeRequestSeen = false;
    chrome.webRequest.onBeforeRequest.addListener(() => {
      onBeforeRequestSeen = true;
    }, filter);

    // The request will fail since it will be blocked by DNR.
    chrome.webRequest.onErrorOccurred.addListener(() => {
      if (onBeforeRequestSeen)
        chrome.test.sendMessage('PASS');
    }, filter);

    chrome.test.sendMessage('INSTALLED');
  )";

  ExtensionTestMessageListener pass_listener("PASS", false /* will_reply */);
  ExtensionTestMessageListener installed_listener("INSTALLED",
                                                  false /* will_reply */);
  ExecuteScriptInBackgroundPageNoWait(
      last_loaded_extension_id(),
      base::StringPrintf(kWebRequestListenerScript, url.spec().c_str()));

  // Wait for the web request listeners to be installed before navigating.
  ASSERT_TRUE(installed_listener.WaitUntilSatisfied());

  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_TRUE(pass_listener.WaitUntilSatisfied());
}

// Ensure Declarative Net Request gets priority over the web request API.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, WebRequestPriority) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_has_background_script(true);

  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/index.html");
  GURL redirect_url = embedded_test_server()->GetURL(
      "redirect.com", "/pages_with_script/index.html");

  TestRule example_com_redirect_rule = CreateGenericRule();
  example_com_redirect_rule.condition->url_filter = "||example.com";
  example_com_redirect_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  example_com_redirect_rule.action->type = std::string("redirect");
  example_com_redirect_rule.action->redirect.emplace();
  example_com_redirect_rule.action->redirect->url = redirect_url.spec();
  example_com_redirect_rule.priority = kMinValidPriority;

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({example_com_redirect_rule}, "test_extension",
                             {URLPattern::kAllUrlsPattern}));

  // Set up a web request listener to block the request to example.com.
  const char kWebRequestBlockScript[] = R"(
    let filter = {'urls' : ['%s'], 'types' : ['main_frame']};
    chrome.webRequest.onBeforeRequest.addListener((details) => {
      chrome.test.sendMessage('SEEN')
    }, filter, ['blocking']);
    chrome.test.sendMessage('INSTALLED');
  )";

  ExtensionTestMessageListener seen_listener("SEEN", false /* will_reply */);
  ExtensionTestMessageListener installed_listener("INSTALLED",
                                                  false /* will_reply */);
  ExecuteScriptInBackgroundPageNoWait(
      last_loaded_extension_id(),
      base::StringPrintf(kWebRequestBlockScript, url.spec().c_str()));

  // Wait for the web request listeners to be installed before navigating.
  ASSERT_TRUE(installed_listener.WaitUntilSatisfied());

  ui_test_utils::NavigateToURL(browser(), url);

  // Ensure the response from the web request listener was ignored and the
  // request was redirected.
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), redirect_url);

  // Ensure onBeforeRequest is seen by the web request extension.
  EXPECT_TRUE(seen_listener.WaitUntilSatisfied());
}

// Test that the extension cannot retrieve the number of actions matched
// from the badge text by calling chrome.browserAction.getBadgeText.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetBadgeTextForActionsMatched) {
  auto query_badge_text_from_ext = [this](const ExtensionId& extension_id,
                                          int tab_id) {
    static constexpr char kBadgeTextQueryScript[] = R"(
        chrome.browserAction.getBadgeText({tabId: %d}, badgeText => {
          window.domAutomationController.send(badgeText);
        });
      )";

    return ExecuteScriptInBackgroundPage(
        extension_id, base::StringPrintf(kBadgeTextQueryScript, tab_id));
  };

  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_has_background_script(true);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* dnr_extension = extension_registry()->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);

  ExtensionAction* action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  const std::string default_badge_text = "asdf";
  action->SetBadgeText(ExtensionAction::kDefaultTabId, default_badge_text);

  const GURL page_url = embedded_test_server()->GetURL(
      "norulesmatched.com", "/pages_with_script/index.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

  // The preference is initially turned off. Both the visible badge text and the
  // badge text queried by the extension using getBadgeText() should return the
  // default badge text.
  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ(default_badge_text, action->GetDisplayBadgeText(first_tab_id));

  std::string queried_badge_text =
      query_badge_text_from_ext(extension_id, first_tab_id);
  EXPECT_EQ(default_badge_text, queried_badge_text);

  SetActionsAsBadgeText(extension_id, true);
  // Since the preference is on for the current tab, attempting to query the
  // badge text from the extension should return the placeholder text instead of
  // the matched action count.
  queried_badge_text = query_badge_text_from_ext(extension_id, first_tab_id);
  EXPECT_EQ(declarative_net_request::kActionCountPlaceholderBadgeText,
            queried_badge_text);

  // The displayed badge text should show "0" as no actions have been matched.
  EXPECT_EQ("0", action->GetDisplayBadgeText(first_tab_id));

  SetActionsAsBadgeText(extension_id, false);
  // Switching the preference off should cause the extension queried badge text
  // to be the explicitly set badge text for this tab if it exists. In this
  // case, the queried badge text should be the default badge text.
  queried_badge_text = query_badge_text_from_ext(extension_id, first_tab_id);
  EXPECT_EQ(default_badge_text, queried_badge_text);

  // The displayed badge text should be the default badge text now that the
  // preference is off.
  EXPECT_EQ(default_badge_text, action->GetDisplayBadgeText(first_tab_id));

  // Verify that turning off the preference deletes the DNR action count within
  // the extension action.
  EXPECT_FALSE(action->HasDNRActionCount(first_tab_id));
}

// Test that enabling the setActionCountAsBadgeText preference will update
// all browsers sharing the same browser context.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionCountPreferenceMultipleWindows) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_has_background_script(true);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* dnr_extension = extension_registry()->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);

  ExtensionAction* extension_action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  const GURL page_url = embedded_test_server()->GetURL(
      "norulesmatched.com", "/pages_with_script/index.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

  int first_browser_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("", extension_action->GetDisplayBadgeText(first_browser_tab_id));

  // Now create a new browser with the same profile as |browser()| and navigate
  // to |page_url|.
  Browser* second_browser = CreateBrowser(profile());
  ui_test_utils::NavigateToURL(second_browser, page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame(second_browser)));
  content::WebContents* second_browser_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();

  int second_browser_tab_id =
      ExtensionTabUtil::GetTabId(second_browser_contents);
  EXPECT_EQ("", extension_action->GetDisplayBadgeText(second_browser_tab_id));

  // Set up an observer to listen for ExtensionAction updates for the active web
  // contents of both browser windows.
  TestExtensionActionAPIObserver test_api_observer(
      profile(), extension_id, {web_contents(), second_browser_contents});

  SetActionsAsBadgeText(extension_id, true);

  // Wait until ExtensionActionAPI::NotifyChange is called, then perform a
  // sanity check on the browser action's badge text.
  test_api_observer.Wait();

  EXPECT_EQ("0", extension_action->GetDisplayBadgeText(first_browser_tab_id));

  // The badge text for the second browser window should also update to the
  // matched action count because the second browser shares the same browser
  // context as the first.
  EXPECT_EQ("0", extension_action->GetDisplayBadgeText(second_browser_tab_id));
}

// Test that the action matched badge text for an extension is visible in an
// incognito context if the extension is incognito enabled.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionsMatchedCountAsBadgeTextIncognito) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "abc.com";
  rule.id = kMinValidID;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  std::vector<TestRule> rules({rule});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rules}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId extension_id = last_loaded_extension_id();
  util::SetIsIncognitoEnabled(extension_id, profile(), true /*enabled*/);

  ExtensionPrefs::Get(profile())->SetDNRUseActionCountAsBadgeText(extension_id,
                                                                  true);

  Browser* incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito_browser, GURL("http://abc.com"));

  content::WebContents* incognito_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  const Extension* dnr_extension = extension_registry()->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  ExtensionAction* incognito_action =
      ExtensionActionManager::Get(incognito_contents->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  // TODO(crbug.com/992251): This should be a "1" after the main-frame
  // navigation case is fixed.
  EXPECT_EQ("0", incognito_action->GetDisplayBadgeText(
                     ExtensionTabUtil::GetTabId(incognito_contents)));
}

// Test that the actions matched badge text for an extension will be reset
// when a main-frame navigation finishes.
// TODO(crbug.com/992251): Edit this test to add more main-frame cases.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionsMatchedCountAsBadgeTextMainFrame) {
  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname,
                                          "/pages_with_script/index.html");
  };

  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    std::vector<std::string> resource_types;
    base::Optional<std::string> redirect_url;
  } rules_data[] = {
      {"abc.com", 1, 1, "block", std::vector<std::string>({"script"}),
       base::nullopt},
      {"def.com", 2, 1, "redirect", std::vector<std::string>({"main_frame"}),
       get_url_for_host("abc.com").spec()},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.priority = rule_data.priority;
    rule.condition->resource_types = rule_data.resource_types;
    rule.action->type = rule_data.action_type;
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  const Extension* dnr_extension = extension_registry()->GetExtensionById(
      last_loaded_extension_id(), extensions::ExtensionRegistry::ENABLED);

  ExtensionPrefs::Get(profile())->SetDNRUseActionCountAsBadgeText(
      last_loaded_extension_id(), true);

  ExtensionAction* action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  struct {
    std::string frame_hostname;
    std::string expected_badge_text;
  } test_cases[] = {
      // The script on get_url_for_host("abc.com") matches with a rule and
      // should increment the badge text.
      {"abc.com", "1"},
      // No rules match, so the badge text should be 0 once navigation finishes.
      {"nomatch.com", "0"},
      // The request to def.com will redirect to get_url_for_host("abc.com") and
      // the script on abc.com should match with a rule.
      {"def.com", "1"},
  };

  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  for (const auto& test_case : test_cases) {
    GURL url = get_url_for_host(test_case.frame_hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expected_badge_text,
              action->GetDisplayBadgeText(first_tab_id));
  }
}

// Test that the badge text for extensions will update correctly for
// removeHeader rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       RemoveHeadersBadgeText) {
  auto get_referer_url = [this](std::string host) {
    return embedded_test_server()->GetURL(host, "/set-header?referer: none");
  };
  auto get_set_cookie_url = [this](std::string host) {
    return embedded_test_server()->GetURL(host, "/set-cookie?a=b");
  };

  auto create_remove_headers_rule =
      [](int id, const std::string& url_filter,
         const std::vector<std::string>& remove_headers_list) {
        TestRule rule = CreateGenericRule();
        rule.id = id;
        rule.condition->url_filter = url_filter;
        rule.condition->resource_types =
            std::vector<std::string>({"sub_frame"});
        rule.action->type = "removeHeaders";
        rule.action->remove_headers_list = remove_headers_list;

        return rule;
      };

  const std::string kFrameName1 = "frame1";
  const GURL page_url = embedded_test_server()->GetURL(
      "nomatch.com", "/page_with_two_frames.html");

  // Create an extension with rules and get the ExtensionAction for it.
  TestRule example_set_cookie_rule =
      create_remove_headers_rule(kMinValidID, "example.com", {"setCookie"});

  TestRule both_headers_rule = create_remove_headers_rule(
      kMinValidID + 1, "google.com", {"referer", "setCookie"});

  TestRule abc_set_cookie_rule =
      create_remove_headers_rule(kMinValidID + 2, "abc.com", {"setCookie"});

  TestRule abc_referer_rule =
      create_remove_headers_rule(kMinValidID + 3, "abc.com", {"referer"});

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({example_set_cookie_rule, both_headers_rule,
                              abc_set_cookie_rule, abc_referer_rule},
                             "extension_1", {}));

  const ExtensionId extension_1_id = last_loaded_extension_id();
  ExtensionPrefs::Get(profile())->SetDNRUseActionCountAsBadgeText(
      extension_1_id, true);

  ExtensionAction* extension_1_action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*extension_registry()->GetExtensionById(
              extension_1_id, extensions::ExtensionRegistry::ENABLED));

  // Create another extension which removes the referer header from example.com
  // and get the ExtensionAction for it.
  TestRule example_referer_rule =
      create_remove_headers_rule(kMinValidID, "example.com", {"referer"});

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({example_referer_rule}, "extension_2", {}));

  const ExtensionId extension_2_id = last_loaded_extension_id();
  ExtensionPrefs::Get(profile())->SetDNRUseActionCountAsBadgeText(
      extension_2_id, true);

  ExtensionAction* extension_2_action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*extension_registry()->GetExtensionById(
              extension_2_id, extensions::ExtensionRegistry::ENABLED));

  struct {
    GURL url;
    bool use_referrer;
    std::string expected_ext_1_badge_text;
    std::string expected_ext_2_badge_text;
  } test_cases[] = {
      // This request only has a Set-Cookie header. Only the badge text for the
      // extension with a remove Set-Cookie header rule should be incremented.
      {get_set_cookie_url("example.com"), false, "1", "0"},
      // This request only has a Referer header. Only the badge text for the
      // extension with a remove Referer header rule should be incremented.
      {get_referer_url("example.com"), true, "1", "1"},
      // This request has both a Referer and a Set-Cookie header. The badge text
      // for both extensions should be incremented.
      {get_set_cookie_url("example.com"), true, "2", "2"},
      // This request with a Referer and Set-Cookie header matches with one rule
      // from |extension_1| and so the action count for |extension_1| should
      // only increment by one,
      {get_set_cookie_url("google.com"), true, "3", "2"},
      // This request with a Referer and Set-Cookie header matches with two
      // separate rules from |extension_1| and so the action count for
      // |extension_1| should increment by two.
      {get_set_cookie_url("abc.com"), true, "5", "2"},
  };

  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("0", extension_1_action->GetDisplayBadgeText(first_tab_id));
  EXPECT_EQ("0", extension_2_action->GetDisplayBadgeText(first_tab_id));

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Testing URL: %s, using referrer: %s",
                                    test_case.url.spec().c_str(),
                                    test_case.use_referrer ? "true" : "false"));

    NavigateFrame(kFrameName1, test_case.url, test_case.use_referrer);
    EXPECT_EQ(test_case.expected_ext_1_badge_text,
              extension_1_action->GetDisplayBadgeText(first_tab_id));

    EXPECT_EQ(test_case.expected_ext_2_badge_text,
              extension_2_action->GetDisplayBadgeText(first_tab_id));
  }
}

// Test that the onRuleMatchedDebug event is only available for unpacked
// extensions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       OnRuleMatchedDebugAvailability) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_has_background_script(true);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const char kGetOnRuleMatchedDebugScript[] = R"(
    const hasEvent = !!chrome.declarativeNetRequest.onRuleMatchedDebug ?
      'true' : 'false';
    window.domAutomationController.send(hasEvent);
  )";
  std::string actual_event_availability = ExecuteScriptInBackgroundPage(
      last_loaded_extension_id(), kGetOnRuleMatchedDebugScript);

  std::string expected_event_availability =
      GetParam() == ExtensionLoadType::UNPACKED ? "true" : "false";

  ASSERT_EQ(expected_event_availability, actual_event_availability);
}

// Test that the onRuleMatchedDebug event returns the correct number of matched
// rules for a request which is matched with multiple rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Unpacked,
                       OnRuleMatchedDebugMultipleRules) {
  // This is only tested for unpacked extensions since the onRuleMatchedDebug
  // event is only available for unpacked extensions.
  ASSERT_EQ(ExtensionLoadType::UNPACKED, GetParam());

  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_has_background_script(true);

  auto create_remove_headers_rule =
      [](int id, const std::string& url_filter,
         const std::vector<std::string>& remove_headers_list) {
        TestRule rule = CreateGenericRule();
        rule.id = id;
        rule.condition->url_filter = url_filter;
        rule.condition->resource_types =
            std::vector<std::string>({"sub_frame"});
        rule.action->type = "removeHeaders";
        rule.action->remove_headers_list = remove_headers_list;

        return rule;
      };

  const std::string kFrameName1 = "frame1";
  const std::string sub_frame_host = "abc.com";
  const GURL page_url = embedded_test_server()->GetURL(
      "nomatch.com", "/page_with_two_frames.html");

  TestRule abc_referer_rule =
      create_remove_headers_rule(kMinValidID, sub_frame_host, {"referer"});

  TestRule abc_set_cookie_rule = create_remove_headers_rule(
      kMinValidID + 1, sub_frame_host, {"setCookie"});

  // Load an extension with removeHeaders rules for the Referer and Set-Cookie
  // headers.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {abc_set_cookie_rule, abc_referer_rule}, "extension_1", {}));

  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

  // Start the onRuleMatchedDebug observer.
  const char kOnRuleMatchedDebugScript[] = R"(
    var matchedRules = [];
    var onRuleMatchedDebugCallback = (rule) => {
      matchedRules.push(rule);
    };

    chrome.declarativeNetRequest.onRuleMatchedDebug.addListener(
      onRuleMatchedDebugCallback);
    window.domAutomationController.send('ready');
  )";

  ASSERT_EQ("ready", ExecuteScriptInBackgroundPage(last_loaded_extension_id(),
                                                   kOnRuleMatchedDebugScript));

  auto set_cookie_and_referer_url =
      embedded_test_server()->GetURL(sub_frame_host, "/set-cookie?a=b");

  NavigateFrame(kFrameName1, set_cookie_and_referer_url);

  // Now query the onRuleMatchedDebug results.
  const char kQueryMatchedRulesScript[] = R"(
    chrome.declarativeNetRequest.onRuleMatchedDebug.removeListener(
      onRuleMatchedDebugCallback);
    var ruleIds = matchedRules.map(matchedRule => matchedRule.rule.ruleId);
    window.domAutomationController.send(ruleIds.sort().join());
  )";

  std::string matched_rule_ids = ExecuteScriptInBackgroundPage(
      last_loaded_extension_id(), kQueryMatchedRulesScript);

  // The request to |set_cookie_and_referer_url| should be matched with the
  // Referer rule (ruleId 1) and the Set-Cookie rule (ruleId 2).
  EXPECT_EQ("1,2", matched_rule_ids);
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
    rule.action->redirect.emplace();
    rule.action->redirect->url =
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
                       SubframesRequireNoInitiatorPermissions) {
  // The extension has access to requests to "frame_1.com" and "frame_2.com".
  // These should be redirected. Note: extensions don't need access to the
  // initiator of a navigation request to redirect it (See crbug.com/918137).
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithHostPermissions({GetMatchPatternForDomain("frame_1"),
                                        GetMatchPatternForDomain("frame_2")}));
  RunTests({{"frame_1", true},
            {"frame_2", true},
            {"frame_3", false},
            {"frame_4", false}});
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

INSTANTIATE_TEST_SUITE_P(,
                         DeclarativeNetRequestBrowserTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_SUITE_P(,
                         DeclarativeNetRequestHostPermissionsBrowserTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));
INSTANTIATE_TEST_SUITE_P(,
                         DeclarativeNetRequestResourceTypeBrowserTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_SUITE_P(,
                         DeclarativeNetRequestBrowserTest_Packed,
                         ::testing::Values(ExtensionLoadType::PACKED));

INSTANTIATE_TEST_SUITE_P(,
                         DeclarativeNetRequestBrowserTest_Unpacked,
                         ::testing::Values(ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
