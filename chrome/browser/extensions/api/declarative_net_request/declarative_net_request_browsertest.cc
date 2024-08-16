// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/to_value_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_timeouts.h"
#include "base/test/values_test_util.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/version_info/channel.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_transport_simple_test_server.h"
#include "extensions/browser/api/declarative_net_request/action_tracker.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/file_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "ui/webui/untrusted_web_ui_browsertest_util.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace dnr_api = api::declarative_net_request;

using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

constexpr char kDefaultRulesetID[] = "id";

// Returns true if |window.scriptExecuted| is true for the given frame.
bool WasFrameWithScriptLoaded(content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host) {
    return false;
  }
  return content::EvalJs(render_frame_host, "!!window.scriptExecuted")
      .ExtractBool();
}

// Helper to wait for ruleset load in response to extension load.
class RulesetLoadObserver : public RulesMonitorService::TestObserver {
 public:
  RulesetLoadObserver(RulesMonitorService* service,
                      const ExtensionId& extension_id)
      : service_(service), extension_id_(extension_id) {
    service_->SetObserverForTest(this);
  }

  ~RulesetLoadObserver() override { service_->SetObserverForTest(nullptr); }

  void Wait() { run_loop_.Run(); }

 private:
  // RulesMonitorService::TestObserver override:
  void OnRulesetLoadComplete(const ExtensionId& extension_id) override {
    if (extension_id_ == extension_id)
      run_loop_.Quit();
  }

  const raw_ptr<RulesMonitorService> service_;
  const ExtensionId extension_id_;
  base::RunLoop run_loop_;
};

class DeclarativeNetRequestBrowserTest
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<ExtensionLoadType, bool>> {
 public:
  DeclarativeNetRequestBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI, blink::features::kFledge,
         blink::features::kFencedFrames,
         blink::features::kFencedFramesAPIChanges,
         blink::features::kFencedFramesDefaultMode,
         features::kPrivacySandboxAdsAPIsOverride},
        /*disabled_features=*/
        {// TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid
         // having to disable these features.
         features::kHttpsUpgrades, features::kHttpsFirstModeIncognito});
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
  }

  DeclarativeNetRequestBrowserTest(const DeclarativeNetRequestBrowserTest&) =
      delete;
  DeclarativeNetRequestBrowserTest& operator=(
      const DeclarativeNetRequestBrowserTest&) = delete;

  // Returns the path of the files served by the EmbeddedTestServer.
  static base::FilePath GetHttpServerPath() {
    base::FilePath test_root_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
    test_root_path = test_root_path.AppendASCII("extensions")
                         .AppendASCII("declarative_net_request");
    return test_root_path;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    if (GetAllowChromeURLs()) {
      command_line->AppendSwitch(switches::kExtensionsOnChromeURLs);
    } else {
      command_line->RemoveSwitch(switches::kExtensionsOnChromeURLs);
    }
  }

  // ExtensionBrowserTest overrides:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromDirectory(GetHttpServerPath());

    embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&DeclarativeNetRequestBrowserTest::MonitorRequest,
                            base::Unretained(this)));

    content::SetupCrossSiteRedirector(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");

    CreateTempDir();
    InitializeRulesetManagerObserver();
  }

  void TearDownOnMainThread() override {
    // Ensure |ruleset_manager_observer_| gets destructed on the UI thread.
    ruleset_manager_observer_.reset();

    ExtensionBrowserTest::TearDownOnMainThread();
  }

  // Handler to monitor the requests which reach the EmbeddedTestServer. This
  // will be run on EmbeddedTestServers' IO threads. Public so it can be bound
  // in test bodies.
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock lock(requests_to_server_lock_);
    requests_to_server_[request.GetURL()] = request;
    if (url_to_wait_for_ == request.GetURL()) {
      ASSERT_TRUE(wait_for_request_run_loop_);
      url_to_wait_for_ = GURL();
      wait_for_request_run_loop_->Quit();
    }
  }

 protected:
  ExtensionLoadType GetLoadType() { return testing::get<0>(GetParam()); }

  bool GetAllowChromeURLs() { return testing::get<1>(GetParam()); }

  // Returns the number of extensions with active rulesets.
  size_t extensions_with_rulesets_count() {
    return ruleset_manager()->GetMatcherCountForTest();
  }

  RulesetManagerObserver* ruleset_manager_observer() {
    return ruleset_manager_observer_.get();
  }

  // Waits till the number of extensions with active rulesets is |count|.
  void WaitForExtensionsWithRulesetsCount(size_t count) {
    ruleset_manager_observer()->WaitForExtensionsWithRulesetsCount(count);
  }

  content::WebContents* web_contents(Browser* browser) const {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* web_contents() const { return web_contents(browser()); }

  content::RenderFrameHost* GetPrimaryMainFrame(Browser* browser) const {
    return web_contents(browser)->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetPrimaryMainFrame() const {
    return GetPrimaryMainFrame(browser());
  }

  content::RenderFrameHost* GetFrameByName(const std::string& name) const {
    return content::FrameMatchingPredicate(
        web_contents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, name));
  }

  content::PageType GetPageType(Browser* browser) const {
    return web_contents(browser)
        ->GetController()
        .GetLastCommittedEntry()
        ->GetPageType();
  }

  RulesMonitorService* rules_monitor_service() {
    return RulesMonitorService::Get(profile());
  }

  RulesetManager* ruleset_manager() {
    return rules_monitor_service()->ruleset_manager();
  }

  const Extension* last_loaded_extension() {
    return extension_registry()->enabled_extensions().GetByID(
        last_loaded_extension_id());
  }

  content::PageType GetPageType() const { return GetPageType(browser()); }

  std::string GetPageBody() const {
    const char* script = "document.body.innerText.trim()";
    return content::EvalJs(web_contents(), script).ExtractString();
  }

  std::string GetPageCookie() const {
    const char* script = "document.cookie";
    return content::EvalJs(web_contents(), script).ExtractString();
  }

  void set_config_flags(unsigned flags) { flags_ = flags; }

  // Loads an extension with the given `rulesets` in the given `directory`.
  // Generates a fatal failure if the extension failed to load. `hosts`
  // specifies the host permissions the extensions should have. Waits until the
  // ruleset is loaded.
  void LoadExtensionWithRulesets(const std::vector<TestRulesetInfo>& rulesets,
                                 const std::string& directory,
                                 const std::vector<std::string>& hosts) {
    bool has_enabled_rulesets = base::ranges::any_of(
        rulesets,
        [](const TestRulesetInfo& ruleset) { return ruleset.enabled; });

    size_t expected_extensions_with_rulesets_count_change =
        has_enabled_rulesets ? 1 : 0;
    LoadExtensionInternal(rulesets, directory, hosts,
                          expected_extensions_with_rulesets_count_change,
                          /*has_dynamic_ruleset=*/false,
                          /*is_extension_update=*/false,
                          /*is_delayed_update=*/false);
  }

  // Similar to LoadExtensionWithRulesets above but updates the last loaded
  // extension instead. `expected_extensions_with_rulesets_count_change`
  // corresponds to the expected change in the number of extensions with
  // rulesets after extension update. `has_dynamic_ruleset` should be true if
  // the installed extension has a dynamic ruleset. If `is_delayed_update` is
  // set to true, then a delayed update will be simulated by receiving the new
  // version's update first, then reloading the extension to finish the update.
  void UpdateLastLoadedExtension(
      const std::vector<TestRulesetInfo>& new_rulesets,
      const std::string& new_directory,
      const std::vector<std::string>& new_hosts,
      int expected_extensions_with_rulesets_count_change,
      bool has_dynamic_ruleset,
      bool is_delayed_update) {
    LoadExtensionInternal(new_rulesets, new_directory, new_hosts,
                          expected_extensions_with_rulesets_count_change,
                          has_dynamic_ruleset, /*is_extension_update=*/true,
                          is_delayed_update);
  }

  // Specialization of LoadExtensionWithRulesets above for an extension with a
  // single static ruleset.
  void LoadExtensionWithRules(const std::vector<TestRule>& rules,
                              const std::string& directory,
                              const std::vector<std::string>& hosts) {
    constexpr char kJSONRulesFilename[] = "rules_file.json";
    LoadExtensionWithRulesets(
        {TestRulesetInfo(kDefaultRulesetID, kJSONRulesFilename,
                         ToListValue(rules))},
        directory, hosts);
  }

  // Returns a url with |filter| as a substring.
  GURL GetURLForFilter(const std::string& filter) const {
    return embedded_test_server()->GetURL(
        "abc.com", "/pages_with_script/index.html?" + filter);
  }

  void LoadExtensionWithRules(const std::vector<TestRule>& rules) {
    LoadExtensionWithRules(rules, "test_extension", {} /* hosts */);
  }

  // Returns true if the navigation to given |url| is blocked.
  bool IsNavigationBlocked(const GURL& url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return !WasFrameWithScriptLoaded(GetPrimaryMainFrame());
  }

  void VerifyNavigations(const std::vector<GURL>& expected_blocked_urls,
                         const std::vector<GURL>& expected_allowed_urls) {
    for (const GURL& url : expected_blocked_urls)
      EXPECT_TRUE(IsNavigationBlocked(url)) << url;

    for (const GURL& url : expected_allowed_urls)
      EXPECT_FALSE(IsNavigationBlocked(url)) << url;
  }

  std::string ExecuteScriptInBackgroundPageAndReturnString(
      const ExtensionId& extension_id,
      const std::string& script,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kActivate) {
    base::Value result = ExecuteScriptInBackgroundPage(extension_id, script,
                                                       script_user_activation);
    return result.is_string() ? result.GetString() : "";
  }

  TestRule CreateMainFrameBlockRule(const std::string& filter) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    return rule;
  }

  void AddDynamicRules(const ExtensionId& extension_id,
                       const std::vector<TestRule>& rules) {
    UpdateRules(extension_id, {}, rules, RulesetScope::kDynamic);
  }
  void RemoveDynamicRules(const ExtensionId& extension_id,
                          const std::vector<int>& rule_ids) {
    UpdateRules(extension_id, rule_ids, {}, RulesetScope::kDynamic);
  }

  void UpdateSessionRules(const ExtensionId& extension_id,
                          const std::vector<int>& rule_ids_to_remove,
                          const std::vector<TestRule>& rules_to_add) {
    UpdateRules(extension_id, rule_ids_to_remove, rules_to_add,
                RulesetScope::kSession);
  }

  void UpdateEnabledRulesets(
      const ExtensionId& extension_id,
      const std::vector<std::string>& ruleset_ids_to_remove,
      const std::vector<std::string>& ruleset_ids_to_add) {
    std::string result = UpdateEnabledRulesetsInternal(
        extension_id, ruleset_ids_to_remove, ruleset_ids_to_add);
    ASSERT_EQ("success", result);
  }

  void UpdateEnabledRulesetsAndFail(
      const ExtensionId& extension_id,
      const std::vector<std::string>& ruleset_ids_to_remove,
      const std::vector<std::string>& ruleset_ids_to_add,
      std::string expected_error) {
    std::string result = UpdateEnabledRulesetsInternal(
        extension_id, ruleset_ids_to_remove, ruleset_ids_to_add);
    ASSERT_NE("success", result);
    EXPECT_EQ(expected_error, result);
  }

  void UpdateStaticRules(const ExtensionId& extension_id,
                         const std::string& ruleset_id,
                         const std::vector<int>& rule_ids_to_disable,
                         const std::vector<int>& rule_ids_to_enable) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.updateStaticRules(
          {rulesetId: $1, disableRuleIds: $2, enableRuleIds: $3},
          () => {
            chrome.test.sendScriptResult(chrome.runtime.lastError ?
                chrome.runtime.lastError.message : 'success');
          });
    )";

    base::Value::List ids_to_disable = base::ToValueList(rule_ids_to_disable);
    base::Value::List ids_to_enable = base::ToValueList(rule_ids_to_enable);

    const std::string script = content::JsReplace(
        kScript, ruleset_id, base::Value(std::move(ids_to_disable)),
        base::Value(std::move(ids_to_enable)));

    std::string result =
        ExecuteScriptInBackgroundPageAndReturnString(extension_id, script);
    ASSERT_EQ("success", result);
  }

  base::flat_set<int> GetDisabledRuleIdsFromMatcher(
      const std::string& ruleset_id_string) {
    return GetDisabledRuleIdsFromMatcherForTesting(
        *ruleset_manager(), *last_loaded_extension(), ruleset_id_string);
  }

  void VerifyGetDisabledRuleIds(
      const ExtensionId& extension_id,
      const std::string& ruleset_id_string,
      const std::vector<int>& expected_disabled_rule_ids) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.getDisabledRuleIds(
          {rulesetId: $1},
          (disabledRuleIds) => {
            if (chrome.runtime.lastError) {
              chrome.test.sendScriptResult(
                  'error: ' + chrome.runtime.lastError.message);
              return;
            }
            if (!disabledRuleIds) {
              chrome.test.sendScriptResult('no result');
              return;
            }

            let actual = JSON.stringify(disabledRuleIds);
            let expected = JSON.stringify($2);
            chrome.test.sendScriptResult(actual == expected
                ? 'success'
                : ['expected:', expected, '; actual:', actual].join(''));
          });
    )";
    base::Value::List expected = base::ToValueList(expected_disabled_rule_ids);
    std::string result = ExecuteScriptInBackgroundPageAndReturnString(
        extension_id,
        content::JsReplace(kScript, ruleset_id_string, std::move(expected)));
    ASSERT_EQ("success", result);
  }

  void VerifyPublicRulesetIds(
      const Extension* extension,
      const std::vector<std::string>& expected_ruleset_ids) {
    ASSERT_TRUE(extension);

    CompositeMatcher* composite_matcher =
        ruleset_manager()->GetMatcherForExtension(extension->id());
    if (!composite_matcher) {
      // The extension could've been loaded with no enabled rulesets. This is
      // the only case where `composite_matcher` may be null.
      ASSERT_TRUE(expected_ruleset_ids.empty());
      return;
    }

    ASSERT_TRUE(composite_matcher);
    EXPECT_THAT(GetPublicRulesetIDs(*extension, *composite_matcher),
                UnorderedElementsAreArray(expected_ruleset_ids));
  }

  std::string SetExtensionActionOptions(const ExtensionId& extension_id,
                                        const std::string& options) {
    static constexpr char kSetExtensionActionOptionsScript[] = R"(
      chrome.declarativeNetRequest.setExtensionActionOptions(%s,
        () => {
          chrome.test.sendScriptResult(chrome.runtime.lastError ?
              chrome.runtime.lastError.message : 'success');
        }
      );
    )";

    return ExecuteScriptInBackgroundPageAndReturnString(
        extension_id,
        base::StringPrintf(kSetExtensionActionOptionsScript, options.c_str()));
  }

  // Navigates frame with name |frame_name| to |url|.
  void NavigateFrame(const std::string& frame_name,
                     const GURL& url,
                     bool use_frame_referrer = true) {
    content::TestNavigationObserver navigation_observer(
        web_contents(), 1 /*number_of_navigations*/);

    const char* referrer_policy = use_frame_referrer ? "origin" : "no-referrer";

    ASSERT_TRUE(content::ExecJs(
        GetPrimaryMainFrame(),
        base::StringPrintf(R"(
          document.getElementsByName('%s')[0].referrerPolicy = '%s';
          document.getElementsByName('%s')[0].src = '%s';)",
                           frame_name.c_str(), referrer_policy,
                           frame_name.c_str(), url.spec().c_str())));
    navigation_observer.Wait();
  }

  // Removes frame with name `frame_name` from the DOM, changes its src to
  // `url`, and then adds it back to the DOM to trigger the navigation.
  void RemoveNavigateAndReAddFrame(const std::string& frame_name,
                                   const GURL& url) {
    content::TestNavigationObserver navigation_observer(
        web_contents(), 1 /*number_of_navigations*/);

    ASSERT_TRUE(content::ExecJs(GetPrimaryMainFrame(),
                                content::JsReplace(R"(
          {
            const frame = document.getElementsByName($1)[0];
            const parentElement = frame.parentElement;

            frame.remove();
            frame.src = $2;
            parentElement.appendChild(frame);
          })",
                                                   frame_name, url)));
    navigation_observer.Wait();
  }

  // Verifies whether the frame with name `frame_name` is collapsed.
  void TestFrameCollapse(const std::string& frame_name, bool expect_collapsed) {
    SCOPED_TRACE(base::StringPrintf("Testing frame %s", frame_name.c_str()));
    content::RenderFrameHost* frame = GetFrameByName(frame_name);
    ASSERT_TRUE(frame);

    EXPECT_EQ(!expect_collapsed, WasFrameWithScriptLoaded(frame));

    constexpr char kScript[] = R"(
        var iframe = document.getElementsByName('%s')[0];
        var collapsed = iframe.clientWidth === 0 && iframe.clientHeight === 0;
        collapsed;
    )";
    EXPECT_EQ(expect_collapsed,
              content::EvalJs(GetPrimaryMainFrame(),
                              base::StringPrintf(kScript, frame_name.c_str())));
  }

  // Calls getMatchedRules for |extension_id| and optionally, the |tab_id| and
  // returns comma separated pairs of rule_id and tab_id, with each pair
  // delimited by '|'. Matched Rules are sorted in ascending order by ruleId,
  // and ties are resolved by the tabId (in ascending order.)
  // E.g. "<rule_1>,<tab_1>|<rule_2>,<tab_2>|<rule_3>,<tab_3>"
  std::string GetRuleAndTabIdsMatched(const ExtensionId& extension_id,
                                      std::optional<int> tab_id) {
    static constexpr char kGetMatchedRulesScript[] = R"(
      chrome.declarativeNetRequest.getMatchedRules({%s}, (rules) => {
        // |ruleAndTabIds| is a list of `${ruleId},${tabId}`
        var ruleAndTabIds = rules.rulesMatchedInfo.map(rule => {
          return [rule.rule.ruleId, rule.tabId];
        }).sort((a, b) => {
          // Sort ascending by rule ID, and resolve ties by tab ID.
          const idDiff = a.ruleId - b.ruleId;
          return (idDiff != 0) ? idDiff : a.tabId - b.tabId;
        }).map(ruleAndTabId => ruleAndTabId.join());

        // Join the comma separated (ruleId,tabId) pairs with '|'.
        chrome.test.sendScriptResult(ruleAndTabIds.join('|'));
      });
    )";

    std::string tab_id_param =
        tab_id ? base::StringPrintf("tabId: %d", *tab_id) : "";
    return ExecuteScriptInBackgroundPageAndReturnString(
        extension_id,
        base::StringPrintf(kGetMatchedRulesScript, tab_id_param.c_str()),
        browsertest_util::ScriptUserActivation::kDontActivate);
  }

  // Calls getMatchedRules for `extension_id` and optionally, the `tab_id` and
  // only rule matches more recent than `timestamp`. Returns a comma separated
  // list of rule_ids, Matched Rules are sorted in ascending order by ruleId,
  // and ties are resolved by the tabId (in ascending order.)
  // E.g. "<rule_1>,<rule_2>,<rule_3>"
  std::string GetRuleIdsMatched(const ExtensionId& extension_id,
                                base::Time timestamp) {
    static constexpr char kGetMatchedRulesScript[] = R"(
      chrome.declarativeNetRequest.getMatchedRules(%s, (rules) => {
        var ruleIds = rules.rulesMatchedInfo.map(rule => rule.rule.ruleId)
            .sort();

        chrome.test.sendScriptResult(ruleIds.join(','));
      });
    )";

    double js_timestamp = timestamp.InMillisecondsFSinceUnixEpochIgnoringNull();
    std::string param_string =
        base::StringPrintf("{minTimeStamp: %f}", js_timestamp);

    return ExecuteScriptInBackgroundPageAndReturnString(
        extension_id,
        base::StringPrintf(kGetMatchedRulesScript, param_string.c_str()),
        browsertest_util::ScriptUserActivation::kDontActivate);
  }

  // Calls getMatchedRules for |extension_id| and optionally, |tab_id| and
  // |timestamp|. Returns the matched rule count for rules more recent than
  // |timestamp| if specified, and are associated with the tab specified by
  // |tab_id| or all tabs if |tab_id| is not specified. |script_user_activation|
  // specifies if the call should be treated as a user gesture. Returns any API
  // error if the call fails.
  std::string GetMatchedRuleCount(
      const ExtensionId& extension_id,
      std::optional<int> tab_id,
      std::optional<base::Time> timestamp,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kDontActivate) {
    static constexpr char kGetMatchedRulesScript[] = R"(
      chrome.declarativeNetRequest.getMatchedRules(%s, (rules) => {
        if (chrome.runtime.lastError) {
          chrome.test.sendScriptResult(chrome.runtime.lastError.message);
          return;
        }

        var rule_count = rules.rulesMatchedInfo.length;
        chrome.test.sendScriptResult(rule_count.toString());
      });
    )";

    double timestamp_in_js =
        timestamp.has_value()
            ? timestamp->InMillisecondsFSinceUnixEpochIgnoringNull()
            : 0;

    std::string param_string =
        tab_id.has_value()
            ? base::StringPrintf("{tabId: %d, minTimeStamp: %f}", *tab_id,
                                 timestamp_in_js)
            : base::StringPrintf("{minTimeStamp: %f}", timestamp_in_js);

    return ExecuteScriptInBackgroundPageAndReturnString(
        extension_id,
        base::StringPrintf(kGetMatchedRulesScript, param_string.c_str()),
        script_user_activation);
  }

  std::string GetAvailableStaticRuleCount(const ExtensionId& extension_id) {
    static constexpr char kGetAvailableStaticRuleCountScript[] = R"(
      chrome.declarativeNetRequest.getAvailableStaticRuleCount((rule_count) => {
        if (chrome.runtime.lastError) {
          chrome.test.sendScriptResult(chrome.runtime.lastError.message);
          return;
        }

        chrome.test.sendScriptResult(rule_count.toString());
      });
    )";

    return ExecuteScriptInBackgroundPageAndReturnString(
        extension_id, kGetAvailableStaticRuleCountScript,
        browsertest_util::ScriptUserActivation::kDontActivate);
  }

  std::map<GURL, net::test_server::HttpRequest> GetAndResetRequestsToServer() {
    base::AutoLock lock(requests_to_server_lock_);
    auto results = std::move(requests_to_server_);
    requests_to_server_.clear();
    return results;
  }

  // Waits until MonitorRequest() has observed `url_to_wait_for` at least once
  // since the last time GetAndResetRequestsToServer() was invoked. Returns
  // instantly if `url_to_wait_for` has already been observed.
  void WaitForRequest(const GURL& url_to_wait_for) {
    {
      base::AutoLock lock(requests_to_server_lock_);

      DCHECK(url_to_wait_for_.is_empty());
      DCHECK(!wait_for_request_run_loop_);

      if (requests_to_server_.count(url_to_wait_for))
        return;
      url_to_wait_for_ = url_to_wait_for;
      wait_for_request_run_loop_ = std::make_unique<base::RunLoop>();
    }

    wait_for_request_run_loop_->Run();
    wait_for_request_run_loop_.reset();
  }

  TestRule CreateModifyHeadersRule(
      int id,
      int priority,
      const std::string& url_filter,
      std::optional<std::vector<TestHeaderInfo>> request_headers,
      std::optional<std::vector<TestHeaderInfo>> response_headers) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.priority = priority;
    rule.condition->url_filter = url_filter;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rule.action->type = "modifyHeaders";
    rule.action->request_headers = std::move(request_headers);
    rule.action->response_headers = std::move(response_headers);

    return rule;
  }

  void CreateTempDir() { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void InitializeRulesetManagerObserver() {
    ruleset_manager_observer_ =
        std::make_unique<RulesetManagerObserver>(ruleset_manager());
  }

  net::EmbeddedTestServer* https_server() {
    if (!https_server_)
      InitializeHttpsServer();

    return https_server_.get();
  }

 private:
  enum class RulesetScope { kDynamic, kSession };
  void UpdateRules(const ExtensionId& extension_id,
                   const std::vector<int>& rule_ids_to_remove,
                   const std::vector<TestRule>& rules_to_add,
                   RulesetScope scope) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.%s(
        {addRules: $1, removeRuleIds: $2},
        function() {
          chrome.test.sendScriptResult(chrome.runtime.lastError ?
              chrome.runtime.lastError.message : 'success');
        });
    )";

    const char* function_name = nullptr;
    switch (scope) {
      case RulesetScope::kDynamic:
        function_name = "updateDynamicRules";
        break;
      case RulesetScope::kSession:
        function_name = "updateSessionRules";
        break;
    }

    const std::string script =
        content::JsReplace(base::StringPrintf(kScript, function_name),
                           base::ToValueList(rules_to_add, &TestRule::ToValue),
                           base::ToValueList(rule_ids_to_remove));
    ASSERT_EQ("success", ExecuteScriptInBackgroundPageAndReturnString(
                             extension_id, script));
  }

  // Helper to load an extension. `has_dynamic_ruleset` should be true if the
  // extension has a dynamic ruleset on load. If `is_extension_update`, the last
  // loaded extension is updated.
  void LoadExtensionInternal(const std::vector<TestRulesetInfo>& rulesets,
                             const std::string& directory,
                             const std::vector<std::string>& hosts,
                             int expected_extensions_with_rulesets_count_change,
                             bool has_dynamic_ruleset,
                             bool is_extension_update,
                             bool is_delayed_update) {
    CHECK(!is_extension_update || GetLoadType() == ExtensionLoadType::PACKED);

    // The "crx" directory is reserved for use by this test fixture.
    CHECK_NE("crx", directory);

    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    base::HistogramTester tester;

    base::FilePath extension_dir = temp_dir_.GetPath().AppendASCII(directory);
    ASSERT_FALSE(base::PathExists(extension_dir));
    EXPECT_TRUE(base::CreateDirectory(extension_dir));

    WriteManifestAndRulesets(extension_dir, rulesets, hosts, flags_,
                             directory /* extension_name */);

    ExtensionTestMessageListener background_page_ready_listener("ready");
    size_t current_ruleset_count = extensions_with_rulesets_count();

    const Extension* extension = nullptr;
    switch (GetLoadType()) {
      case ExtensionLoadType::PACKED: {
        base::FilePath crx_dir =
            temp_dir_.GetPath().AppendASCII("crx").AppendASCII(directory);
        base::FilePath crx_path = crx_dir.AppendASCII("temp.crx");

        base::FilePath pem_path;
        if (is_extension_update)
          pem_path = last_pem_path_;
        else
          last_pem_path_ = crx_dir.AppendASCII("temp.pem");

        ASSERT_FALSE(base::PathExists(crx_dir));
        ASSERT_TRUE(base::CreateDirectory(crx_dir));
        ASSERT_EQ(crx_path,
                  PackExtensionWithOptions(extension_dir, crx_path, pem_path,
                                           last_pem_path_ /* pem_out_path */
                                           ));

        if (is_extension_update) {
          const ExtensionId& extension_id = last_loaded_extension_id();
          if (is_delayed_update) {
            // TODO(kelvinjiang): When background script goes away, a different
            // method will be needed to trigger a delayed update.
            ASSERT_TRUE(flags_ & kConfig_HasBackgroundScript);
            ASSERT_TRUE(flags_ & kConfig_ListenForOnUpdateAvailable);
            UpdateExtensionWaitForIdle(extension_id, crx_path,
                                       /*expected_change=*/0);

            // Force a reload of the extension to complete the delayed update.
            // This invalidates the existing `extension` pointer so it needs to
            // be set again after the reload.
            extension_service()->ReloadExtension(extension_id);
            extension =
                ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
                    extension_id);
          } else {
            extension = UpdateExtension(extension_id, crx_path,
                                        /*expected_change=*/0);
          }
        } else {
          extension = InstallExtensionWithPermissionsGranted(
              crx_path, /*expected_change=*/1);
        }
        break;
      }
      case ExtensionLoadType::UNPACKED:
        extension = LoadExtension(extension_dir);
        break;
    }

    ASSERT_TRUE(extension);

    WaitForExtensionsWithRulesetsCount(
        current_ruleset_count + expected_extensions_with_rulesets_count_change);

    size_t expected_enabled_rulesets_count = has_dynamic_ruleset ? 1 : 0;
    size_t expected_manifest_enabled_rules_count = 0;

    bool has_enabled_rulesets = false;
    for (const TestRulesetInfo& info : rulesets) {
      size_t rules_count = info.rules_value.GetList().size();

      if (info.enabled) {
        has_enabled_rulesets = true;
        expected_enabled_rulesets_count++;
        expected_manifest_enabled_rules_count += rules_count;
      }
    }

    // The histograms below are not logged for unpacked extensions.
    if (GetLoadType() == ExtensionLoadType::PACKED) {
      size_t expected_histogram_counts = has_enabled_rulesets ? 1 : 0;

      tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram,
                              expected_histogram_counts);
      tester.ExpectBucketCount(kManifestEnabledRulesCountHistogram,
                               /*sample=*/expected_manifest_enabled_rules_count,
                               expected_histogram_counts);
    }
    tester.ExpectTotalCount(
        "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime",
        expected_enabled_rulesets_count);
    tester.ExpectUniqueSample(kLoadRulesetResultHistogram,
                              /*sample=*/LoadRulesetResult::kSuccess,
                              expected_enabled_rulesets_count);

    auto ruleset_filter = FileBackedRulesetSource::RulesetFilter::kIncludeAll;
    if (GetLoadType() == ExtensionLoadType::PACKED) {
      ruleset_filter =
          FileBackedRulesetSource::RulesetFilter::kIncludeManifestEnabled;
    }
    EXPECT_TRUE(AreAllIndexedStaticRulesetsValid(*extension, profile(),
                                                 ruleset_filter));

    // Wait for the background page to load if needed.
    if (flags_ & kConfig_HasBackgroundScript) {
      ASSERT_TRUE(background_page_ready_listener.WaitUntilSatisfied());
      ASSERT_EQ(extension->id(),
                background_page_ready_listener.extension_id_for_message());
    }

    // Ensure no load errors were reported.
    EXPECT_TRUE(LoadErrorReporter::GetInstance()->GetErrors()->empty());
  }

  std::string UpdateEnabledRulesetsInternal(
      const ExtensionId& extension_id,
      const std::vector<std::string>& ruleset_ids_to_remove,
      const std::vector<std::string>& ruleset_ids_to_add) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.updateEnabledRulesets({
        disableRulesetIds: $1,
        enableRulesetIds: $2
      }, () => {
        chrome.test.sendScriptResult(chrome.runtime.lastError ?
            chrome.runtime.lastError.message : 'success');
      });
    )";

    const std::string script =
        content::JsReplace(kScript, base::ToValueList(ruleset_ids_to_remove),
                           base::ToValueList(ruleset_ids_to_add));
    return ExecuteScriptInBackgroundPageAndReturnString(extension_id, script);
  }

  void InitializeHttpsServer() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);

    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_->AddDefaultHandlers();
    https_server_->ServeFilesFromDirectory(GetHttpServerPath());
    https_server_->RegisterRequestMonitor(
        base::BindRepeating(&DeclarativeNetRequestBrowserTest::MonitorRequest,
                            base::Unretained(this)));
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;

  unsigned flags_ = ConfigFlag::kConfig_None;

  // Requests observed by the EmbeddedTestServer. This is accessed on both the
  // UI and the EmbeddedTestServer's IO thread. Access is protected by
  // `requests_to_server_lock_`.
  std::map<GURL, net::test_server::HttpRequest> requests_to_server_
      GUARDED_BY(requests_to_server_lock_);
  // URL that `wait_for_request_run_loop_` is currently waiting to observe.
  GURL url_to_wait_for_ GUARDED_BY(requests_to_server_lock_);
  // RunLoop to quit when a request for `url_to_wait_for_` is observed.
  std::unique_ptr<base::RunLoop> wait_for_request_run_loop_;

  base::Lock requests_to_server_lock_;

  std::unique_ptr<RulesetManagerObserver> ruleset_manager_observer_;

  // Path to the PEM file for the last installed packed extension.
  base::FilePath last_pem_path_;

  // Most tests don't use this, so it is initialized lazily.
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

using DeclarativeNetRequestBrowserTest_Packed =
    DeclarativeNetRequestBrowserTest;
using DeclarativeNetRequestBrowserTest_Unpacked =
    DeclarativeNetRequestBrowserTest;

// Tests the "urlFilter" and "regexFilter" property of a declarative rule
// condition.
// TODO: test times out on win, mac and linux. http://crbug.com/900447.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       DISABLED_BlockRequests_UrlFilter) {
  struct {
    std::string filter;
    int id;
    bool is_regex_rule;
  } rules_data[] = {
      {"pages_with_script/*ex", 1, false},
      {"||a.b.com", 2, false},
      {"|http://*.us", 3, false},
      {"pages_with_script/page2.html|", 4, false},
      {"|http://msn*/pages_with_script/page.html|", 5, false},
      {"%20", 6, false},     // Block any urls with space.
      {"%C3%A9", 7, false},  // Percent-encoded non-ascii character é.
      // Internationalized domain "ⱴase.com" in punycode.
      {"|http://xn--ase-7z0b.com", 8, false},
      {R"((http|https)://(\w+\.){1,2}com.*reg$)", 9, true},
      {R"(\d+\.google\.com)", 10, true},
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
      {"msn.test", "/pages_with_script/page.html", false},  // Rule 5
      {"msn.test", "/pages_with_script/page.html?q=hello", true},
      {"a.msn.test", "/pages_with_script/page.html", true},
      {"abc.com", "/pages_with_script/page.html?q=hi bye", false},    // Rule 6
      {"abc.com", "/pages_with_script/page.html?q=hi%20bye", false},  // Rule 6
      {"abc.com", "/pages_with_script/page.html?q=hibye", true},
      {"abc.com",
       "/pages_with_script/page.html?q=" + base::WideToUTF8(L"\u00E9"),
       false},  // Rule 7
      {base::WideToUTF8(L"\x2c74"
                        L"ase.com"),
       "/pages_with_script/page.html", false},                  // Rule 8
      {"abc.com", "/pages_with_script/page2.html?reg", false},  // Rule 9
      {"abc.com", "/pages_with_script/page2.html?reg1", true},
      {"w1.w2.com", "/pages_with_script/page2.html?reg", false},  // Rule 9
      {"w1.w2.w3.com", "/pages_with_script/page2.html?reg", true},
      {"24.google.com", "/pages_with_script/page.html", false},  // Rule 10
      {"xyz.google.com", "/pages_with_script/page.html", true},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter.reset();

    if (rule_data.is_regex_rule)
      rule.condition->regex_filter = rule_data.filter;
    else
      rule.condition->url_filter = rule_data.filter;

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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

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
                     {"x.com", "xn--tst-bma.com" /* punycode for tést.com */},
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
      {base::WideToUTF8(L"tést.com"), false /*Rule 1*/, false /*Rule 2*/},
      {"b.x.com", false /* Rule 1 - Subdomain matches */, false /* Rule 2 */},
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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    content::RenderFrameHost* main_frame = GetPrimaryMainFrame();
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

// Tests the "requestDomains" and "excludedRequestDomains" properties of
// declarativeNetRequest rule conditions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_RequestDomains) {
  struct {
    size_t id;
    std::vector<std::string> request_domains;
    std::vector<std::string> excluded_request_domains;
  } rules_data[] = {
      {1, {"x.com", "y.com"}, {"a.x.com"}},
      {2, {/* all domains */}, {"z.com", "a.x.com", "y.com"}},
  };

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = "child_frame.html";
    rule.id = rule_data.id;

    // An empty list is not allowed for the "request_domains" property.
    if (!rule_data.request_domains.empty())
      rule.condition->request_domains = rule_data.request_domains;

    rule.condition->excluded_request_domains =
        rule_data.excluded_request_domains;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  GURL url = embedded_test_server()->GetURL(
      "example.com",
      "/request_domain_test.html?w.com,x.com,subdomain.x.com,y.com,a.x.com,"
      "subdomain.a.x.com,z.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  ASSERT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

  struct {
    std::string domain;
    bool expect_frame_loaded;
  } cases[] = {
      {"x.com", false /* Rule 1 */},
      {"subdomain.x.com", false /* Rule 1 - Subdomain matches */},
      {"a.x.com", true},
      {"subdomain.a.x.com", true},
      {"y.com", false /* Rule 1 */},
      {"z.com", true},
      {"w.com", false /* Rule 2 */},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(
        base::StringPrintf("Testing domain %s", test_case.domain.c_str()));

    content::RenderFrameHost* child = GetFrameByName(test_case.domain);
    EXPECT_TRUE(child);
    EXPECT_EQ(test_case.expect_frame_loaded, WasFrameWithScriptLoaded(child));
  }
}

// Tests the "domainType" property of a declarative rule condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_DomainType) {
  struct {
    std::string url_filter;
    size_t id;
    std::optional<std::string> domain_type;
  } rules_data[] = {
      {"child_frame.html?case=1", 1, std::string("firstParty")},
      {"child_frame.html?case=2", 2, std::string("thirdParty")},
      {"child_frame.html?case=3", 3, std::nullopt},
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
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

// Tests that main frame navigations to Web origins initiated by
// chrome-untrusted:// can be blocked.
IN_PROC_BROWSER_TEST_P(
    DeclarativeNetRequestBrowserTest,
    BlockRequests_WebMainFrameNavigationsFromChromeUntrusted) {
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test"));
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test2"));

  TestRule rule = CreateGenericRule();
  rule.id = 1;
  rule.condition->url_filter = "title2.html";
  rule.condition->resource_types = {"main_frame"};

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Opens a chrome-untrusted:// page.
  auto* rfh = ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome-untrusted://test/title1.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  {
    // Trigger a `window.open()` to a Web origin from chrome-untrusted:// page.
    const GURL web_url =
        embedded_test_server()->GetURL("example.com", "/title2.html");
    content::TestNavigationObserver navigation_observer(web_url);
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(content::ExecJs(
        rfh, content::JsReplace("window.open($1, '_blank');", web_url.spec())));
    navigation_observer.Wait();
    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  }

  {
    // Trigger a `window.open()` to a WebUI origin from chrome-untrusted://
    // page.
    const GURL webui_url = GURL("chrome-untrusted://test2/title2.html");
    content::TestNavigationObserver navigation_observer(webui_url);
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(content::ExecJs(
        rfh,
        content::JsReplace("window.open($1, '_blank');", webui_url.spec())));
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  {
    // The page itself navigates to a Web origin.
    const GURL web_url =
        embedded_test_server()->GetURL("example.com", "/title2.html");
    content::TestNavigationObserver navigation_observer(web_url);
    navigation_observer.WatchExistingWebContents();
    ASSERT_TRUE(content::ExecJs(
        rfh, content::JsReplace("location.href=$1;", web_url.spec())));
    navigation_observer.Wait();
    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  }
}

// Tests that subframe navigations to Web origins initiated by
// chrome-untrusted:// can't be blocked.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       CanNotBlockSubframeNavigationsFromChromeUntrusted) {
  content::TestUntrustedDataSourceHeaders headers;
  headers.child_src = "child-src *;";
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test", headers));

  TestRule rule = CreateGenericRule();
  rule.id = 1;
  rule.condition->url_filter = "title2.html";
  rule.condition->resource_types = {"main_frame"};

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Opens a chrome-untrusted:// page.
  auto* rfh = ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome-untrusted://test/title1.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Start a subframe navigation to Web origin.
  const auto web_url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  content::TestNavigationObserver navigation_observer(web_url);
  navigation_observer.WatchExistingWebContents();
  ASSERT_TRUE(content::ExecJs(rfh, content::JsReplace(R"javascript(
        const el = document.createElement("iframe");
        document.body.appendChild(el);
        el.src = $1;
      )javascript",
                                                      web_url.spec())));
  navigation_observer.Wait();

  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
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
    rule.priority = 1;
    rules.push_back(rule);
  }

  // Allow all main-frame requests ending with even numbers from 1 to
  // |kNumRequests|.
  for (int i = 2; i <= kNumRequests; i += 2) {
    rule.id = id++;
    rule.condition->url_filter = base::StringPrintf("num=%d|", i);
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = std::string("allow");
    rule.priority = 2;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  for (int i = 1; i <= kNumRequests; ++i) {
    GURL url = embedded_test_server()->GetURL(
        "example.com",
        base::StringPrintf("/pages_with_script/page.html?num=%d", i));
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    // All requests ending with odd numbers should be blocked.
    const bool page_should_load = (i % 2 == 0);
    EXPECT_EQ(page_should_load,
              WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
    content::PageType expected_page_type =
        page_should_load ? content::PAGE_TYPE_NORMAL : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests allowing rules for redirects.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, AllowRedirect) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  const GURL static_redirect_url = embedded_test_server()->GetURL(
      "example.com", base::StringPrintf("/pages_with_script/page2.html"));

  const GURL dynamic_redirect_url = embedded_test_server()->GetURL(
      "abc.com", base::StringPrintf("/pages_with_script/page.html"));

  // Create 2 static and 2 dynamic rules.
  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    std::optional<std::string> redirect_url;
  } rules_data[] = {
      {"google.com", 1, 1, "redirect", static_redirect_url.spec()},
      {"num=1|", 2, 3, "allow", std::nullopt},
      {"1|", 3, 4, "redirect", dynamic_redirect_url.spec()},
      {"num=3|", 4, 2, "allow", std::nullopt},
  };

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.id = rule_data.id;
    rule.priority = rule_data.priority;
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
      {get_url(3), static_redirect_url},
  };

  for (const auto& test_case : static_test_cases) {
    GURL url = test_case.initial_url;
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

    GURL final_url = web_contents()->GetLastCommittedURL();
    EXPECT_EQ(test_case.expected_final_url, final_url);
  }

  // Now add dynamic rules. These should share the priority space with static
  // rules.
  const ExtensionId& extension_id = last_loaded_extension_id();
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, dynamic_rules));

  // Test that dynamic and static rules are in the same priority space.
  struct {
    GURL initial_url;
    GURL expected_final_url;
  } dynamic_test_cases[] = {
      {get_url(1), dynamic_redirect_url},
      {get_url(3), get_url(3)},
  };

  for (const auto& test_case : dynamic_test_cases) {
    GURL url = test_case.initial_url;
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

    GURL final_url = web_contents()->GetLastCommittedURL();
    EXPECT_EQ(test_case.expected_final_url, final_url);
  }
}

// Test is flaky on win. http://crbug.com/1241762.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Enable_Disable_Reload_Uninstall \
  DISABLED_Enable_Disable_Reload_Uninstall
#else
#define MAYBE_Enable_Disable_Reload_Uninstall Enable_Disable_Reload_Uninstall
#endif
// Tests that the extension ruleset is active only when the extension is
// enabled.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       MAYBE_Enable_Disable_Reload_Uninstall) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Block all main frame requests to "static.example".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("static.example");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  const ExtensionId& extension_id = last_loaded_extension_id();

  // Add dynamic rule to block requests to "dynamic.example".
  rule.condition->url_filter = std::string("dynamic.example");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  // Add session-scoped rule to block requests to "session.example".
  rule.condition->url_filter = std::string("session.example");
  ASSERT_NO_FATAL_FAILURE(UpdateSessionRules(extension_id, {}, {rule}));

  constexpr char kUrlPath[] = "/pages_with_script/index.html";
  GURL static_rule_url =
      embedded_test_server()->GetURL("static.example", kUrlPath);
  GURL dynamic_rule_url =
      embedded_test_server()->GetURL("dynamic.example", kUrlPath);
  GURL session_rule_url =
      embedded_test_server()->GetURL("session.example", kUrlPath);

  auto test_extension_enabled = [&](bool expected_enabled) {
    EXPECT_EQ(expected_enabled,
              ExtensionRegistry::Get(profile())->enabled_extensions().Contains(
                  extension_id));

    // Ensure the various registered rules work correctly if the extension is
    // enabled.
    EXPECT_EQ(expected_enabled, IsNavigationBlocked(static_rule_url));
    EXPECT_EQ(expected_enabled, IsNavigationBlocked(dynamic_rule_url));
    EXPECT_EQ(expected_enabled, IsNavigationBlocked(session_rule_url));
  };

  {
    SCOPED_TRACE("Testing extension after load");
    EXPECT_EQ(1u, extensions_with_rulesets_count());
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing DisableExtension");
    DisableExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(0);
    test_extension_enabled(false);
  }

  {
    SCOPED_TRACE("Testing EnableExtension");
    EnableExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(1);
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing ReloadExtension");
    // Don't use ExtensionBrowserTest::ReloadExtension since it waits for the
    // extension to be loaded again. But we need to use our custom waiting logic
    // below.
    extension_service()->ReloadExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(0);
    WaitForExtensionsWithRulesetsCount(1);
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing UninstallExtension");
    UninstallExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(0);
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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
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
        ->GetURL(base::NumberToString(num) + ".com",
                 "/pages_with_script/index.html")
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
        {rule}, base::NumberToString(i), {URLPattern::kAllUrlsPattern}));

    // Verify that the install time of this extension is greater than the last
    // extension.
    base::Time install_time = ExtensionPrefs::Get(profile())->GetLastUpdateTime(
        last_loaded_extension_id());
    EXPECT_GT(install_time, last_extension_install_time);
    last_extension_install_time = install_time;
  }

  // Load a url on example.com. It should be redirected to the redirect url
  // corresponding to the most recently installed extension.
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
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
    int priority;
    std::string action_type;
    std::optional<std::string> redirect_url;
  } rules_data[] = {
      {"example.com", 1, 1, "redirect", get_url_for_host("yahoo.com")},
      {"yahoo.com", 2, 1, "redirect", get_url_for_host("google.com")},
      {"abc.com", 3, 1, "redirect", get_url_for_host("def.com")},
      {"def.com", 4, 2, "block", std::nullopt},
      {"def.com", 5, 1, "redirect", get_url_for_host("xyz.com")},
      {"ghi*", 6, 1, "redirect", get_url_for_host("ghijk.com")},
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

  struct {
    std::string hostname;
    bool expected_main_frame_loaded;
    std::optional<GURL> expected_final_url;
    std::optional<size_t> expected_redirect_chain_length;
  } test_cases[] = {
      // example.com -> yahoo.com -> google.com.
      {"example.com", true, GURL(get_url_for_host("google.com")), 3},
      // yahoo.com -> google.com.
      {"yahoo.com", true, GURL(get_url_for_host("google.com")), 2},
      // abc.com -> def.com (blocked).
      // Note def.com won't be redirected since blocking rules are given
      // priority over redirect rules.
      {"abc.com", false, std::nullopt, std::nullopt},
      // def.com (blocked).
      {"def.com", false, std::nullopt, std::nullopt},
      // ghi.com -> ghijk.com.
      // Though ghijk.com still matches the redirect rule for |ghi*|, it will
      // not redirect to itself.
      {"ghi.com", true, GURL(get_url_for_host("ghijk.com")), 2},
  };

  for (const auto& test_case : test_cases) {
    std::string url = get_url_for_host(test_case.hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.c_str()));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    EXPECT_EQ(test_case.expected_main_frame_loaded,
              WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

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
    return base::NumberToString(num) + ".com";
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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
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
  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname,
                                          "/pages_with_script/index.html");
  };

  GURL google_url = get_url_for_host("google.com");
  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    std::optional<std::string> redirect_url;
  } rules_data[] = {
      {"exa*", 1, 4, "upgradeScheme", std::nullopt},
      {"|http:*yahoo", 2, 100, "redirect", "http://other.com"},
      // Since the test server can only display http requests, redirect all
      // https requests to google.com in the end.
      // TODO(crbug.com/41471360): Add a https test server to display https
      // pages so this redirect rule can be removed.
      {"|https*", 3, 6, "redirect", google_url.spec()},
      {"exact.com", 4, 5, "block", std::nullopt},
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
    // |expected_final_url| is null if the request is expected to be blocked.
    std::optional<GURL> expected_final_url;
  } test_cases[] = {
      {"exact.com", std::nullopt},
      // http://example.com -> https://example.com/ -> http://google.com
      {"example.com", google_url},
      // test_extension_2 should upgrade the scheme for http://yahoo.com
      // despite having no host permissions. Note that this request is not
      // matched with test_extension_1's ruleset as test_extension_2 is
      // installed more recently.
      // http://yahoo.com -> https://yahoo.com/ -> http://google.com
      {"yahoo.com", google_url},
  };

  for (const auto& test_case : test_cases) {
    GURL url = get_url_for_host(test_case.hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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

  const ExtensionId& extension_id = last_loaded_extension_id();
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  Browser* incognito_browser = CreateIncognitoBrowser();

  auto test_enabled_in_incognito = [&](bool expected_enabled_in_incognito) {
    EXPECT_EQ(expected_enabled_in_incognito,
              util::IsIncognitoEnabled(extension_id, profile()));

    // The url should be blocked in normal context.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_FALSE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
    EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());

    // In incognito context, the url should be blocked if the extension is
    // enabled in incognito mode.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));
    EXPECT_EQ(!expected_enabled_in_incognito,
              WasFrameWithScriptLoaded(GetPrimaryMainFrame(incognito_browser)));
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
    // Toggling the incognito mode reloads the extension. Wait for it to reload.
    WaitForExtensionsWithRulesetsCount(0);
    WaitForExtensionsWithRulesetsCount(1);
    test_enabled_in_incognito(true);
  }

  {
    // Disable the extension in incognito mode.
    SCOPED_TRACE("Testing extension after disabling it in incognito");
    util::SetIsIncognitoEnabled(extension_id, profile(), false /*enabled*/);
    // Toggling the incognito mode reloads the extension. Wait for it to reload.
    WaitForExtensionsWithRulesetsCount(0);
    WaitForExtensionsWithRulesetsCount(1);
    test_enabled_in_incognito(false);
  }
}

// Tests the declarativeNetRequestWithHostAccess permission.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, HostAccessPermission) {
  std::vector<TestRule> rules;
  int rule_id = kMinValidID;

  auto get_url = [this](const std::string& host, const std::string& query) {
    std::string path = "/pages_with_script/index.html?q=" + query;

    return embedded_test_server()->GetURL(host, path);
  };

  {
    TestRule rule = CreateMainFrameBlockRule("block");
    rule.id = rule_id++;
    rules.push_back(rule);
  }

  {
    TestRule rule = CreateGenericRule(rule_id++);
    rule.condition->url_filter = "redirect";
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = "redirect";
    rule.action->redirect.emplace();
    rule.action->redirect->url = get_url("final.com", "final").spec();
    rules.push_back(rule);
  }

  {
    TestRule rule = CreateGenericRule(rule_id++);
    rule.condition->url_filter = "|http://*upgrade";
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = "upgradeScheme";
    rules.push_back(rule);
  }

  {
    // Have a rule which redirects upgraded https requests to a special url. It
    // might seem that this can be avoided by using a HTTPS test server but it
    // doesn't seem trivial to listen to the same port on both http and https.
    TestRule rule = CreateGenericRule(rule_id++);
    rule.condition->url_filter = "|https://*upgrade";
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = "redirect";
    rule.action->redirect.emplace();
    rule.action->redirect->url = get_url("https.com", "https").spec();
    rules.push_back(rule);
  }

  set_config_flags(
      ConfigFlag::kConfig_HasDelarativeNetRequestWithHostAccessPermission |
      ConfigFlag::kConfig_OmitDeclarativeNetRequestPermission);
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules(rules, "directory", {"*://allowed.com:*/*"}));

  struct {
    GURL url;
    // nullopt if expected to be blocked.
    std::optional<GURL> expected_final_url;
  } cases[] = {
      {get_url("allowed.com", "block"), std::nullopt},
      {get_url("notallowed.com", "block"), get_url("notallowed.com", "block")},
      {get_url("allowed.com", "redirect"), get_url("final.com", "final")},
      {get_url("notallowed.com", "redirect"),
       get_url("notallowed.com", "redirect")},

      // This should be upgraded first and then match the https->http rule.
      {get_url("allowed.com", "upgrade"), get_url("https.com", "https")},

      {get_url("notallowed.com", "upgrade"),
       get_url("notallowed.com", "upgrade")},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(
        base::StringPrintf("Testing %s", test_case.url.spec().c_str()));

    if (!test_case.expected_final_url) {
      EXPECT_TRUE(IsNavigationBlocked(test_case.url));
      continue;
    }

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_case.url));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    EXPECT_EQ(*test_case.expected_final_url,
              web_contents()->GetLastCommittedURL());
  }
}

#if BUILDFLAG(IS_MAC) && !defined(NDEBUG)
// Times out on mac-debug: https://crbug.com/1159418
#define MAYBE_ChromeURLS DISABLED_ChromeURLS
#else
#define MAYBE_ChromeURLS ChromeURLS
#endif
// Ensure extensions can't intercept chrome:// urls, even after explicitly
// requesting access to <all_urls>.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, MAYBE_ChromeURLS) {
  // Have the extension block all chrome:// urls.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter =
      std::string(content::kChromeUIScheme) + url::kStandardSchemeSeparator;
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  std::vector<const char*> test_urls = {
      chrome::kChromeUIFlagsURL, chrome::kChromeUIAboutURL,
      chrome::kChromeUIExtensionsURL, chrome::kChromeUIVersionURL};

  for (const char* url : test_urls) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  }
}

// Test that a packed extension with a DNR ruleset behaves correctly after
// browser restart.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       PRE_BrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetLoadType());

  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Block all main frame requests to "static.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("static.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}, "extension", {}));
  const ExtensionId& extension_id = last_loaded_extension_id();

  // Add dynamic rule to block main-frame requests to "dynamic.com".
  rule.condition->url_filter = std::string("dynamic.com");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  // Add session rule to block main-frame requests to "session.com".
  rule.condition->url_filter = std::string("session.com");
  ASSERT_NO_FATAL_FAILURE(UpdateSessionRules(extension_id, {}, {rule}));

  CompositeMatcher* composite_matcher =
      ruleset_manager()->GetMatcherForExtension(extension_id);
  ASSERT_TRUE(composite_matcher);
  EXPECT_THAT(
      GetPublicRulesetIDs(*last_loaded_extension(), *composite_matcher),
      UnorderedElementsAre(kDefaultRulesetID, dnr_api::DYNAMIC_RULESET_ID,
                           dnr_api::SESSION_RULESET_ID));

  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "static.com", "/pages_with_script/index.html")));
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "dynamic.com", "/pages_with_script/index.html")));
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "session.com", "/pages_with_script/index.html")));
  EXPECT_FALSE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "unmatched.com", "/pages_with_script/index.html")));
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       BrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetLoadType());

  const Extension* extension = nullptr;
  for (const auto& ext : extension_registry()->enabled_extensions()) {
    if (ext->name() == "extension") {
      extension = ext.get();
      break;
    }
  }
  ASSERT_TRUE(extension);

  // The session-scoped ruleset should not be enabled after the browser
  // restarts. However both the static and dynamic ruleset enabled in the last
  // session should be enabled.
  CompositeMatcher* composite_matcher =
      ruleset_manager()->GetMatcherForExtension(extension->id());
  ASSERT_TRUE(composite_matcher);
  EXPECT_THAT(
      GetPublicRulesetIDs(*extension, *composite_matcher),
      UnorderedElementsAre(kDefaultRulesetID, dnr_api::DYNAMIC_RULESET_ID));

  // Ensure that the DNR extension enabled in previous browser session still
  // correctly blocks network requests.
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "static.com", "/pages_with_script/index.html")));
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "dynamic.com", "/pages_with_script/index.html")));
  EXPECT_FALSE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "session.com", "/pages_with_script/index.html")));
  EXPECT_FALSE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "unmatched.com", "/pages_with_script/index.html")));
}

// Tests than an extension can omit the "declarative_net_request" manifest key
// but can still use dynamic rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, ZeroRulesets) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_OmitDeclarativeNetRequestKey);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRulesets(
      {} /* rulesets */, "extension_directory", {} /* hosts */));
  const ExtensionId& extension_id = last_loaded_extension_id();

  const GURL url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page.html");
  EXPECT_FALSE(IsNavigationBlocked(url));

  // Add dynamic rule to block main-frame requests to "page.html".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("page.html");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  EXPECT_EQ(0u, extensions_with_rulesets_count());
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));
  WaitForExtensionsWithRulesetsCount(1);

  EXPECT_TRUE(IsNavigationBlocked(url));

  DisableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(0);
  EXPECT_FALSE(IsNavigationBlocked(url));

  EnableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(1);
  EXPECT_TRUE(IsNavigationBlocked(url));
}

// Ensure that when an extension blocks a main-frame request using
// declarativeNetRequest, the resultant error page attributes this to an
// extension.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ErrorPageForBlockedMainFrameNavigation) {
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({CreateMainFrameBlockRule("example.com")}));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURLForFilter("example.com")));
  EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());

  std::string body =
      content::EvalJs(web_contents(), "document.body.textContent")
          .ExtractString();

  EXPECT_TRUE(
      base::Contains(body, "This page has been blocked by an extension"));
  EXPECT_TRUE(base::Contains(body, "Try disabling your extensions."));
}

// Test an extension with multiple static rulesets.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, MultipleRulesets) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  const int kNumStaticRulesets = 4;
  const char* kStaticFilterPrefix = "static";
  std::vector<GURL> expected_blocked_urls;
  std::vector<TestRulesetInfo> rulesets;
  std::vector<GURL> expected_allowed_urls;
  for (int i = 0; i < kNumStaticRulesets; ++i) {
    std::vector<TestRule> rules;
    std::string id = kStaticFilterPrefix + base::NumberToString(i);
    rules.push_back(CreateMainFrameBlockRule(id));

    // Enable even indexed rulesets by default.
    bool enabled = i % 2 == 0;
    if (enabled)
      expected_blocked_urls.push_back(GetURLForFilter(id));
    else
      expected_allowed_urls.push_back(GetURLForFilter(id));

    rulesets.emplace_back(id, ToListValue(rules), enabled);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRulesets(
      rulesets, "extension_directory", {} /* hosts */));
  const ExtensionId& extension_id = last_loaded_extension_id();

  // Also add a dynamic rule blocking pages with string "dynamic".
  const char* kDynamicFilter = "dynamic";
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(
      extension_id, {CreateMainFrameBlockRule(kDynamicFilter)}));
  expected_blocked_urls.push_back(GetURLForFilter(kDynamicFilter));

  expected_allowed_urls.push_back(GetURLForFilter("no_such_rule"));

  VerifyNavigations(expected_blocked_urls, expected_allowed_urls);
}

// Ensure that Blink's in-memory cache is cleared on adding/removing rulesets.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, RendererCacheCleared) {
  // Observe requests to RulesetManager to monitor requests to script.js.
  GURL observed_url =
      embedded_test_server()->GetURL("example.com", "/cached/script.js");

  GURL url = embedded_test_server()->GetURL(
      "example.com", "/cached/page_with_cacheable_script.html");

  // With no extension loaded, the request to the script should succeed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

  // NOTE: RulesetMatcher will not see network requests if no rulesets are
  // active.
  bool expect_request_seen =
      base::FeatureList::IsEnabled(
          extensions_features::kForceWebRequestProxyForTest);
  EXPECT_EQ(expect_request_seen,
            base::Contains(ruleset_manager_observer()->GetAndResetRequestSeen(),
                           observed_url));

  // Another request to |url| should not cause a network request for
  // script.js since it will be served by the renderer's in-memory
  // cache.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  EXPECT_FALSE(base::Contains(
      ruleset_manager_observer()->GetAndResetRequestSeen(), observed_url));

  // Now block requests to script.js.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("script.js");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Adding an extension ruleset should have cleared the renderer's in-memory
  // cache. Hence the browser process will observe the request to
  // script.js and block it.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  EXPECT_TRUE(base::Contains(
      ruleset_manager_observer()->GetAndResetRequestSeen(), observed_url));

  // Disable the extension.
  DisableExtension(last_loaded_extension_id());
  WaitForExtensionsWithRulesetsCount(0);

  // Disabling the extension should cause the request to succeed again. The
  // request for the script will again be observed by the browser since it's not
  // in the renderer's in-memory cache.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  EXPECT_EQ(expect_request_seen,
            base::Contains(ruleset_manager_observer()->GetAndResetRequestSeen(),
                           observed_url));
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
  pref_service->SetDict(proxy_config::prefs::kProxy,
                        ProxyConfigDictionary::CreatePacScript(
                            embedded_test_server()->GetURL("/self.pac").spec(),
                            true /* pac_mandatory */));
  // Flush the proxy configuration change over the Mojo pipe to avoid any races.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->FlushProxyConfigMonitorForTesting();

  // Verify that the extension can't intercept the network request.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("http://does.not.resolve.test/pages_with_script/page.html")));
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
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
  const ExtensionId& extension_id_1 = last_loaded_extension_id();

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules_2, "extension_2", {URLPattern::kAllUrlsPattern}));
  const ExtensionId& extension_id_2 = last_loaded_extension_id();

  auto get_manifest_url = [](const ExtensionId& extension_id) {
    return GURL(base::StringPrintf("%s://%s/manifest.json",
                                   extensions::kExtensionScheme,
                                   extension_id.c_str()));
  };

  // Extension 1 should not be able to block the request to its own
  // manifest.json or that of the Extension 2, even with "<all_urls>" host
  // permissions.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           get_manifest_url(extension_id_1)));
  GURL final_url = web_contents()->GetLastCommittedURL();
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           get_manifest_url(extension_id_2)));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
}

// Ensures that any <img> elements blocked by the API are collapsed.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, ImageCollapsed) {
  // Loads a page with an image and returns whether the image was collapsed.
  auto is_image_collapsed = [this]() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("google.com", "/image.html")));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    const std::string script = "!!window.imageCollapsed;";
    return content::EvalJs(web_contents(), script).ExtractBool();
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
  const std::string kFrameName1 = "frame1";
  const std::string kFrameName2 = "frame2";
  const GURL page_url = embedded_test_server()->GetURL(
      "google.com", "/page_with_two_frames.html");

  // Load a page with two iframes (|kFrameName1| and |kFrameName2|). Initially
  // both the frames should be loaded successfully.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  {
    SCOPED_TRACE("No extension loaded");
    TestFrameCollapse(kFrameName1, false);
    TestFrameCollapse(kFrameName2, false);
  }

  // Now load an extension which blocks all requests with "frame=1" in its url.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("frame=1");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Reloading the page should cause |kFrameName1| to be collapsed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  {
    SCOPED_TRACE("Extension loaded initial");
    TestFrameCollapse(kFrameName1, true);
    TestFrameCollapse(kFrameName2, false);
  }

  // Now interchange the "src" of the two frames. This should cause
  // |kFrameName2| to be collapsed and |kFrameName1| to be un-collapsed.
  GURL frame_url_1 = GetFrameByName(kFrameName1)->GetLastCommittedURL();
  GURL frame_url_2 = GetFrameByName(kFrameName2)->GetLastCommittedURL();
  NavigateFrame(kFrameName1, frame_url_2);
  NavigateFrame(kFrameName2, frame_url_1);
  {
    SCOPED_TRACE("Extension loaded src swapped");
    TestFrameCollapse(kFrameName1, false);
    TestFrameCollapse(kFrameName2, true);
  }

  // Remove the frames from the DOM, swap the "src" of the frames,
  // then add them back to the DOM. `kFrameName1` should be blocked
  // and therefore collapsed.
  RemoveNavigateAndReAddFrame(kFrameName1, frame_url_1);
  RemoveNavigateAndReAddFrame(kFrameName2, frame_url_2);
  {
    SCOPED_TRACE("Removed src-swapped and readded to DOM");
    TestFrameCollapse(kFrameName1, true);
    TestFrameCollapse(kFrameName2, false);
  }

  // Remove the frames from the DOM again, but this time add them back
  // without changing their "src". `kFrameName1` should still be
  // collapsed.
  RemoveNavigateAndReAddFrame(kFrameName1, frame_url_1);
  RemoveNavigateAndReAddFrame(kFrameName2, frame_url_2);
  {
    SCOPED_TRACE("Removed and readded to DOM");
    TestFrameCollapse(kFrameName1, true);
    TestFrameCollapse(kFrameName2, false);
  }
}

// Tests that we correctly reindex a corrupted ruleset. This is only tested for
// packed extensions, since the JSON ruleset is reindexed on each extension
// load for unpacked extensions, so corruption is not an issue.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       CorruptedIndexedRuleset) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Load an extension which blocks all main-frame requests to "google.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("||google.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  const ExtensionId& extension_id = last_loaded_extension_id();

  // Add a dynamic rule to block main-frame requests to "example.com".
  rule.condition->url_filter = std::string("||example.com");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  const GURL static_rule_url = embedded_test_server()->GetURL(
      "google.com", "/pages_with_script/index.html");
  const GURL dynamic_rule_url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html");
  const GURL unblocked_url = embedded_test_server()->GetURL(
      "yahoo.com", "/pages_with_script/index.html");

  const Extension* extension = last_loaded_extension();
  std::vector<FileBackedRulesetSource> static_sources =
      FileBackedRulesetSource::CreateStatic(
          *extension, FileBackedRulesetSource::RulesetFilter::kIncludeAll);
  ASSERT_EQ(1u, static_sources.size());
  FileBackedRulesetSource dynamic_source =
      FileBackedRulesetSource::CreateDynamic(profile(), extension->id());

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
        ASSERT_TRUE(base::WriteFile(indexed_path, corrupted_data));
      };

  // Helper to reload the extension and ensure it is working.
  auto test_extension_works_after_reload = [&]() {
    EXPECT_EQ(1u, extensions_with_rulesets_count());
    DisableExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(0);

    EnableExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(1);

    EXPECT_TRUE(IsNavigationBlocked(static_rule_url));
    EXPECT_TRUE(IsNavigationBlocked(dynamic_rule_url));
    EXPECT_FALSE(IsNavigationBlocked(unblocked_url));
  };

  const char* kReindexHistogram =
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful";

  // Test static ruleset re-indexing.
  {
    SCOPED_TRACE("Static ruleset corruption");
    corrupt_file_for_checksum_mismatch(static_sources[0].indexed_path());

    base::HistogramTester tester;
    test_extension_works_after_reload();

    // Loading the static ruleset would fail initially due to checksum mismatch
    // but will succeed on re-indexing.
    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 1 /*count*/);
    // Count of 2 because we load both static and dynamic rulesets.
    tester.ExpectBucketCount(kLoadRulesetResultHistogram,
                             LoadRulesetResult::kSuccess /* sample */,
                             2 /* count */);
  }

  // Test dynamic ruleset re-indexing.
  {
    SCOPED_TRACE("Dynamic ruleset corruption");
    corrupt_file_for_checksum_mismatch(dynamic_source.indexed_path());

    base::HistogramTester tester;
    test_extension_works_after_reload();

    // Loading the dynamic ruleset would have failed initially due to checksum
    // mismatch and later succeeded on re-indexing.
    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 1 /*count*/);

    // Count of 2 because we load both static and dynamic rulesets.
    tester.ExpectBucketCount(kLoadRulesetResultHistogram,
                             LoadRulesetResult::kSuccess /* sample */,
                             2 /* count */);
  }

  // Go crazy and corrupt both static and dynamic rulesets.
  {
    SCOPED_TRACE("Static and dynamic ruleset corruption");
    corrupt_file_for_checksum_mismatch(dynamic_source.indexed_path());
    corrupt_file_for_checksum_mismatch(static_sources[0].indexed_path());

    base::HistogramTester tester;
    test_extension_works_after_reload();

    // Loading the ruleset would have failed initially due to checksum mismatch
    // and later succeeded.
    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 2 /*count*/);
    // Count of 2 because we load both static and dynamic rulesets.
    tester.ExpectBucketCount(kLoadRulesetResultHistogram,
                             LoadRulesetResult::kSuccess /* sample */,
                             2 /* count */);
  }
}

// Tests that we surface a warning to the user if any of its ruleset fail to
// load.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       WarningOnFailedRulesetLoad) {
  const size_t kNumStaticRulesets = 4;
  const char* kStaticFilterPrefix = "static";
  std::vector<TestRulesetInfo> rulesets;
  std::vector<GURL> urls_for_indices;
  for (size_t i = 0; i < kNumStaticRulesets; ++i) {
    std::vector<TestRule> rules;
    std::string id = kStaticFilterPrefix + base::NumberToString(i);
    rules.push_back(CreateMainFrameBlockRule(id));

    urls_for_indices.push_back(GetURLForFilter(id));

    rulesets.emplace_back(id, ToListValue(rules));
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRulesets(
      rulesets, "extension_directory", {} /* hosts */));

  const ExtensionId& extension_id = last_loaded_extension_id();
  EXPECT_TRUE(ruleset_manager()->GetMatcherForExtension(extension_id));

  const Extension* extension = last_loaded_extension();
  ASSERT_TRUE(extension);

  std::vector<FileBackedRulesetSource> sources =
      FileBackedRulesetSource::CreateStatic(
          *extension, FileBackedRulesetSource::RulesetFilter::kIncludeAll);
  ASSERT_EQ(kNumStaticRulesets, sources.size());

  // Mimic extension prefs corruption by overwriting the indexed ruleset
  // checksum for the first, third and fourth rulesets.
  const int kInvalidRulesetChecksum = -1;
  std::vector<int> corrupted_ruleset_indices = {0, 2, 3};
  std::vector<int> non_corrupted_ruleset_indices = {1};

  PrefsHelper helper(*ExtensionPrefs::Get(profile()));
  for (int index : corrupted_ruleset_indices) {
    helper.SetStaticRulesetChecksum(extension_id, sources[index].id(),
                                    kInvalidRulesetChecksum);
  }

  DisableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(0);
  EXPECT_FALSE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // Reindexing and loading corrupted rulesets should fail now. This should
  // cause a warning.
  base::HistogramTester tester;
  WarningService* warning_service = WarningService::Get(profile());
  WarningServiceObserver warning_observer(warning_service, extension_id);
  EnableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(1);
  EXPECT_TRUE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // Wait till we surface a warning.
  warning_observer.WaitForWarning();
  EXPECT_THAT(warning_service->GetWarningTypesAffectingExtension(extension_id),
              ::testing::ElementsAre(Warning::kRulesetFailedToLoad));

  // Verify that loading the corrupted rulesets failed due to checksum mismatch.
  // The non-corrupted rulesets should load fine.
  tester.ExpectTotalCount(kLoadRulesetResultHistogram, rulesets.size());
  EXPECT_EQ(corrupted_ruleset_indices.size(),
            static_cast<size_t>(tester.GetBucketCount(
                kLoadRulesetResultHistogram,
                LoadRulesetResult::kErrorChecksumMismatch /*sample*/)));
  EXPECT_EQ(non_corrupted_ruleset_indices.size(),
            static_cast<size_t>(
                tester.GetBucketCount(kLoadRulesetResultHistogram,
                                      LoadRulesetResult::kSuccess /*sample*/)));

  // Verify that re-indexing the corrupted rulesets failed.
  tester.ExpectUniqueSample(
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
      false /*sample*/, corrupted_ruleset_indices.size() /*count*/);

  // Finally verify that the non-corrupted rulesets still work fine, but others
  // don't.
  std::vector<GURL> expected_blocked_urls;
  for (int index : non_corrupted_ruleset_indices)
    expected_blocked_urls.push_back(urls_for_indices[index]);

  std::vector<GURL> expected_allowed_urls;
  for (int index : corrupted_ruleset_indices)
    expected_allowed_urls.push_back(urls_for_indices[index]);

  VerifyNavigations(expected_blocked_urls, expected_allowed_urls);
}

// Tests that we reindex the extension rulesets in case its ruleset format
// version is not the same as one used by Chrome.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       ReindexOnRulesetVersionMismatch) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");

  const int kNumStaticRulesets = 4;
  std::vector<TestRulesetInfo> rulesets;
  for (int i = 0; i < kNumStaticRulesets; ++i)
    rulesets.emplace_back(base::NumberToString(i), ToListValue({rule}));

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRulesets(
      rulesets, "extension_directory", {} /* hosts */));

  const ExtensionId& extension_id = last_loaded_extension_id();
  EXPECT_TRUE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // Add a dynamic rule.
  AddDynamicRules(extension_id, {rule});

  DisableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(0);
  EXPECT_FALSE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // Now change the current indexed ruleset format version. This should cause a
  // version mismatch when the extension is loaded again, but re-indexing should
  // still succeed.
  ScopedIncrementRulesetVersion scoped_version_change =
      CreateScopedIncrementRulesetVersionForTesting();

  // Also override the checksum value for the indexed ruleset to simulate a
  // flatbuffer schema change. This will ensure that the checksum of the
  // re-indexed file differs from the current checksum.
  const int kTestChecksum = 100;
  OverrideGetChecksumForTest(kTestChecksum);

  base::HistogramTester tester;
  EnableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(1);
  EXPECT_TRUE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // We add 1 to include the dynamic ruleset.
  const int kNumRulesets = kNumStaticRulesets + 1;

  // Verify that loading the static and dynamic rulesets would cause reindexing
  // due to version header mismatch and later succeeded.
  tester.ExpectUniqueSample(
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
      true /*sample*/, kNumRulesets /*count*/);
  EXPECT_EQ(kNumRulesets,
            tester.GetBucketCount(kLoadRulesetResultHistogram,
                                  LoadRulesetResult::kSuccess /*sample*/));

  // Ensure that the new checksum was correctly persisted in prefs.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  const Extension* extension = last_loaded_extension();
  std::vector<FileBackedRulesetSource> static_sources =
      FileBackedRulesetSource::CreateStatic(
          *extension, FileBackedRulesetSource::RulesetFilter::kIncludeAll);
  ASSERT_EQ(static_cast<size_t>(kNumStaticRulesets), static_sources.size());

  int checksum = kTestChecksum + 1;

  PrefsHelper helper(*prefs);
  for (const FileBackedRulesetSource& source : static_sources) {
    EXPECT_TRUE(
        helper.GetStaticRulesetChecksum(extension_id, source.id(), checksum));
    EXPECT_EQ(kTestChecksum, checksum);

    // Reset checksum for the next test.
    checksum = kTestChecksum + 1;
  }

  EXPECT_TRUE(helper.GetDynamicRulesetChecksum(extension_id, checksum));
  EXPECT_EQ(kTestChecksum, checksum);
}

// Tests that static ruleset preferences are deleted on uninstall for an edge
// case where ruleset loading is completed after extension uninstallation.
// Regression test for crbug.com/1067441.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       RulesetPrefsDeletedOnUninstall) {
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({} /* rules */));

  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* extension = last_loaded_extension();

  std::vector<FileBackedRulesetSource> static_sources =
      FileBackedRulesetSource::CreateStatic(
          *extension, FileBackedRulesetSource::RulesetFilter::kIncludeAll);
  ASSERT_EQ(1u, static_sources.size());

  DisableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(0);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  PrefsHelper helper(*prefs);
  int checksum = -1;
  EXPECT_TRUE(helper.GetStaticRulesetChecksum(
      extension_id, static_sources[0].id(), checksum));

  // Now change the current indexed ruleset format version. This should cause a
  // version mismatch when the extension is loaded again, but re-indexing should
  // still succeed.
  ScopedIncrementRulesetVersion scoped_version_change =
      CreateScopedIncrementRulesetVersionForTesting();

  // Also override the checksum value for the indexed ruleset to simulate a
  // flatbuffer schema change. This will ensure that the checksum of the
  // re-indexed file differs from the current checksum.
  const int kTestChecksum = checksum + 1;
  OverrideGetChecksumForTest(kTestChecksum);

  base::HistogramTester tester;
  RulesetLoadObserver load_observer(rules_monitor_service(), extension_id);

  // Now enable the extension, causing the asynchronous extension ruleset load
  // which further results in an asynchronous re-indexing task. Immediately
  // uninstall the extension to ensure that the uninstallation precedes
  // completion of ruleset load.
  EnableExtension(extension_id);
  UninstallExtension(extension_id);

  load_observer.Wait();

  // Verify that reindexing succeeded.
  tester.ExpectUniqueSample(
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
      true /*sample*/, 1 /*count*/);

  // Verify that the prefs for the static ruleset were deleted successfully.
  EXPECT_FALSE(helper.GetStaticRulesetChecksum(
      extension_id, static_sources[0].id(), checksum));
}

// Tests that redirecting requests using the declarativeNetRequest API works
// with runtime host permissions.
// Disabled due to flakes across all desktop platforms; see
// https://crbug.com/1274533
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       DISABLED_WithheldPermissions_Redirect) {
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

  const Extension* extension = last_loaded_extension();
  ASSERT_TRUE(extension);

  auto verify_script_redirected = [this, extension](
                                      const GURL& page_url,
                                      bool expect_script_redirected,
                                      int expected_blocked_actions) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

    // The page should have loaded correctly.
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    EXPECT_EQ(expect_script_redirected,
              WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

    // The EmbeddedTestServer sees requests after the hostname has been
    // resolved.
    const GURL requested_script_url =
        embedded_test_server()->GetURL("/subresources/not_a_valid_script.js");
    const GURL redirected_script_url =
        embedded_test_server()->GetURL("/subresources/script.js");

    std::map<GURL, net::test_server::HttpRequest> seen_requests =
        GetAndResetRequestsToServer();
    EXPECT_EQ(!expect_script_redirected,
              base::Contains(seen_requests, requested_script_url));
    EXPECT_EQ(expect_script_redirected,
              base::Contains(seen_requests, redirected_script_url));

    ExtensionActionRunner* runner =
        ExtensionActionRunner::GetForWebContents(web_contents());
    ASSERT_TRUE(runner);
    EXPECT_EQ(expected_blocked_actions,
              runner->GetBlockedActions(extension->id()));
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

  const Extension* extension = last_loaded_extension();
  ASSERT_TRUE(extension);

  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .HasEffectiveAccessToAllHosts());

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/pages_with_script/index.html")));
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

  // Sanity check that the script.js request is not blocked if does not match a
  // rule.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/pages_with_script/index.html")));
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
}

// Tests the dynamic rule support.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, DynamicRules) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), example_url));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
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

  TestRule rule4 = CreateGenericRule();
  rule4.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule4.id = kMinValidID + 3;
  rule4.condition->url_filter.reset();
  rule4.condition->regex_filter = R"(^(.+?)://(abc|def)\.exy\.com(.*)$)";
  rule4.action->type = std::string("redirect");
  rule4.priority = kMinValidPriority + 1;
  rule4.action->redirect.emplace();
  rule4.action->redirect->regex_substitution = R"(\1://www.\2.com\3)";

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({rule1, rule2, rule3, rule4}, "test_extension",
                             {URLPattern::kAllUrlsPattern}));

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
                    "google.com", "/pages_with_script/index.html")},
               // Because of a priority higher than |rule1|, |rule4| is chosen.
               {embedded_test_server()->GetURL("abc.exy.com",
                                               "/pages_with_script/page.html"),
                embedded_test_server()->GetURL("www.abc.com",
                                               "/pages_with_script/page.html")},
               {embedded_test_server()->GetURL("xyz.exy.com",
                                               "/pages_with_script/page.html"),
                embedded_test_server()->GetURL(
                    "google.com", "/pages_with_script/index.html")}};

  for (const auto& test_case : cases) {
    SCOPED_TRACE("Testing " + test_case.url.spec());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_case.url));
    EXPECT_EQ(test_case.expected_url, web_contents()->GetLastCommittedURL());
  }
}

// Test that the badge text for an extension will update to reflect the number
// of actions taken on requests matching the extension's ruleset.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionsMatchedCountAsBadgeText) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

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
    std::optional<std::string> redirect_url;
    std::optional<std::vector<TestHeaderInfo>> request_headers;
  } rules_data[] = {
      {"abc.com", 1, 1, "block", std::nullopt, std::nullopt},
      {"def.com", 2, 1, "redirect", "http://zzz.com", std::nullopt},
      {"jkl.com", 3, 1, "modifyHeaders", std::nullopt,
       std::vector<TestHeaderInfo>(
           {TestHeaderInfo("referer", "remove", std::nullopt)})},
      {"abcd.com", 4, 1, "block", std::nullopt, std::nullopt},
      {"abcd", 5, 1, "allow", std::nullopt, std::nullopt},
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
    rule.action->request_headers = rule_data.request_headers;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* dnr_extension = last_loaded_extension();

  PrefsHelper helper(*ExtensionPrefs::Get(profile()));
  helper.SetUseActionCountAsBadgeText(extension_id, true);

  ExtensionAction* action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  struct {
    std::string frame_hostname;
    std::string expected_badge_text;
    bool has_referrer_header;
  } test_cases[] = {
      // zzz.com does not match any rules, so no badge text should be displayed.
      {"zzz.com", "", false},
      // abc.com is blocked by a matching rule and should increment the badge
      // text.
      {"abc.com", "1", false},
      // def.com is redirected by a matching rule and should increment the badge
      // text.
      {"def.com", "2", false},
      // jkl.com matches with a modifyHeaders rule which removes the referer
      // header, but has no headers. Therefore no action is taken and the badge
      // text stays the same.
      {"jkl.com", "2", false},
      // jkl.com matches with a modifyHeaders rule and has a referer header.
      // Therefore the badge text should be incremented.
      {"jkl.com", "3", true},
      // abcd.com matches both a block rule and an allow rule. Since the allow
      // rule overrides the block rule, no action is taken and the badge text
      // stays the same,
      {"abcd.com", "3", false},
  };

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

  // Verify that the badge text is empty when navigation finishes because no
  // actions have been matched.
  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("", action->GetDisplayBadgeText(first_tab_id));

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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  int second_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("", action->GetDisplayBadgeText(second_tab_id));

  // Verify that the badge text for the first tab is unaffected.
  EXPECT_EQ(first_tab_badge_text, action->GetDisplayBadgeText(first_tab_id));

  // Verify that the correct rules are returned via getMatchedRules. Returns "|"
  // separated pairs of <rule_id>,<ruleset_id>, sorted by rule ids, e.g.
  // "ruleId1,rulesetId1|ruleId2,rulesetId2".
  auto get_matched_rules = [this](int tab_id) {
    static constexpr char kGetMatchedRulesScript[] = R"(
      chrome.declarativeNetRequest.getMatchedRules({tabId: %d}, (matches) => {
        // Sort by |ruleId|.
        matches.rulesMatchedInfo.sort((a, b) => a.rule.ruleId - b.rule.ruleId);

        var ruleAndRulesetIDs = matches.rulesMatchedInfo.map(
          match => [match.rule.ruleId, match.rule.rulesetId].join());

        chrome.test.sendScriptResult(ruleAndRulesetIDs.join('|'));
      });
    )";

    return ExecuteScriptInBackgroundPageAndReturnString(
        last_loaded_extension_id(),
        base::StringPrintf(kGetMatchedRulesScript, tab_id),
        browsertest_util::ScriptUserActivation::kDontActivate);
  };

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  // Four rules should be matched on the tab with |first_tab_id|:
  //   - the block rule for abc.com (ruleId = 1)
  //   - the redirect rule for def.com (ruleId = 2)
  //   - the modifyHeaders rule for jkl.com (ruleId = 3)
  //   - the allow rule for abcd.com (ruleId = 5)
  EXPECT_EQ(base::StringPrintf("1,%s|2,%s|3,%s|5,%s", kDefaultRulesetID,
                               kDefaultRulesetID, kDefaultRulesetID,
                               kDefaultRulesetID),
            get_matched_rules(first_tab_id));

  // No rule should be matched on the tab with |second_tab_id|.
  EXPECT_EQ("", get_matched_rules(second_tab_id));
}

// Ensure web request events are still dispatched even if DNR blocks/redirects
// the request. (Regression test for crbug.com/999744).
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, WebRequestEvents) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "||example.com";
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "test_extension", {URLPattern::kAllUrlsPattern}));

  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/index.html");

  // Set up web request listeners listening to request to |url|.
  static constexpr char kWebRequestListenerScript[] = R"(
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

  ExtensionTestMessageListener pass_listener("PASS");
  ExtensionTestMessageListener installed_listener("INSTALLED");
  ExecuteScriptInBackgroundPageNoWait(
      last_loaded_extension_id(),
      base::StringPrintf(kWebRequestListenerScript, url.spec().c_str()));

  // Wait for the web request listeners to be installed before navigating.
  ASSERT_TRUE(installed_listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_FALSE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  EXPECT_TRUE(pass_listener.WaitUntilSatisfied());
}

// Ensure Declarative Net Request gets priority over the web request API.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, WebRequestPriority) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

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
  static constexpr char kWebRequestBlockScript[] = R"(
    let filter = {'urls' : ['%s'], 'types' : ['main_frame']};
    chrome.webRequest.onBeforeRequest.addListener((details) => {
      chrome.test.sendMessage('SEEN')
    }, filter, ['blocking']);
    chrome.test.sendMessage('INSTALLED');
  )";

  ExtensionTestMessageListener seen_listener("SEEN");
  ExtensionTestMessageListener installed_listener("INSTALLED");
  ExecuteScriptInBackgroundPageNoWait(
      last_loaded_extension_id(),
      base::StringPrintf(kWebRequestBlockScript, url.spec().c_str()));

  // Wait for the web request listeners to be installed before navigating.
  ASSERT_TRUE(installed_listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Ensure the response from the web request listener was ignored and the
  // request was redirected.
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), redirect_url);

  // Ensure onBeforeRequest is seen by the web request extension.
  EXPECT_TRUE(seen_listener.WaitUntilSatisfied());
}

// Tests filtering based on the tab ID of the request.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, TabIdFiltering) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  constexpr char kUrlPath[] = "/pages_with_script/index.html";
  GURL url = embedded_test_server()->GetURL("example.com", kUrlPath);
  // Open three tabs to `url`.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  int second_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  ASSERT_NE(first_tab_id, second_tab_id);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(3, tab_strip_model->count());
  int third_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  ASSERT_NE(first_tab_id, third_tab_id);

  int rule_id = kMinValidID;
  constexpr char kHost[] = "foo.com";

  auto create_redirect_rule = [&kHost](int id,
                                       const std::string& redirect_host) {
    TestRule rule = CreateGenericRule(id);
    rule.action->type = "redirect";
    rule.action->redirect.emplace();
    rule.action->redirect->transform.emplace();
    rule.action->redirect->transform->host = redirect_host;
    rule.condition->resource_types =
        std::vector<std::string>({"main_frame", "xmlhttprequest"});
    rule.condition->url_filter = kHost;
    return rule;
  };

  // Add a rule which redirects hosts on all requests on the `first_tab_id` to
  // "rule1.com".
  TestRule rule1 = create_redirect_rule(rule_id++, "rule1.com");
  rule1.condition->tab_ids = std::vector<int>({first_tab_id});

  // Add a rule which redirects hosts on all non-tab specific requests to
  // "rule2.com".
  TestRule rule2 = create_redirect_rule(rule_id++, "rule2.com");
  rule2.condition->tab_ids = std::vector<int>({-1});

  // Add a regex rule which redirects hosts on all requests (except those from
  // the excluded tabs below) to "rule3.com".
  TestRule rule3 = create_redirect_rule(rule_id++, "rule3.com");
  rule3.condition->excluded_tab_ids =
      std::vector<int>({first_tab_id, second_tab_id, -1});
  rule3.condition->url_filter = std::nullopt;
  rule3.condition->regex_filter = std::string(kHost);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {}, "test_extension", {URLPattern::kAllUrlsPattern}));
  ASSERT_NO_FATAL_FAILURE(UpdateSessionRules(last_loaded_extension_id(), {},
                                             {rule1, rule2, rule3}));

  constexpr char kFetchTemplate[] = R"(
    (async function func() {
      let url = '%s';
      const response = await fetch(url);
      return response.ok ? response.url : 'bad response';
    })();
  )";
  constexpr char kFetchPath[] = "/subresources/xhr_target.txt";
  GURL fetch_url = embedded_test_server()->GetURL(kHost, kFetchPath);
  std::string script =
      base::StringPrintf(kFetchTemplate, fetch_url.spec().c_str());

  GURL new_url = embedded_test_server()->GetURL(kHost, kUrlPath);
  struct {
    int expected_tab_id;
    std::string expected_host;
  } cases[] = {
      {first_tab_id, "rule1.com"},
      {second_tab_id, kHost},
      {third_tab_id, "rule3.com"},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing case %zu", i));
    // Navigate the i'th tab to `new_url`.
    tab_strip_model->ActivateTabAt(static_cast<int>(i));
    EXPECT_EQ(cases[i].expected_tab_id,
              ExtensionTabUtil::GetTabId(web_contents()));

    // Verify tab ID filtering works on a main-frame request.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_url));
    EXPECT_EQ(embedded_test_server()->GetURL(cases[i].expected_host, kUrlPath),
              web_contents()->GetLastCommittedURL());

    // Verify tab ID filtering works on a subresource request.
    EXPECT_EQ(
        embedded_test_server()->GetURL(cases[i].expected_host, kFetchPath),
        content::EvalJs(web_contents(), script));
  }

  // Test matching against requests which don't originate from a tab (tab ID of
  // -1) by performing a fetch from a shared worker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(kHost, "/fetch_from_shared_worker.html")));

  // This navigation should have matched the third rule.
  ASSERT_EQ(embedded_test_server()->GetURL("rule3.com",
                                           "/fetch_from_shared_worker.html"),
            web_contents()->GetLastCommittedURL());

  EXPECT_EQ(embedded_test_server()->GetURL("rule2.com", kFetchPath),
            content::EvalJs(web_contents(),
                            base::StringPrintf("fetchFromSharedWorker('%s');",
                                               fetch_url.spec().c_str())));
}

// Test that the extension cannot retrieve the number of actions matched
// from the badge text by calling chrome.browserAction.getBadgeText, unless
// it has the declarativeNetRequestFeedback permission or activeTab is granted
// for the tab.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetBadgeTextForActionsMatched) {
  auto query_badge_text = [this](const ExtensionId& extension_id, int tab_id) {
    static constexpr char kBadgeTextQueryScript[] = R"(
        chrome.browserAction.getBadgeText({tabId: %d}, badgeText => {
          chrome.test.sendScriptResult(badgeText);
        });
      )";

    return ExecuteScriptInBackgroundPageAndReturnString(
        extension_id, base::StringPrintf(kBadgeTextQueryScript, tab_id));
  };

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "def.com";
  rule.id = kMinValidID;
  rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
  rule.action->type = "block";

  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasActiveTab);

  // Create the first extension.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "test_extension", {URLPattern::kAllUrlsPattern}));
  const Extension* extension_1 = last_loaded_extension();
  ExtensionAction* action_1 =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*extension_1);

  TestRule rule_2 = CreateGenericRule();
  rule_2.condition->url_filter = "ghi.com";
  rule_2.id = kMinValidID;
  rule_2.condition->resource_types = std::vector<std::string>({"sub_frame"});
  rule_2.action->type = "block";

  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  // Create the second extension with the feedback permission.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule_2}, "test_extension_2", {URLPattern::kAllUrlsPattern}));
  const Extension* extension_2 = last_loaded_extension();
  ExtensionAction* action_2 =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*extension_2);

  const std::string default_badge_text = "asdf";
  action_1->SetBadgeText(ExtensionAction::kDefaultTabId, default_badge_text);
  action_2->SetBadgeText(ExtensionAction::kDefaultTabId, default_badge_text);

  // Navigate to a page with two frames, no requests should be blocked
  // initially.
  const GURL page_url =
      embedded_test_server()->GetURL("abc.com", "/page_with_two_frames.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents());
  ActiveTabPermissionGranter* active_tab_granter =
      tab_helper->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_granter);

  // The preference is initially turned off. Both the visible badge text and the
  // badge text queried by the extension using getBadgeText() should return the
  // default badge text.
  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ(default_badge_text, action_1->GetDisplayBadgeText(first_tab_id));
  EXPECT_EQ(default_badge_text, action_2->GetDisplayBadgeText(first_tab_id));

  EXPECT_EQ(default_badge_text,
            query_badge_text(extension_1->id(), first_tab_id));
  EXPECT_EQ(default_badge_text,
            query_badge_text(extension_2->id(), first_tab_id));

  EXPECT_EQ(SetExtensionActionOptions(extension_1->id(),
                                      "{displayActionCountAsBadgeText: true}"),
            "success");
  EXPECT_EQ(SetExtensionActionOptions(extension_2->id(),
                                      "{displayActionCountAsBadgeText: true}"),
            "success");

  // After enabling the preference the visible badge text should remain as the
  // default initially, as the action count for the tab is still 0 for both
  // extensions.
  EXPECT_EQ(default_badge_text, action_1->GetDisplayBadgeText(first_tab_id));
  EXPECT_EQ(default_badge_text, action_2->GetDisplayBadgeText(first_tab_id));

  // Since the preference is on for the current tab, attempting to query the
  // badge text should return the placeholder text instead of the matched action
  // count for |extension_1|.
  EXPECT_EQ(declarative_net_request::kActionCountPlaceholderBadgeText,
            query_badge_text(extension_1->id(), first_tab_id));

  // The placeholder should not be returned if the declarativeNetRequestFeedback
  // permission is enabled.
  EXPECT_EQ(default_badge_text,
            query_badge_text(extension_2->id(), first_tab_id));

  // |extension_1| should block this frame navigation.
  NavigateFrame("frame1", embedded_test_server()->GetURL("def.com", "/"));
  // |extension_2| should block this frame navigation.
  NavigateFrame("frame2", embedded_test_server()->GetURL("ghi.com", "/"));

  // One action was matched, and this should be reflected in the badge text.
  EXPECT_EQ("1", action_1->GetDisplayBadgeText(first_tab_id));
  EXPECT_EQ("1", action_2->GetDisplayBadgeText(first_tab_id));

  // The placeholder should still be returned for the first extension, but the
  // second extension with permission should now return the action count.
  EXPECT_EQ(declarative_net_request::kActionCountPlaceholderBadgeText,
            query_badge_text(extension_1->id(), first_tab_id));
  EXPECT_EQ("1", query_badge_text(extension_2->id(), first_tab_id));

  // Tab-specific badge text should take priority over the action count and
  // placeholder text.
  action_1->SetBadgeText(first_tab_id, "text_1");
  action_2->SetBadgeText(first_tab_id, "text_2");
  EXPECT_EQ("text_1", query_badge_text(extension_1->id(), first_tab_id));
  EXPECT_EQ("text_2", query_badge_text(extension_2->id(), first_tab_id));

  action_1->ClearBadgeText(first_tab_id);
  action_2->ClearBadgeText(first_tab_id);

  // Querying the badge text with the activeTab access for the current tab
  // should provide the matched action count.
  active_tab_granter->GrantIfRequested(extension_1);
  EXPECT_EQ("1", query_badge_text(extension_1->id(), first_tab_id));
  // Revoking activeTab should cause the placeholder text to be returned again
  // since we don't have the declarativeNetRequestFeedback permission.
  active_tab_granter->RevokeForTesting();
  EXPECT_EQ(declarative_net_request::kActionCountPlaceholderBadgeText,
            query_badge_text(extension_1->id(), first_tab_id));

  EXPECT_EQ(SetExtensionActionOptions(extension_1->id(),
                                      "{displayActionCountAsBadgeText: false}"),
            "success");
  EXPECT_EQ(SetExtensionActionOptions(extension_2->id(),
                                      "{displayActionCountAsBadgeText: false}"),
            "success");

  // Switching the preference off should cause the extension queried badge text
  // to be the explicitly set badge text for this tab if it exists. In this
  // case, the queried badge text should be the default badge text.
  EXPECT_EQ(default_badge_text,
            query_badge_text(extension_1->id(), first_tab_id));
  EXPECT_EQ(default_badge_text,
            query_badge_text(extension_2->id(), first_tab_id));

  // The displayed badge text should be the default badge text now that the
  // preference is off.
  EXPECT_EQ(default_badge_text, action_1->GetDisplayBadgeText(first_tab_id));
  EXPECT_EQ(default_badge_text, action_2->GetDisplayBadgeText(first_tab_id));

  // Verify that turning off the preference deletes the DNR action count within
  // the extension action.
  EXPECT_FALSE(action_1->HasDNRActionCount(first_tab_id));
  EXPECT_FALSE(action_2->HasDNRActionCount(first_tab_id));
}

// Test that enabling the "displayActionCountAsBadgeText" preference using
// setExtensionActionOptions will update all browsers sharing the same browser
// context.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionCountPreferenceMultipleWindows) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "abc.com";
  rule.id = kMinValidID;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  std::vector<TestRule> rules({rule});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rules}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* dnr_extension = last_loaded_extension();

  ExtensionAction* extension_action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  const GURL page_url = embedded_test_server()->GetURL(
      "abc.com", "/pages_with_script/index.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  int first_browser_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("", extension_action->GetDisplayBadgeText(first_browser_tab_id));

  // Now create a new browser with the same profile as |browser()| and navigate
  // to |page_url|.
  Browser* second_browser = CreateBrowser(profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(second_browser, page_url));
  content::WebContents* second_browser_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();

  int second_browser_tab_id =
      ExtensionTabUtil::GetTabId(second_browser_contents);
  EXPECT_EQ("", extension_action->GetDisplayBadgeText(second_browser_tab_id));

  // Set up an observer to listen for ExtensionAction updates for the active web
  // contents of both browser windows.
  TestExtensionActionAPIObserver test_api_observer(
      profile(), extension_id, {web_contents(), second_browser_contents});

  EXPECT_EQ(SetExtensionActionOptions(extension_id,
                                      "{displayActionCountAsBadgeText: true}"),
            "success");

  // Wait until ExtensionActionAPI::NotifyChange is called, then perform a
  // sanity check that one action was matched, and this is reflected in the
  // badge text.
  test_api_observer.Wait();

  EXPECT_EQ("1", extension_action->GetDisplayBadgeText(first_browser_tab_id));

  // The badge text for the second browser window should also update to the
  // matched action count because the second browser shares the same browser
  // context as the first.
  EXPECT_EQ("1", extension_action->GetDisplayBadgeText(second_browser_tab_id));
}

// Test that the action matched badge text for an extension is visible in an
// incognito context if the extension is incognito enabled.
// Test is disabled on Mac. See https://crbug.com/1280116
#if BUILDFLAG(IS_MAC)
#define MAYBE_ActionsMatchedCountAsBadgeTextIncognito \
  DISABLED_ActionsMatchedCountAsBadgeTextIncognito
#else
#define MAYBE_ActionsMatchedCountAsBadgeTextIncognito \
  ActionsMatchedCountAsBadgeTextIncognito
#endif
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       MAYBE_ActionsMatchedCountAsBadgeTextIncognito) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "abc.com";
  rule.id = kMinValidID;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  std::vector<TestRule> rules({rule});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rules}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_id = last_loaded_extension_id();
  util::SetIsIncognitoEnabled(extension_id, profile(), true /*enabled*/);

  PrefsHelper helper(*ExtensionPrefs::Get(profile()));
  helper.SetUseActionCountAsBadgeText(extension_id, true);

  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(incognito_browser, GURL("http://abc.com")));

  content::WebContents* incognito_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  const Extension* dnr_extension = last_loaded_extension();
  ExtensionAction* incognito_action =
      ExtensionActionManager::Get(incognito_contents->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  EXPECT_EQ("1", incognito_action->GetDisplayBadgeText(
                     ExtensionTabUtil::GetTabId(incognito_contents)));
}

// Test that the actions matched badge text for an extension will be reset
// when a main-frame navigation finishes.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       // TODO(crbug.com/40843749): Re-enable this test
                       DISABLED_ActionsMatchedCountAsBadgeTextMainFrame) {
  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname,
                                          "/pages_with_script/index.html");
  };

  auto get_set_cookie_url = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname, "/set-cookie?a=b");
  };

  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    std::vector<std::string> resource_types;
    std::optional<std::string> redirect_url;
    std::optional<std::vector<TestHeaderInfo>> response_headers;
  } rules_data[] = {
      {"abc.com", 1, 1, "block", std::vector<std::string>({"script"}),
       std::nullopt, std::nullopt},
      {"||def.com", 2, 1, "redirect", std::vector<std::string>({"main_frame"}),
       get_url_for_host("abc.com").spec(), std::nullopt},
      {"gotodef.com", 3, 1, "redirect",
       std::vector<std::string>({"main_frame"}),
       get_url_for_host("def.com").spec(), std::nullopt},
      {"ghi.com", 4, 1, "block", std::vector<std::string>({"main_frame"}),
       std::nullopt, std::nullopt},
      {"gotosetcookie.com", 5, 1, "redirect",
       std::vector<std::string>({"main_frame"}),
       get_set_cookie_url("setcookie.com").spec(), std::nullopt},
      {"setcookie.com", 6, 1, "modifyHeaders",
       std::vector<std::string>({"main_frame"}), std::nullopt,
       std::vector<TestHeaderInfo>(
           {TestHeaderInfo("set-cookie", "remove", std::nullopt)})},
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
    rule.action->response_headers = rule_data.response_headers;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  const Extension* dnr_extension = last_loaded_extension();

  PrefsHelper helper(*ExtensionPrefs::Get(profile()));
  helper.SetUseActionCountAsBadgeText(last_loaded_extension_id(), true);

  ExtensionAction* action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  struct {
    std::string frame_hostname;
    std::string expected_badge_text;
  } test_cases[] = {
      // The request to ghi.com should be blocked, so the badge text should be 1
      // once navigation finishes.
      {"ghi.com", "1"},
      // The script on get_url_for_host("abc.com") matches with a rule and
      // should increment the badge text.
      {"abc.com", "1"},
      // No rules match, so there should be no badge text once navigation
      // finishes.
      {"nomatch.com", ""},
      // The request to def.com will redirect to get_url_for_host("abc.com") and
      // the script on abc.com should match with a rule.
      {"def.com", "2"},
      // Same as the above test, except with an additional redirect from
      // gotodef.com to def.com caused by a rule match. Therefore the badge text
      // should be 3.
      {"gotodef.com", "3"},
      // The request to gotosetcookie.com will match with a rule and redirect to
      // setcookie.com. The Set-Cookie header on setcookie.com will also match
      // with a rule and get removed. Therefore the badge text should be 2.
      {"gotosetcookie.com", "2"},
  };

  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  for (const auto& test_case : test_cases) {
    GURL url = get_url_for_host(test_case.frame_hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(test_case.expected_badge_text,
              action->GetDisplayBadgeText(first_tab_id));
  }
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ModifyHeadersBadgeText) {
  auto get_referer_url = [this](const std::string& host) {
    return embedded_test_server()->GetURL(host, "/set-header?referer: none");
  };
  auto get_set_cookie_url = [this](const std::string& host) {
    return embedded_test_server()->GetURL(host, "/set-cookie?a=b");
  };
  auto get_no_headers_url = [this](const std::string& host) {
    return embedded_test_server()->GetURL(host,
                                          "/pages_with_script/index.html");
  };

  const std::string kFrameName1 = "frame1";
  const GURL page_url = embedded_test_server()->GetURL(
      "nomatch.com", "/page_with_two_frames.html");

  // Create an extension with rules and get the ExtensionAction for it.
  TestRule example_set_cookie_rule = CreateModifyHeadersRule(
      kMinValidID, kMinValidPriority, "example.com", std::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("set-cookie", "remove", std::nullopt)}));

  TestRule both_headers_rule = CreateModifyHeadersRule(
      kMinValidID + 1, kMinValidPriority, "google.com",
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("referer", "remove", std::nullopt)}),
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("set-cookie", "remove", std::nullopt)}));

  TestRule abc_set_cookie_rule = CreateModifyHeadersRule(
      kMinValidID + 2, kMinValidPriority, "abc.com", std::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("set-cookie", "remove", std::nullopt)}));

  TestRule abc_referer_rule = CreateModifyHeadersRule(
      kMinValidID + 3, kMinValidPriority, "abc.com",
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("referer", "remove", std::nullopt)}),
      std::nullopt);

  TestRule ext1_set_custom_request_header_rule = CreateModifyHeadersRule(
      kMinValidID + 4, kMinValidPriority, "def.com",
      std::vector<TestHeaderInfo>({TestHeaderInfo("header1", "set", "ext_1")}),
      std::nullopt);

  TestRule ext1_add_custom_response_header_rule = CreateModifyHeadersRule(
      kMinValidID + 5, kMinValidPriority, "ghi.com", std::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header2", "append", "ext_1")}));

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {example_set_cookie_rule, both_headers_rule, abc_set_cookie_rule,
       abc_referer_rule, ext1_set_custom_request_header_rule,
       ext1_add_custom_response_header_rule},
      "extension_1", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_1_id = last_loaded_extension_id();
  PrefsHelper helper(*ExtensionPrefs::Get(profile()));
  helper.SetUseActionCountAsBadgeText(extension_1_id, true);

  ExtensionAction* extension_1_action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(
              *extension_registry()->enabled_extensions().GetByID(
                  extension_1_id));

  // Create another extension which removes the referer header from example.com
  // and get the ExtensionAction for it.
  TestRule example_referer_rule = CreateModifyHeadersRule(
      kMinValidID, kMinValidPriority, "example.com",
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("referer", "remove", std::nullopt)}),
      std::nullopt);

  TestRule ext2_set_custom_request_header_rule = CreateModifyHeadersRule(
      kMinValidID + 4, kMinValidPriority, "def.com",
      std::vector<TestHeaderInfo>({TestHeaderInfo("header1", "set", "ext_2")}),
      std::nullopt);

  TestRule ext2_add_custom_response_header_rule = CreateModifyHeadersRule(
      kMinValidID + 5, kMinValidPriority, "ghi.com", std::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header2", "append", "ext_2")}));

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {example_referer_rule, ext2_set_custom_request_header_rule,
       ext2_add_custom_response_header_rule},
      "extension_2", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_2_id = last_loaded_extension_id();
  helper.SetUseActionCountAsBadgeText(extension_2_id, true);

  ExtensionAction* extension_2_action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(
              *extension_registry()->enabled_extensions().GetByID(
                  extension_2_id));

  struct {
    GURL url;
    bool use_referrer;
    std::string expected_ext_1_badge_text;
    std::string expected_ext_2_badge_text;
  } test_cases[] = {
      // This request only has a Set-Cookie header. Only the badge text for the
      // extension with a remove Set-Cookie header rule should be incremented.
      {get_set_cookie_url("example.com"), false, "1", ""},
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
      // This request without headers matches rules to set the header1 request
      // header from both extensions. Since |extension_2| was installed later
      // than |extension_1|, only the rule from |extension_2| should take effect
      // and so the action count for |extension_2| should increment by one.
      {get_no_headers_url("def.com"), false, "5", "3"},
      // This request without headers matches rules to append the header2
      // response header from both extensions. Since each extension has a rule
      // which has taken effect, the action count for both extensions should
      // increment by one.
      {get_no_headers_url("ghi.com"), false, "6", "4"},
  };

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("", extension_1_action->GetDisplayBadgeText(first_tab_id));
  EXPECT_EQ("", extension_2_action->GetDisplayBadgeText(first_tab_id));

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

// Test that the setExtensionActionOptions tabUpdate option works correctly.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionsMatchedCountAsBadgeTextTabUpdate) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Set up an extension with a blocking rule.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("||abc.com");
  rule.condition->resource_types = std::vector<std::string>({"sub_frame"});

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "extension", {URLPattern::kAllUrlsPattern}));
  const Extension* extension = last_loaded_extension();
  ExtensionAction* action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*extension);

  const std::string default_badge_text = "asdf";
  action->SetBadgeText(ExtensionAction::kDefaultTabId, default_badge_text);

  // Display action count as badge text for |extension|.
  EXPECT_EQ(SetExtensionActionOptions(extension->id(),
                                      "{displayActionCountAsBadgeText: true}"),
            "success");

  // Navigate to a page with two frames, the same-origin one should be blocked.
  const GURL page_url =
      embedded_test_server()->GetURL("abc.com", "/page_with_two_frames.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  int tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Verify that the initial badge text reflects that the same-origin frame was
  // blocked.
  EXPECT_EQ("1", action->GetDisplayBadgeText(tab_id));

  // Increment the action count.
  EXPECT_EQ(SetExtensionActionOptions(
                extension->id(),
                base::StringPrintf("{tabUpdate: {tabId: %d, increment: 10}}",
                                   tab_id)),
            "success");
  EXPECT_EQ("11", action->GetDisplayBadgeText(tab_id));

  // An increment of 0 is ignored.
  EXPECT_EQ(
      SetExtensionActionOptions(
          extension->id(),
          base::StringPrintf("{tabUpdate: {tabId: %d, increment: 0}}", tab_id)),
      "success");
  EXPECT_EQ("11", action->GetDisplayBadgeText(tab_id));

  // If the tab doesn't exist an error should be shown.
  EXPECT_EQ(
      SetExtensionActionOptions(
          extension->id(),
          base::StringPrintf("{tabUpdate: {tabId: %d, increment: 10}}", 999)),
      ErrorUtils::FormatErrorMessage(declarative_net_request::kTabNotFoundError,
                                     "999"));
  EXPECT_EQ("11", action->GetDisplayBadgeText(tab_id));

  // The action count should continue to increment when an action is taken.
  NavigateFrame("frame1", page_url);
  EXPECT_EQ("12", action->GetDisplayBadgeText(tab_id));

  // The action count can be decremented.
  EXPECT_EQ(SetExtensionActionOptions(
                extension->id(),
                base::StringPrintf("{tabUpdate: {tabId: %d, increment: -5}}",
                                   tab_id)),
            "success");
  EXPECT_EQ("7", action->GetDisplayBadgeText(tab_id));

  // Check that the action count cannot be decremented below 0. We fallback to
  // displaying the default badge text when the action count is 0.
  EXPECT_EQ(SetExtensionActionOptions(
                extension->id(),
                base::StringPrintf("{tabUpdate: {tabId: %d, increment: -10}}",
                                   tab_id)),
            "success");
  EXPECT_EQ(default_badge_text, action->GetDisplayBadgeText(tab_id));
  EXPECT_EQ(SetExtensionActionOptions(
                extension->id(),
                base::StringPrintf("{tabUpdate: {tabId: %d, increment: -1}}",
                                   tab_id)),
            "success");
  EXPECT_EQ(default_badge_text, action->GetDisplayBadgeText(tab_id));
  EXPECT_EQ(
      SetExtensionActionOptions(
          extension->id(),
          base::StringPrintf("{tabUpdate: {tabId: %d, increment: 3}}", tab_id)),
      "success");
  EXPECT_EQ("3", action->GetDisplayBadgeText(tab_id));

  // The action count cannot be incremented if the display action as badge text
  // feature is not enabled.
  EXPECT_EQ(
      SetExtensionActionOptions(
          extension->id(),
          base::StringPrintf("{displayActionCountAsBadgeText: false, "
                             "tabUpdate: {tabId: %d, increment: 10}}",
                             tab_id)),
      declarative_net_request::kIncrementActionCountWithoutUseAsBadgeTextError);
  EXPECT_EQ(default_badge_text, action->GetDisplayBadgeText(tab_id));

  // Any increment to the action count should not be ignored if we're enabling
  // the preference.
  EXPECT_EQ(SetExtensionActionOptions(
                extension->id(),
                base::StringPrintf("{displayActionCountAsBadgeText: true, "
                                   "tabUpdate: {tabId: %d, increment: 5}}",
                                   tab_id)),
            "success");
  EXPECT_EQ("8", action->GetDisplayBadgeText(tab_id));
}

// Test that the onRuleMatchedDebug event is only available for unpacked
// extensions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       OnRuleMatchedDebugAvailability) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it can have access to the onRuleMatchedDebug event when
  // unpacked.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {}, "test_extension", {URLPattern::kAllUrlsPattern}));

  static constexpr char kGetOnRuleMatchedDebugScript[] = R"(
    const hasEvent = !!chrome.declarativeNetRequest.onRuleMatchedDebug ?
      'true' : 'false';
    chrome.test.sendScriptResult(hasEvent);
  )";
  std::string actual_event_availability =
      ExecuteScriptInBackgroundPageAndReturnString(
          last_loaded_extension_id(), kGetOnRuleMatchedDebugScript);

  std::string expected_event_availability =
      GetLoadType() == ExtensionLoadType::UNPACKED ? "true" : "false";

  ASSERT_EQ(expected_event_availability, actual_event_availability);
}

// Test that the onRuleMatchedDebug event returns the correct number of matched
// rules for a request which is matched with multiple rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Unpacked,
                       OnRuleMatchedDebugMultipleRules) {
  // This is only tested for unpacked extensions since the onRuleMatchedDebug
  // event is only available for unpacked extensions.
  ASSERT_EQ(ExtensionLoadType::UNPACKED, GetLoadType());

  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the onRuleMatchedDebug event.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  const std::string kFrameName1 = "frame1";
  const std::string sub_frame_host = "abc.com";
  const GURL page_url = embedded_test_server()->GetURL(
      "nomatch.com", "/page_with_two_frames.html");

  TestRule abc_referer_rule = CreateModifyHeadersRule(
      kMinValidID, kMinValidPriority, sub_frame_host,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("referer", "remove", std::nullopt)}),
      std::nullopt);

  TestRule abc_set_cookie_rule = CreateModifyHeadersRule(
      kMinValidID + 1, kMinValidPriority, sub_frame_host, std::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("set-cookie", "remove", std::nullopt)}));

  // Load an extension with removeHeaders rules for the Referer and Set-Cookie
  // headers.
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({abc_set_cookie_rule, abc_referer_rule},
                             "extension_1", {URLPattern::kAllUrlsPattern}));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

  // Start the onRuleMatchedDebug observer.
  static constexpr char kOnRuleMatchedDebugScript[] = R"(
    var matchedRules = [];
    var onRuleMatchedDebugCallback = (rule) => {
      matchedRules.push(rule);
    };

    chrome.declarativeNetRequest.onRuleMatchedDebug.addListener(
      onRuleMatchedDebugCallback);
    chrome.test.sendScriptResult('ready');
  )";

  ASSERT_EQ("ready", ExecuteScriptInBackgroundPage(last_loaded_extension_id(),
                                                   kOnRuleMatchedDebugScript));

  auto set_cookie_and_referer_url =
      embedded_test_server()->GetURL(sub_frame_host, "/set-cookie?a=b");

  NavigateFrame(kFrameName1, set_cookie_and_referer_url);

  // Now query the onRuleMatchedDebug results.
  static constexpr char kQueryMatchedRulesScript[] = R"(
    chrome.declarativeNetRequest.onRuleMatchedDebug.removeListener(
      onRuleMatchedDebugCallback);
    var ruleIds = matchedRules.map(matchedRule => matchedRule.rule.ruleId);
    chrome.test.sendScriptResult(ruleIds.sort().join());
  )";

  std::string matched_rule_ids = ExecuteScriptInBackgroundPageAndReturnString(
      last_loaded_extension_id(), kQueryMatchedRulesScript);

  // The request to |set_cookie_and_referer_url| should be matched with the
  // Referer rule (ruleId 1) and the Set-Cookie rule (ruleId 2).
  EXPECT_EQ("1,2", matched_rule_ids);
}

// Test that getMatchedRules returns the correct rules when called by different
// extensions with rules matched by requests initiated from different tabs.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesMultipleTabs) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  // Sub-frames are used for navigations instead of the main-frame to allow
  // multiple requests to be made without triggering a main-frame navigation,
  // which would move rules attributed to the previous main-frame to the unknown
  // tab ID.
  const std::string kFrameName1 = "frame1";
  const GURL page_url = embedded_test_server()->GetURL(
      "nomatch.com", "/page_with_two_frames.html");

  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname,
                                          "/pages_with_script/index.html");
  };

  auto create_block_rule = [](int id, const std::string& url_filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter = url_filter;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rule.action->type = "block";
    return rule;
  };

  TestRule abc_rule = create_block_rule(kMinValidID, "abc.com");
  TestRule def_rule = create_block_rule(kMinValidID + 1, "def.com");

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({abc_rule}, "extension_1", {}));
  auto extension_1_id = last_loaded_extension_id();

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({def_rule}, "extension_2", {}));
  auto extension_2_id = last_loaded_extension_id();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  const int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Navigate to abc.com. The rule from |extension_1| should match for
  // |first_tab_id|.
  NavigateFrame(kFrameName1, get_url_for_host("abc.com"));

  // Navigate to abc.com. The rule from |extension_2| should match for
  // |first_tab_id|.
  NavigateFrame(kFrameName1, get_url_for_host("def.com"));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));
  const int second_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Navigate to abc.com from the second tab. The rule from |extension_1| should
  // match for |second_tab_id|.
  NavigateFrame(kFrameName1, get_url_for_host("abc.com"));

  int abc_id = *abc_rule.id;
  int def_id = *def_rule.id;

  struct {
    ExtensionId extension_id;
    std::optional<int> tab_id;
    std::string expected_rule_and_tab_ids;
  } test_cases[] = {
      // No rules should be matched for |extension_1| and the unknown tab ID.
      {extension_1_id, extension_misc::kUnknownTabId, ""},

      // No filter is specified for |extension_1|, therefore two MatchedRuleInfo
      // should be returned:
      // (abc_id, first_tab_id) and (abc_id, second_tab_id)
      {extension_1_id, std::nullopt,
       base::StringPrintf("%d,%d|%d,%d", abc_id, first_tab_id, abc_id,
                          second_tab_id)},

      // Filtering by tab_id = |first_tab_id| should return one MatchedRuleInfo:
      // (abc_id, first_tab_id)
      {extension_1_id, first_tab_id,
       base::StringPrintf("%d,%d", abc_id, first_tab_id)},

      // Filtering by tab_id = |second_tab_id| should return one
      // MatchedRuleInfo: (abc_id, second_tab_id)
      {extension_1_id, second_tab_id,
       base::StringPrintf("%d,%d", abc_id, second_tab_id)},

      // For |extension_2|, filtering by tab_id = |first_tab_id| should return
      // one MatchedRuleInfo: (def_id, first_tab_id)
      {extension_2_id, first_tab_id,
       base::StringPrintf("%d,%d", def_id, first_tab_id)},

      // Since no rules from |extension_2| was matched for the second tab,
      // getMatchedRules should not return any rules.
      {extension_2_id, second_tab_id, ""}};

  for (const auto& test_case : test_cases) {
    if (test_case.tab_id) {
      SCOPED_TRACE(base::StringPrintf(
          "Testing getMatchedRules for tab %d and extension %s",
          *test_case.tab_id, test_case.extension_id.c_str()));
    } else {
      SCOPED_TRACE(base::StringPrintf(
          "Testing getMatchedRules for all tabs and extension %s",
          test_case.extension_id.c_str()));
    }

    std::string actual_rule_and_tab_ids =
        GetRuleAndTabIdsMatched(test_case.extension_id, test_case.tab_id);
    EXPECT_EQ(test_case.expected_rule_and_tab_ids, actual_rule_and_tab_ids);
  }

  // Close the second tab opened.
  browser()->tab_strip_model()->CloseSelectedTabs();

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(0));

  std::string actual_rule_and_tab_ids =
      GetRuleAndTabIdsMatched(extension_1_id, std::nullopt);

  // The matched rule info for the second tab should have its tab ID changed to
  // the unknown tab ID after the second tab has been closed.
  EXPECT_EQ(
      base::StringPrintf("%d,%d|%d,%d", abc_id, extension_misc::kUnknownTabId,
                         abc_id, first_tab_id),
      actual_rule_and_tab_ids);
}

// Test that rules matched for main-frame navigations are attributed will be
// reset when a main-frame navigation finishes.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesMainFrame) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  const std::string test_host = "abc.com";
  GURL page_url = embedded_test_server()->GetURL(
      test_host, "/pages_with_script/index.html");

  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.condition->url_filter = test_host;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}, "extension_1", {}));

  // Navigate to abc.com.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  std::string actual_rule_and_tab_ids =
      GetRuleAndTabIdsMatched(last_loaded_extension_id(), std::nullopt);

  // Since the block rule for abc.com is matched for the main-frame navigation
  // request, it should be attributed to the current tab.
  const int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  std::string expected_rule_and_tab_ids =
      base::StringPrintf("%d,%d", *rule.id, first_tab_id);

  EXPECT_EQ(expected_rule_and_tab_ids, actual_rule_and_tab_ids);

  // Navigate to abc.com again.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  actual_rule_and_tab_ids =
      GetRuleAndTabIdsMatched(last_loaded_extension_id(), std::nullopt);

  // The same block rule is matched for this navigation request, and should be
  // attributed to the current tab. Since the main-frame for which the older
  // matched rule is associated with is no longer active, the older matched
  // rule's tab ID should be changed to the unknown tab ID.
  expected_rule_and_tab_ids =
      base::StringPrintf("%d,%d|%d,%d", *rule.id, extension_misc::kUnknownTabId,
                         *rule.id, first_tab_id);

  EXPECT_EQ(expected_rule_and_tab_ids, actual_rule_and_tab_ids);

  // Navigate to nomatch,com.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "nomatch.com", "/pages_with_script/index.html")));

  // No rules should be matched for the navigation request to nomatch.com and
  // all rules previously attributed to |first_tab_id| should now be attributed
  // to the unknown tab ID as a result of the navigation. Therefore
  // GetMatchedRules should return no matched rules.
  actual_rule_and_tab_ids =
      GetRuleAndTabIdsMatched(last_loaded_extension_id(), first_tab_id);
  EXPECT_EQ("", actual_rule_and_tab_ids);
}

// Test that getMatchedRules only returns rules more recent than the provided
// timestamp.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesTimestampFiltering) {
  base::Time start_time = base::Time::Now();

  base::SimpleTestClock clock_;
  clock_.SetNow(start_time);
  rules_monitor_service()->action_tracker().SetClockForTests(&clock_);

  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  const std::string kFrameName1 = "frame1";
  const GURL page_url = embedded_test_server()->GetURL(
      "nomatch.com", "/page_with_two_frames.html");

  const std::string example_host = "example.com";
  const GURL sub_frame_url = embedded_test_server()->GetURL(
      example_host, "/pages_with_script/index.html");

  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.condition->url_filter = example_host;
  rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
  rule.action->type = "block";

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}, "extension_1", {}));
  const ExtensionId& extension_id = last_loaded_extension_id();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  // Using subframes here to make requests without triggering main-frame
  // navigations. This request will match with the block rule for example.com at
  // |start_time|.
  NavigateFrame(kFrameName1, sub_frame_url);

  static constexpr char getMatchedRuleTimestampScript[] = R"(
    chrome.declarativeNetRequest.getMatchedRules((rules) => {
      var rule_count = rules.rulesMatchedInfo.length;
      var timestamp = rule_count === 1 ?
          rules.rulesMatchedInfo[0].timeStamp.toString() : '';

      chrome.test.sendScriptResult(timestamp);
    });
  )";

  std::string timestamp_string = ExecuteScriptInBackgroundPageAndReturnString(
      extension_id, getMatchedRuleTimestampScript,
      browsertest_util::ScriptUserActivation::kDontActivate);

  double matched_rule_timestamp;
  ASSERT_TRUE(base::StringToDouble(timestamp_string, &matched_rule_timestamp));

  // Verify that the rule was matched at |start_time|.
  EXPECT_DOUBLE_EQ(start_time.InMillisecondsFSinceUnixEpochIgnoringNull(),
                   matched_rule_timestamp);

  // Advance the clock to capture a timestamp after when the first request was
  // made.
  clock_.Advance(base::Milliseconds(100));
  base::Time timestamp_1 = clock_.Now();
  clock_.Advance(base::Milliseconds(100));

  // Navigate to example.com again. This should cause |rule| to be matched.
  NavigateFrame(kFrameName1, sub_frame_url);

  // Advance the clock to capture a timestamp after when the second request was
  // made.
  clock_.Advance(base::Milliseconds(100));
  base::Time timestamp_2 = clock_.Now();

  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Two rules should be matched on |first_tab_id|.
  std::string rule_count = GetMatchedRuleCount(extension_id, first_tab_id,
                                               std::nullopt /* timestamp */);
  EXPECT_EQ("2", rule_count);

  // Only one rule should be matched on |first_tab_id| after |timestamp_1|.
  rule_count = GetMatchedRuleCount(extension_id, first_tab_id, timestamp_1);
  EXPECT_EQ("1", rule_count);

  // No rules should be matched on |first_tab_id| after |timestamp_2|.
  rule_count = GetMatchedRuleCount(extension_id, first_tab_id, timestamp_2);
  EXPECT_EQ("0", rule_count);

  rules_monitor_service()->action_tracker().SetClockForTests(nullptr);
}

// Test that getMatchedRules will only return matched rules for individual tabs
// where activeTab is granted.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesActiveTab) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasActiveTab);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  const std::string test_host = "abc.com";
  GURL page_url = embedded_test_server()->GetURL(
      test_host, "/pages_with_script/index.html");

  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.condition->url_filter = test_host;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}, "extension_1", {}));
  const ExtensionId& extension_id = last_loaded_extension_id();

  // Navigate to |page_url| which will cause |rule| to be matched.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Open a new tab and navigate to |page_url| which will cause |rule| to be
  // matched.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  // Get the ActiveTabPermissionGranter for the second tab.
  ActiveTabPermissionGranter* active_tab_granter =
      tab_helper->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_granter);

  const Extension* dnr_extension = last_loaded_extension();

  // Grant the activeTab permission for the second tab.
  active_tab_granter->GrantIfRequested(dnr_extension);

  int second_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Calling getMatchedRules with no tab ID specified should result in an error
  // since the extension does not have the feedback permission.
  EXPECT_EQ(declarative_net_request::kErrorGetMatchedRulesMissingPermissions,
            GetMatchedRuleCount(extension_id, std::nullopt /* tab_id */,
                                std::nullopt /* timestamp */));

  // Calling getMatchedRules for a tab without the activeTab permission granted
  // should result in an error.
  EXPECT_EQ(declarative_net_request::kErrorGetMatchedRulesMissingPermissions,
            GetMatchedRuleCount(extension_id, first_tab_id,
                                std::nullopt /* timestamp */));

  // Calling getMatchedRules for a tab with the activeTab permission granted
  // should return the rules matched for that tab.
  EXPECT_EQ("1", GetMatchedRuleCount(extension_id, second_tab_id,
                                     std::nullopt /* timestamp */));
}

// Test that getMatchedRules will not be throttled if the call is associated
// with a user gesture.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesNoThrottlingIfUserGesture) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  // Ensure that GetMatchedRules is being throttled.
  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(false);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({}));
  const ExtensionId& extension_id = last_loaded_extension_id();

  auto get_matched_rules_count = [this, &extension_id](bool user_gesture) {
    auto user_gesture_setting =
        user_gesture ? browsertest_util::ScriptUserActivation::kActivate
                     : browsertest_util::ScriptUserActivation::kDontActivate;

    return GetMatchedRuleCount(extension_id, std::nullopt /* tab_id */,
                               std::nullopt /* timestamp */,
                               user_gesture_setting);
  };

  // Call getMatchedRules without a user gesture, until the quota is reached.
  // None of these calls should return an error.
  for (int i = 1; i <= dnr_api::MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL; ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "Testing getMatchedRules call without user gesture %d of %d", i,
        dnr_api::MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL));
    EXPECT_EQ("0", get_matched_rules_count(false));
  }

  // Calling getMatchedRules without a user gesture should return an error after
  // the quota has been reached.
  EXPECT_EQ(
      "This request exceeds the MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL quota.",
      get_matched_rules_count(false));

  // Calling getMatchedRules with a user gesture should not return an error even
  // after the quota has been reached.
  EXPECT_EQ("0", get_matched_rules_count(true));
}

// Tests extension update for an extension using declarativeNetRequest.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       ExtensionRemovesOneRulesetOnUpdate) {
  auto create_single_rule_ruleset = [this](
                                        const std::string& ruleset_id_and_path,
                                        bool enabled,
                                        const std::string& filter) {
    std::vector<TestRule> rules = {CreateMainFrameBlockRule(filter)};
    return TestRulesetInfo(ruleset_id_and_path, ToListValue(rules), enabled);
  };

  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  std::vector<TestRulesetInfo> rulesets = {
      create_single_rule_ruleset("id1", true, "google"),
      create_single_rule_ruleset("id2", false, "yahoo"),
      create_single_rule_ruleset("id3", true, "example"),
  };

  constexpr char kDirectory1[] = "dir1";
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, kDirectory1, {} /* hosts */));
  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* extension = last_loaded_extension();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);

  // Since this is a packed extension, rulesets are only indexed when they are
  // first enabled. The second ruleset has not been enabled yet, so it shouldn't
  // have been indexed yet either.
  int checksum = -1;
  PrefsHelper helper(*prefs);
  EXPECT_TRUE(helper.GetStaticRulesetChecksum(
      extension_id, kMinValidStaticRulesetID, checksum));
  EXPECT_FALSE(helper.GetStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 1), checksum));
  EXPECT_TRUE(helper.GetStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 2), checksum));

  // Also add a dynamic rule.
  ASSERT_NO_FATAL_FAILURE(
      AddDynamicRules(extension_id, {CreateMainFrameBlockRule("dynamic")}));

  // Also update the set of enabled static rulesets.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(extension_id, {"id1"}, {"id2", "id3"}));

  CompositeMatcher* composite_matcher =
      ruleset_manager()->GetMatcherForExtension(extension_id);
  ASSERT_TRUE(composite_matcher);
  EXPECT_THAT(GetPublicRulesetIDs(*extension, *composite_matcher),
              UnorderedElementsAre("id2", "id3", dnr_api::DYNAMIC_RULESET_ID));

  // Also sanity check the extension prefs entry for the rulesets. Note that
  // the second static ruleset now should have been indexed since it has now
  // been enabled.
  int dynamic_checksum_1 = -1;
  EXPECT_TRUE(helper.GetStaticRulesetChecksum(
      extension_id, kMinValidStaticRulesetID, checksum));
  EXPECT_TRUE(helper.GetStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 1), checksum));
  EXPECT_TRUE(helper.GetStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 2), checksum));
  EXPECT_FALSE(helper.GetStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 3), checksum));
  EXPECT_TRUE(
      helper.GetDynamicRulesetChecksum(extension_id, dynamic_checksum_1));
  std::optional<std::set<RulesetID>> enabled_static_rulesets =
      helper.GetEnabledStaticRulesets(extension_id);
  ASSERT_TRUE(enabled_static_rulesets);
  EXPECT_THAT(
      *enabled_static_rulesets,
      UnorderedElementsAre(RulesetID(kMinValidStaticRulesetID.value() + 1),
                           RulesetID(kMinValidStaticRulesetID.value() + 2)));

  std::vector<TestRulesetInfo> new_rulesets = {
      create_single_rule_ruleset("id1", true, "yahoo"),
      create_single_rule_ruleset("new_id2", false, "msn")};

  const char* kDirectory2 = "dir2";
  ASSERT_NO_FATAL_FAILURE(UpdateLastLoadedExtension(
      new_rulesets, kDirectory2, {} /* hosts */,
      0 /* expected_extensions_with_rulesets_count_change */,
      true /* has_dynamic_ruleset */, false /* is_delayed_update */));
  extension = extension_registry()->enabled_extensions().GetByID(extension_id);

  composite_matcher = ruleset_manager()->GetMatcherForExtension(extension_id);
  ASSERT_TRUE(composite_matcher);

  EXPECT_THAT(GetPublicRulesetIDs(*extension, *composite_matcher),
              UnorderedElementsAre("id1", dnr_api::DYNAMIC_RULESET_ID));

  int dynamic_checksum_2;
  EXPECT_TRUE(helper.GetStaticRulesetChecksum(
      extension_id, kMinValidStaticRulesetID, checksum));
  EXPECT_FALSE(helper.GetStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 1), checksum));
  EXPECT_FALSE(helper.GetStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 2), checksum));
  EXPECT_TRUE(
      helper.GetDynamicRulesetChecksum(extension_id, dynamic_checksum_2));
  EXPECT_EQ(dynamic_checksum_2, dynamic_checksum_1);

  // Ensure the preference for enabled static rulesets is cleared on extension
  // update.
  EXPECT_FALSE(helper.GetEnabledStaticRulesets(extension_id));
}

// Tests extension update for an extension using declarativeNetRequest.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       ExtensionRemovesAllRulesetsOnUpdate) {
  auto create_single_rule_ruleset = [this](
                                        const std::string& ruleset_id_and_path,
                                        bool enabled,
                                        const std::string& filter) {
    std::vector<TestRule> rules = {CreateMainFrameBlockRule(filter)};
    return TestRulesetInfo(ruleset_id_and_path, ToListValue(rules), enabled);
  };

  std::vector<TestRulesetInfo> rulesets = {
      create_single_rule_ruleset("id1", true, "google"),
      create_single_rule_ruleset("id2", true, "example")};

  const char* kDirectory1 = "dir1";
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, kDirectory1, {} /* hosts */));
  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* extension = last_loaded_extension();

  CompositeMatcher* composite_matcher =
      ruleset_manager()->GetMatcherForExtension(extension_id);
  ASSERT_TRUE(composite_matcher);

  EXPECT_THAT(GetPublicRulesetIDs(*extension, *composite_matcher),
              UnorderedElementsAre("id1", "id2"));

  // Also sanity check the extension prefs entry for the rulesets.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);
  PrefsHelper helper(*prefs);
  int checksum = -1;
  EXPECT_TRUE(helper.GetStaticRulesetChecksum(
      extension_id, kMinValidStaticRulesetID, checksum));
  EXPECT_TRUE(helper.GetStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 1), checksum));
  EXPECT_FALSE(helper.GetDynamicRulesetChecksum(extension_id, checksum));

  const char* kDirectory2 = "dir2";
  ASSERT_NO_FATAL_FAILURE(UpdateLastLoadedExtension(
      {} /* new_rulesets */, kDirectory2, {} /* hosts */,
      -1 /* expected_extensions_with_rulesets_count_change */,
      false /* has_dynamic_ruleset */, false /* is_delayed_update */));
  extension = extension_registry()->enabled_extensions().GetByID(extension_id);

  composite_matcher = ruleset_manager()->GetMatcherForExtension(extension_id);
  EXPECT_FALSE(composite_matcher);

  // Ensure the prefs entry are cleared appropriately.
  EXPECT_FALSE(helper.GetStaticRulesetChecksum(
      extension_id, kMinValidStaticRulesetID, checksum));
  EXPECT_FALSE(helper.GetStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 1), checksum));
  EXPECT_FALSE(helper.GetDynamicRulesetChecksum(extension_id, checksum));
}

// Tests that persisted disabled static rule ids are no longer kept after an
// extension update.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       PackedUpdateAfterUpdateStaticRules) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  std::string ruleset_id = "ruleset1";
  std::vector<TestRulesetInfo> rulesets = {TestRulesetInfo(
      ruleset_id, ToListValue({CreateGenericRule(1), CreateGenericRule(2),
                               CreateGenericRule(3)}))};

  const char* kDirectory1 = "dir1";

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, kDirectory1, {} /* hosts */));
  const Extension* extension = last_loaded_extension();

  CompositeMatcher* composite_matcher =
      ruleset_manager()->GetMatcherForExtension(last_loaded_extension_id());
  ASSERT_TRUE(composite_matcher);

  EXPECT_THAT(GetPublicRulesetIDs(*extension, *composite_matcher),
              UnorderedElementsAre(ruleset_id));

  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(ruleset_id), testing::IsEmpty());
  VerifyGetDisabledRuleIds(last_loaded_extension_id(), ruleset_id, {});

  UpdateStaticRules(last_loaded_extension_id(), ruleset_id,
                    {2} /* rule_ids_to_disable */, {} /* rule_ids_to_enable */);

  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(ruleset_id), ElementsAreArray({2}));
  VerifyGetDisabledRuleIds(last_loaded_extension_id(), ruleset_id, {2});

  const char* kDirectory2 = "dir2";
  ASSERT_NO_FATAL_FAILURE(UpdateLastLoadedExtension(
      rulesets, kDirectory2, {} /* hosts */,
      0 /* expected_extensions_with_rulesets_count_change */,
      false /* has_dynamic_ruleset */, false /* is_delayed_update */));
  extension = last_loaded_extension();

  composite_matcher =
      ruleset_manager()->GetMatcherForExtension(last_loaded_extension_id());
  EXPECT_TRUE(composite_matcher);

  EXPECT_THAT(GetPublicRulesetIDs(*extension, *composite_matcher),
              UnorderedElementsAre(ruleset_id));

  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(ruleset_id), testing::IsEmpty());
  VerifyGetDisabledRuleIds(last_loaded_extension_id(), ruleset_id, {});
}

// Tests that prefs from the older version of the extension such as ruleset
// checksums and enabled static rulesets are reset when the extension goes
// through a delayed update.
// Regression for crbug.com/40285683.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       RemoveStalePrefsOnDelayedUpdate) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_ListenForOnUpdateAvailable);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  PrefsHelper helper(*prefs);
  auto is_checksum_in_prefs = [this, &helper](const RulesetID& ruleset_id) {
    int checksum;
    return helper.GetStaticRulesetChecksum(last_loaded_extension_id(),
                                           ruleset_id, checksum);
  };

  // Load one extension with one ruleset that is disabled by default.
  std::string ruleset_id = "ruleset1";
  std::vector<TestRulesetInfo> rulesets = {
      TestRulesetInfo(ruleset_id, ToListValue({CreateGenericRule(1)}), false)};
  static constexpr char kDirectory1[] = "dir1";
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, kDirectory1, {} /* hosts */));

  VerifyPublicRulesetIds(last_loaded_extension(), {});

  // Now enable the ruleset and check that prefs are updated with the ruleset's
  // checksum and the extension's set of enabled rulesets.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {}, {ruleset_id}));

  VerifyPublicRulesetIds(last_loaded_extension(), {ruleset_id});
  std::optional<std::set<RulesetID>> enabled_static_rulesets =
      helper.GetEnabledStaticRulesets(last_loaded_extension_id());
  EXPECT_THAT(
      enabled_static_rulesets.value_or(std::set<RulesetID>()),
      UnorderedElementsAre(RulesetID(kMinValidStaticRulesetID.value())));
  EXPECT_TRUE(is_checksum_in_prefs(kMinValidStaticRulesetID));

  // Update the extension with a slightly different ruleset that's disabled by
  // default (so the checksum changes) and make it a delayed update.
  static constexpr char kDirectory2[] = "dir2";
  ASSERT_NO_FATAL_FAILURE(UpdateLastLoadedExtension(
      {TestRulesetInfo(ruleset_id, ToListValue({CreateGenericRule(2)}), false)},
      kDirectory2, {} /* hosts */,
      -1 /* expected_extensions_with_rulesets_count_change */, false,
      true /* is_delayed_update */));

  // Verify that no rulesets are enabled and the prefs contain no checksums nor
  // any enabled ruleset ids.
  VerifyPublicRulesetIds(last_loaded_extension(), {});
  EXPECT_FALSE(helper.GetEnabledStaticRulesets(last_loaded_extension_id()));
  EXPECT_FALSE(is_checksum_in_prefs(kMinValidStaticRulesetID));

  // Enable the ruleset again (this operation should succeed) and check the
  // updated state.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {}, {ruleset_id}));

  VerifyPublicRulesetIds(last_loaded_extension(), {ruleset_id});
  enabled_static_rulesets =
      helper.GetEnabledStaticRulesets(last_loaded_extension_id());
  EXPECT_THAT(
      enabled_static_rulesets.value_or(std::set<RulesetID>()),
      UnorderedElementsAre(RulesetID(kMinValidStaticRulesetID.value())));
  EXPECT_TRUE(is_checksum_in_prefs(kMinValidStaticRulesetID));
}

// Fixture to test the "allowAllRequests" action.
class DeclarativeNetRequestAllowAllRequestsBrowserTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  struct RuleData {
    int id;
    int priority;
    std::string action_type;
    std::string url_filter;
    bool is_regex_rule;
    std::optional<std::vector<std::string>> resource_types;
    std::optional<std::vector<std::string>> request_methods;
  };

  DeclarativeNetRequestAllowAllRequestsBrowserTest() = default;

  void RunTest(const std::vector<RuleData>& rule_data,
               const std::vector<std::string>& paths_seen,
               const std::vector<std::string>& paths_not_seen,
               bool post_navigation = false) {
    std::vector<TestRule> test_rules;
    for (const auto& rule : rule_data) {
      TestRule test_rule = CreateGenericRule();
      test_rule.id = rule.id;
      test_rule.priority = rule.priority;
      test_rule.action->type = rule.action_type;
      test_rule.condition->url_filter.reset();
      if (rule.is_regex_rule)
        test_rule.condition->regex_filter = rule.url_filter;
      else
        test_rule.condition->url_filter = rule.url_filter;
      test_rule.condition->resource_types = rule.resource_types;
      test_rule.condition->request_methods = rule.request_methods;
      test_rules.push_back(test_rule);
    }

    ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(test_rules));

    GURL page_url = embedded_test_server()->GetURL(
        "example.com", post_navigation ? "/post_to_page_with_two_frames.html"
                                       : "/page_with_two_frames.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

    std::map<GURL, net::test_server::HttpRequest> requests_seen =
        GetAndResetRequestsToServer();

    for (const auto& path : paths_seen) {
      GURL expected_request_url = embedded_test_server()->GetURL(path);
      EXPECT_TRUE(base::Contains(requests_seen, expected_request_url))
          << expected_request_url.spec()
          << " was not requested from the server.";
    }

    for (const auto& path : paths_not_seen) {
      GURL expected_request_url = embedded_test_server()->GetURL(path);
      EXPECT_FALSE(base::Contains(requests_seen, expected_request_url))
          << expected_request_url.spec() << " request seen unexpectedly.";
    }
  }

 protected:
  // Requests made when the browser is navigated to
  // "http://example.com/page_with_two_frames.html".
  std::vector<std::string> requests = {
      {"/page_with_two_frames.html"},         // 0
      {"/subresources/script.js"},            // 1
      {"/child_frame.html?frame=1"},          // 2
      {"/subresources/script.js?frameId=1"},  // 3
      {"/child_frame.html?frame=2"},          // 4
      {"/subresources/script.js?frameId=2"},  // 5
  };
};

// TODO(crbug.com/40853402): Re-enable this test. It was disabled because of
// flakiness.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestAllowAllRequestsBrowserTest,
                       DISABLED_Test1) {
  std::vector<RuleData> rule_data = {
      {1, 4, "allowAllRequests", "page_with_two_frames\\.html", true,
       std::vector<std::string>({"main_frame"})},
      {2, 3, "block", "script.js|", false},
      {3, 5, "block", "script.js?frameId=1", false},
      {4, 3, "block", "script\\.js?frameId=2", true}};

  // Requests:
  // -/page_with_two_frames.html (Matching rule=1)
  //   -/subresources/script.js (Matching rule=[1,2] Winner=1)
  //   -/child_frame.html?frame=1 (Matching Rule=1)
  //     -/subresources/script.js?frameId=1 (Matching Rule=[1,3] Winner=3)
  //   -/child_frame.html?frame=2 (Matching Rule=1)
  //     -/subresources/script.js?frameId=2 (Matching Rule=1,4 Winner=1)
  // Hence only requests[3] is blocked.
  RunTest(rule_data,
          {requests[0], requests[1], requests[2], requests[4], requests[5]},
          {requests[3]});
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestAllowAllRequestsBrowserTest,
                       Test2) {
  std::vector<RuleData> rule_data = {
      {1, 4, "allowAllRequests", "page_with_two_frames.html", false,
       std::vector<std::string>({"main_frame"})},
      {2, 5, "block", "script\\.js", true},
      {3, 6, "allowAllRequests", "child_frame.html", false,
       std::vector<std::string>({"sub_frame"})},
      {4, 7, "block", "frame=1", true}};

  // Requests:
  // -/page_with_two_frames.html (Matching rule=1)
  //   -/subresources/script.js (Matching rule=[1,2] Winner=2, Blocked)
  //   -/child_frame.html?frame=1 (Matching Rule=[1,3,4] Winner=4, Blocked)
  //     -/subresources/script.js?frameId=1 (Source Frame was blocked)
  //   -/child_frame.html?frame=2 (Matching Rule=[1,3] Winner=3)
  //     -/subresources/script.js?frameId=2 (Matching Rule=[1,2,3] Winner=3)
  RunTest(rule_data, {requests[0], requests[4], requests[5]},
          {requests[1], requests[2], requests[3]});
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestAllowAllRequestsBrowserTest,
                       Test3) {
  std::vector<RuleData> rule_data = {
      {1, 1, "allowAllRequests", "page_with_two_frames.html", false,
       std::vector<std::string>({"main_frame"})},
      {2, 5, "block", ".*", true},
  };

  // Requests:
  // -/page_with_two_frames.html (Matching rule=1)
  //   -/subresources/script.js (Matching rule=[1,2] Winner=2)
  //   -/child_frame.html?frame=1 (Matching Rule=[1,2] Winner=2)
  //     -/subresources/script.js?frameId=1 (Source Frame was blocked)
  //   -/child_frame.html?frame=2 (Matching Rule=[1,2] Winner=2)
  //     -/subresources/script.js?frameId=2 (Source frame was blocked)
  // Hence only the main-frame request goes through.
  RunTest(rule_data, {requests[0]},
          {requests[1], requests[2], requests[3], requests[4], requests[5]});
}

// TODO(crbug.com/40853402): Re-enable this test. It was disabled because of
// flakiness.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestAllowAllRequestsBrowserTest,
                       DISABLED_Test4) {
  std::vector<RuleData> rule_data = {
      {1, 6, "allowAllRequests", "page_with_two_frames\\.html", true,
       std::vector<std::string>({"main_frame"})},
      {2, 5, "block", "*", false},
  };

  // Requests:
  // -/page_with_two_frames.html (Matching rule=1)
  //   -/subresources/script.js (Matching rule=[1,2] Winner=1)
  //   -/child_frame.html?frame=1 (Matching Rule=[1,2] Winner=1)
  //     -/subresources/script.js?frameId=1 (Matching Rule=[1,2] Winner=1)
  //   -/child_frame.html?frame=2 (Matching Rule=[1,2] Winner=1)
  //     -/subresources/script.js?frameId=2 (Matching Rule=[1,2] Winner=1)
  // Hence all requests go through.
  RunTest(rule_data,
          {requests[0], requests[1], requests[2], requests[3], requests[4],
           requests[5]},
          {});
}

// TODO(crbug.com/40846422): Re-enable this test on MAC
#if BUILDFLAG(IS_MAC)
#define MAYBE_TestPostNavigationMatched DISABLED_TestPostNavigationMatched
#else
#define MAYBE_TestPostNavigationMatched TestPostNavigationMatched
#endif
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestAllowAllRequestsBrowserTest,
                       MAYBE_TestPostNavigationMatched) {
  std::vector<RuleData> rule_data = {
      {1, 6, "allowAllRequests", "page_with_two_frames\\.html", true,
       std::vector<std::string>({"main_frame"}),
       std::vector<std::string>({"post"})},
      {2, 5, "block", "*", false},
  };

  // Requests:
  // -/page_with_two_frames.html (Matching rule=1)
  //   -/subresources/script.js (Matching rule=[1,2] Winner=1)
  //   -/child_frame.html?frame=1 (Matching Rule=[1,2] Winner=1)
  //     -/subresources/script.js?frameId=1 (Matching Rule=[1,2] Winner=1)
  //   -/child_frame.html?frame=2 (Matching Rule=[1,2] Winner=1)
  //     -/subresources/script.js?frameId=2 (Matching Rule=[1,2] Winner=1)
  // Hence all requests go through.
  RunTest(rule_data,
          {requests[0], requests[1], requests[2], requests[3], requests[4],
           requests[5]},
          {}, true);
}

// TODO(crbug.com/40852913): Re-enable this test on MAC
#if BUILDFLAG(IS_MAC)
#define MAYBE_TestPostNavigationNotMatched DISABLED_TestPostNavigationNotMatched
#else
#define MAYBE_TestPostNavigationNotMatched TestPostNavigationNotMatched
#endif
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestAllowAllRequestsBrowserTest,
                       MAYBE_TestPostNavigationNotMatched) {
  std::vector<RuleData> rule_data = {
      {1, 6, "allowAllRequests", "page_with_two_frames\\.html", true,
       std::vector<std::string>({"main_frame"}),
       std::vector<std::string>({"get"})},
      {2, 5, "block", "*", false},
  };

  // Requests:
  // -/page_with_two_frames.html (No matches)
  //   -/subresources/script.js (Matching rule 2)
  //   -/child_frame.html?frame=1 (Matching rule 2)
  //     -/subresources/script.js?frameId=1 (Matching rule 2)
  //   -/child_frame.html?frame=2 (Matching rule 2)
  //     -/subresources/script.js?frameId=2 (Matching rule 2)
  // Hence requests are blocked.
  RunTest(rule_data, {requests[0]},
          {requests[1], requests[2], requests[3], requests[4], requests[5]},
          true);
}

// Tests that when an extension is updated but loses the declarativeNetRequest
// permission, its dynamic ruleset is not enabled.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       UpdateRespectsPermission) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Load an extension with no rulesets and add a dynamic rule.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRulesets(
      {} /* rulesets */, "ext" /* directory */, {} /* hosts */));
  const ExtensionId& extension_id = last_loaded_extension_id();
  AddDynamicRules(extension_id, {CreateGenericRule()});

  VerifyPublicRulesetIds(last_loaded_extension(),
                         {dnr_api::DYNAMIC_RULESET_ID});

  // Now update the extension such that it loses the declarativeNetRequest
  // permission and manifest key.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_OmitDeclarativeNetRequestPermission |
                   ConfigFlag::kConfig_OmitDeclarativeNetRequestKey);

  ASSERT_NO_FATAL_FAILURE(UpdateLastLoadedExtension(
      {} /* new_rulesets */, "new_dir" /* new_directory */, {} /* new_hosts */,
      -1 /* expected_extensions_with_rulesets_count_change */,
      false /* has_dynamic_ruleset */, false /* is_delayed_update */));

  // Verify that the extension doesn't have any enabled rulesets since it lacks
  // the declarativeNetRequest permission.
  EXPECT_FALSE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // Now update the extension again but this time with the declarativeNetRequest
  // permission. With the permission added back, its pre-update dynamic ruleset
  // should be enabled again.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);
  ASSERT_NO_FATAL_FAILURE(UpdateLastLoadedExtension(
      {} /* new_rulesets */, "new_dir2" /* new_directory */, {} /* new_hosts */,
      1 /* expected_extensions_with_rulesets_count_change */,
      true /* has_dynamic_ruleset */, false /* is_delayed_update */));
  VerifyPublicRulesetIds(last_loaded_extension(),
                         {dnr_api::DYNAMIC_RULESET_ID});
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

  DeclarativeNetRequestHostPermissionsBrowserTest(
      const DeclarativeNetRequestHostPermissionsBrowserTest&) = delete;
  DeclarativeNetRequestHostPermissionsBrowserTest& operator=(
      const DeclarativeNetRequestHostPermissionsBrowserTest&) = delete;

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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(WasFrameWithScriptLoaded(GetPrimaryMainFrame()));

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

  DeclarativeNetRequestResourceTypeBrowserTest(
      const DeclarativeNetRequestResourceTypeBrowserTest&) = delete;
  DeclarativeNetRequestResourceTypeBrowserTest& operator=(
      const DeclarativeNetRequestResourceTypeBrowserTest&) = delete;

 protected:
  // TODO(crbug.com/40508457): Add tests for "object", "ping", "other", "font",
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
      return content::EvalJs(frame, script).ExtractBool();
    };

    for (const auto& test_case : test_cases) {
      GURL url = embedded_test_server()->GetURL(test_case.hostname,
                                                "/subresources.html");
      SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
      ASSERT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

      content::RenderFrameHost* frame = GetPrimaryMainFrame();

      // sub-frame.
      EXPECT_EQ(!(test_case.blocked_mask & kSubframe),
                execute_script(frame, "!!window.frameLoaded;"));

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
        rule.condition->resource_types = std::nullopt;
      else
        rule.condition->resource_types = rule_data.resource_types;

      rule.condition->excluded_resource_types =
          rule_data.excluded_resource_types;
      rule.id = rule_data.id;
      rule.condition->domains = std::vector<std::string>({rule_data.domain});
      // Don't specify the urlFilter, which should behaves the same as "*".
      rule.condition->url_filter = std::nullopt;
      rules.push_back(rule);
    }
    LoadExtensionWithRules(rules);
  }
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

class DeclarativeNetRequestSubresourceWebBundlesBrowserTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DeclarativeNetRequestSubresourceWebBundlesBrowserTest() = default;
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    CreateTempDir();
    InitializeRulesetManagerObserver();
  }

 protected:
  bool TryLoadScript(const std::string& script_src) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::string script = base::StringPrintf(R"(
      (() => {
        const script = document.createElement('script');
        return new Promise(resolve => {
          script.addEventListener('load', () => {
            resolve(true);
          });
          script.addEventListener('error', () => {
            resolve(false);
          });
          script.src = '%s';
          document.body.appendChild(script);
        });
      })();
                                          )",
                                            script_src.c_str());
    return EvalJs(web_contents->GetPrimaryMainFrame(), script).ExtractBool();
  }

  bool TryLoadBundle(const std::string& href, const std::string& resources) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::string script = base::StringPrintf(R"(
          (() => {
            const script = document.createElement('script');
            script.type = 'webbundle';
            return new Promise(resolve => {
              script.addEventListener('load', () => {
                resolve(true);
              });
              script.addEventListener('error', () => {
                resolve(false);
              });
              script.textContent = JSON.stringify({
                source: '%s',
                resources: ['%s'],
              });
              document.body.appendChild(script);
            });
          })();
        )",
                                            href.c_str(), resources.c_str());
    return EvalJs(web_contents->GetPrimaryMainFrame(), script).ExtractBool();
  }

  // Registers a request handler for static content.
  void RegisterRequestHandler(const std::string& relative_url,
                              const std::string& content_type,
                              const std::string& content) {
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [relative_url, content_type,
         content](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == relative_url) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type(content_type);
            response->set_content(content);
            return std::move(response);
          }
          return nullptr;
        }));
  }

  // Registers a request handler for web bundle. This method takes a pointer of
  // the content of the web bundle, because we can't create the content of the
  // web bundle before starting the server since we need to know the port number
  // of the test server due to the same-origin restriction (the origin of
  // subresource which is written in the web bundle must be same as the origin
  // of the web bundle), and we can't call
  // EmbeddedTestServer::RegisterRequestHandler() after starting the server.
  void RegisterWebBundleRequestHandler(const std::string& relative_url,
                                       const std::string* web_bundle) {
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [relative_url, web_bundle](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == relative_url) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("application/webbundle");
            response->AddCustomHeader("X-Content-Type-Options", "nosniff");
            response->set_content(*web_bundle);
            return std::move(response);
          }
          return nullptr;
        }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test for https://crbug.com/1355162.
// Ensure the following happens when DeclarativeNetRequest API blocks a
// WebBundle:
// - A request for the WebBundle fails.
// - A subresource request associated with the bundle fail.
// - A window.load is fired. In other words, any request shouldn't remain
//   pending forever.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestSubresourceWebBundlesBrowserTest,
                       WebBundleRequestCanceled) {
  // Add a rule to block the bundle.
  TestRule rule = CreateGenericRule();
  std::vector<TestRule> rules;
  rule.id = kMinValidID;
  rule.condition->url_filter = "web_bundle.wbn|";
  rule.priority = 1;
  rules.push_back(rule);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  static constexpr char kPageHtml[] = R"(
        <title>Loaded</title>
        <body>
        <script>
        (async () => {
          const window_load = new Promise((resolve) => {
            window.addEventListener("load", () => {
              resolve();
            });
          });

          const wbn_url =
              new URL('./web_bundle.wbn', location.href).toString();
          const pass_js_url = new URL('./pass.js', location.href).toString();
          const script_web_bundle = document.createElement('script');
          script_web_bundle.type = 'webbundle';
          script_web_bundle.textContent = JSON.stringify({
            source: wbn_url,
            resources: [pass_js_url]
          });

          const web_bundle_error = new Promise((resolve) => {
            script_web_bundle.addEventListener("error", () => {
              resolve();
            });
          });

          document.body.appendChild(script_web_bundle);

          const script_js = document.createElement('script');
          script_js.src = pass_js_url;

          const script_js_error = new Promise((resolve) => {
            script_js.addEventListener("error", () => {
              resolve();
            });
          });

          document.body.appendChild(script_js);

          await Promise.all([window_load, web_bundle_error, script_js_error]);
          document.title = "success";
        })();
        </script>
        </body>
      )";

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);
  RegisterRequestHandler("/test.html", "text/html", kPageHtml);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a web bundle, which can be empty in this test.
  web_package::WebBundleBuilder builder;
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());

  std::u16string expected_title = u"success";
  content::TitleWatcher title_watcher(web_contents, expected_title);

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Ensure DeclarativeNetRequest API can block the requests for the subresources
// inside the web bundle.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestSubresourceWebBundlesBrowserTest,
                       RequestCanceled) {
  TestRule rule = CreateGenericRule();
  std::vector<TestRule> rules;
  rule.id = kMinValidID;
  rule.condition->url_filter = "cancel.js|";
  rule.condition->resource_types = std::vector<std::string>({"script"});
  rule.priority = 1;
  rules.push_back(rule);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  static constexpr char kPageHtml[] = R"(
        <title>Loaded</title>
        <body>
        <script>
        (() => {
          const wbn_url =
              new URL('./web_bundle.wbn', location.href).toString();
          const pass_js_url = new URL('./pass.js', location.href).toString();
          const cancel_js_url =
              new URL('./cancel.js', location.href).toString();
          const script = document.createElement('script');
          script.type = 'webbundle';
          script.textContent = JSON.stringify({
            source: wbn_url,
            resources: [pass_js_url, cancel_js_url]
          });
          document.body.appendChild(script);
        })();
        </script>
        </body>
      )";

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);
  RegisterRequestHandler("/test.html", "text/html", kPageHtml);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a web bundle.
  GURL pass_js_url = embedded_test_server()->GetURL("/pass.js");
  GURL cancel_js_url = embedded_test_server()->GetURL("/cancel.js");
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      pass_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'script loaded';");
  builder.AddExchange(
      cancel_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}}, "");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());

  std::u16string expected_title = u"script loaded";
  content::TitleWatcher title_watcher(web_contents, expected_title);
  EXPECT_TRUE(TryLoadScript("pass.js"));
  // Check that the script in the web bundle is correctly loaded even when the
  // extension with blocking handler intercepted the request.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  EXPECT_FALSE(TryLoadScript("cancel.js"));
}

// Ensure DeclarativeNetRequest API can block the requests for the subresources
// inside the web bundle whose URL is uuid-in-package:.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestSubresourceWebBundlesBrowserTest,
                       RequestCanceledUuidInPackageUrl) {
  static constexpr char pass_js_url[] =
      "uuid-in-package:fc80c15b-69e9-4a45-ab41-9c90d2b55976";
  static constexpr char cancel_js_url[] =
      "uuid-in-package:15d749ad-7d9f-49d9-94f7-83a866e7fef8";
  TestRule rule = CreateGenericRule();
  std::vector<TestRule> rules;
  rule.id = kMinValidID;
  rule.condition->url_filter = std::string(cancel_js_url) + "|";
  rule.condition->resource_types = {"script"};
  rule.priority = 1;
  rules.push_back(rule);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  static constexpr char kHtmlTemplate[] = R"(
        <title>Loaded</title>
        <body>
        <script>
        (() => {
          const wbn_url =
              new URL('./web_bundle.wbn', location.href).toString();
          const pass_js_url = '%s';
          const cancel_js_url = '%s';
          const script = document.createElement('script');
          script.type = 'webbundle';
          script.textContent = JSON.stringify({
            source: wbn_url,
            resources: [pass_js_url, cancel_js_url]
          });
          document.body.appendChild(script);
        })();
        </script>
        </body>
      )";
  const std::string page_html =
      base::StringPrintf(kHtmlTemplate, pass_js_url, cancel_js_url);

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);
  RegisterRequestHandler("/test.html", "text/html", page_html);
  ASSERT_TRUE(embedded_test_server()->Start());

  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      pass_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'script loaded';");
  builder.AddExchange(
      cancel_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}}, "");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());

  std::u16string expected_title = u"script loaded";
  content::TitleWatcher title_watcher(web_contents, expected_title);
  EXPECT_TRUE(TryLoadScript(pass_js_url));
  // Check that the pass_js_url script in the web bundle is correctly loaded
  // even when the extension with blocking handler intercepted the request.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  EXPECT_FALSE(TryLoadScript(cancel_js_url));
}

// Ensure DeclarativeNetRequest API can redirect the requests for the
// subresources inside the web bundle.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestSubresourceWebBundlesBrowserTest,
                       RequestRedirected) {
  static constexpr char kPageHtml[] = R"(
        <title>Loaded</title>
        <body>
        <script>
        (() => {
          const wbn_url = new URL('./web_bundle.wbn', location.href).toString();
          const redirect_js_url =
              new URL('./redirect.js', location.href).toString();
          const redirected_js_url =
              new URL('./redirected.js', location.href).toString();
          const redirect_to_unlisted_js_url =
              new URL('./redirect_to_unlisted.js', location.href).toString();
          const redirect_to_server =
              new URL('./redirect_to_server.js', location.href).toString();

          const script = document.createElement('script');
          script.type = 'webbundle';
          script.textContent = JSON.stringify({
            source: wbn_url,
            resources: [redirect_js_url, redirected_js_url,
                        redirect_to_unlisted_js_url, redirect_to_server]
          });
          document.body.appendChild(script);
        })();
        </script>
        </body>
      )";

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);
  RegisterRequestHandler("/test.html", "text/html", kPageHtml);
  RegisterRequestHandler("/redirect_to_server.js", "application/javascript",
                         "document.title = 'redirect_to_server';");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a web bundle.
  GURL redirect_js_url = embedded_test_server()->GetURL("/redirect.js");
  GURL redirected_js_url = embedded_test_server()->GetURL("/redirected.js");
  GURL redirect_to_unlisted_js_url =
      embedded_test_server()->GetURL("/redirect_to_unlisted.js");
  GURL redirected_to_unlisted_js_url =
      embedded_test_server()->GetURL("/redirected_to_unlisted.js");
  GURL redirect_to_server_js_url =
      embedded_test_server()->GetURL("/redirect_to_server.js");
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      redirect_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirect';");
  builder.AddExchange(
      redirected_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirected';");
  builder.AddExchange(
      redirect_to_unlisted_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirect_to_unlisted';");
  builder.AddExchange(
      redirected_to_unlisted_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirected_to_unlisted';");
  builder.AddExchange(
      redirect_to_server_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirect_to_server';");

  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  struct {
    std::string url_filter;
    std::string redirect_url_path;
  } rules_data[] = {
      {"redirect.js|", "/redirected.js"},
      {"redirect_to_unlisted.js|", "/redirected_to_unlisted.js"},
      {"redirect_to_server.js|", "/redirected_to_server.js"},
  };

  std::vector<TestRule> rules;
  int rule_id = kMinValidID;
  int rule_priority = 1;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.id = rule_id++;
    rule.priority = rule_priority++;
    rule.condition->url_filter = rule_data.url_filter;
    rule.condition->resource_types = std::vector<std::string>({"script"});
    rule.action->type = "redirect";
    rule.action->redirect.emplace();
    rule.action->redirect->url =
        embedded_test_server()->GetURL(rule_data.redirect_url_path).spec();
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());
  {
    std::u16string expected_title = u"redirected";
    content::TitleWatcher title_watcher(web_contents, expected_title);
    EXPECT_TRUE(TryLoadScript("redirect.js"));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
  {
    // In the current implementation, extensions can redirect the request to
    // the other resource in the web bundle even if the resource is not listed
    // in the resources attribute.
    std::u16string expected_title = u"redirected_to_unlisted";
    content::TitleWatcher title_watcher(web_contents, expected_title);
    EXPECT_TRUE(TryLoadScript("redirect_to_unlisted.js"));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
  // In the current implementation, extensions can't redirect the request to
  // outside the web bundle.
  EXPECT_FALSE(TryLoadScript("redirect_to_server.js"));
}

// Ensure that request to Subresource WebBundle fails if it is redirected by
// DeclarativeNetRequest API.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestSubresourceWebBundlesBrowserTest,
                       WebBundleRequestRedirected) {
  std::string web_bundle;
  RegisterWebBundleRequestHandler("/redirect.wbn", &web_bundle);
  RegisterWebBundleRequestHandler("/redirected.wbn", &web_bundle);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a web bundle.
  std::string js_url_str = embedded_test_server()->GetURL("/script.js").spec();
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      js_url_str,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'script loaded';");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  std::vector<TestRule> rules;
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.priority = 1;
  rule.condition->url_filter = "redirect.wbn|";
  rule.condition->resource_types = std::vector<std::string>({"webbundle"});
  rule.action->type = "redirect";
  rule.action->redirect.emplace();
  rule.action->redirect->url =
      embedded_test_server()->GetURL("/redirected.wbn").spec();
  rules.push_back(rule);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  // In the current implementation, extensions can't redirect requests to
  // Subresource WebBundles.
  EXPECT_FALSE(TryLoadBundle("redirect.wbn", js_url_str));

  // Without redirection, bundle load should succeed.
  EXPECT_TRUE(TryLoadBundle("redirected.wbn", js_url_str));
}

class DeclarativeNetRequestGlobalRulesBrowserTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DeclarativeNetRequestGlobalRulesBrowserTest() = default;

 protected:
  void VerifyExtensionAllocationInPrefs(
      const ExtensionId& extension_id,
      std::optional<int> expected_rules_count) {
    int actual_rules_count = 0;

    PrefsHelper helper(*ExtensionPrefs::Get(profile()));
    bool has_allocated_rules_count =
        helper.GetAllocatedGlobalRuleCount(extension_id, actual_rules_count);

    EXPECT_EQ(expected_rules_count.has_value(), has_allocated_rules_count);
    if (expected_rules_count.has_value())
      EXPECT_EQ(*expected_rules_count, actual_rules_count);
  }

  void VerifyKeepExcessAllocation(const ExtensionId& extension_id,
                                  bool expected_keep_allocation) {
    PrefsHelper helper(*ExtensionPrefs::Get(profile()));
    EXPECT_EQ(expected_keep_allocation,
              helper.GetKeepExcessAllocation(extension_id));
  }

 private:
  // Override the API guaranteed minimum to prevent a timeout on loading the
  // extension.
  base::AutoReset<int> guaranteed_minimum_override_ =
      CreateScopedStaticGuaranteedMinimumOverrideForTesting(1);

  // Similarly, override the global limit to prevent a timeout.
  base::AutoReset<int> global_limit_override_ =
      CreateScopedGlobalStaticRuleLimitOverrideForTesting(2);
};

using DeclarativeNetRequestGlobalRulesBrowserTest_Packed =
    DeclarativeNetRequestGlobalRulesBrowserTest;

// Test that extensions with allocated global rules keep their allocations after
// the browser restarts.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestGlobalRulesBrowserTest_Packed,
                       PRE_GlobalRulesBrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetLoadType());

  // Sanity check that the extension can index and enable up to
  // |rule_limit_override_| + |global_limit_override_| rules.
  ASSERT_EQ(3, GetMaximumRulesPerRuleset());

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({CreateGenericRule(1), CreateGenericRule(2)},
                             "extension_1", {} /* hosts */));
  const ExtensionId first_extension_id = last_loaded_extension_id();

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({CreateGenericRule(1), CreateGenericRule(2)},
                             "extension_2", {} /* hosts */));
  const ExtensionId second_extension_id = last_loaded_extension_id();

  std::vector<TestRulesetInfo> rulesets;
  rulesets.emplace_back("ruleset_1", ToListValue({CreateGenericRule(1)}));
  rulesets.emplace_back("ruleset_2", ToListValue({CreateGenericRule(2)}));

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, "extension_3", {} /* hosts */));

  const Extension* third_extension = last_loaded_extension();
  CompositeMatcher* composite_matcher =
      ruleset_manager()->GetMatcherForExtension(third_extension->id());

  ASSERT_TRUE(composite_matcher);

  // Check that the third extension only has |ruleset_1| enabled.
  EXPECT_THAT(GetPublicRulesetIDs(*third_extension, *composite_matcher),
              UnorderedElementsAre("ruleset_1"));

  // First and second extension should have 1 rule in the global pool, third
  // extension should have none because one of its rulesets is not enabled since
  // the global limit is reached.
  VerifyExtensionAllocationInPrefs(first_extension_id, 1);
  VerifyExtensionAllocationInPrefs(second_extension_id, 1);
  VerifyExtensionAllocationInPrefs(third_extension->id(), std::nullopt);
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestGlobalRulesBrowserTest_Packed,
                       GlobalRulesBrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetLoadType());

  // Sanity check that the extension can index and enable up to
  // |rule_limit_override| + |global_limit_override| rules.
  ASSERT_EQ(3, GetMaximumRulesPerRuleset());

  PrefsHelper helper(*ExtensionPrefs::Get(profile()));
  std::map<std::string, size_t> allocated_rule_counts;
  std::map<std::string, const Extension*> extensions_by_name;
  for (const auto& extension : extension_registry()->enabled_extensions()) {
    extensions_by_name[extension->name()] = extension.get();

    int allocated_rule_count = 0;
    if (helper.GetAllocatedGlobalRuleCount(extension->id(),
                                           allocated_rule_count)) {
      DCHECK_GE(allocated_rule_count, 0);
      allocated_rule_counts[extension->name()] = allocated_rule_count;
    }
  }

  // Check that all three extensions have the same rulesets enabled as before
  // the browser restart.
  VerifyPublicRulesetIds(extensions_by_name["extension_1"],
                         {kDefaultRulesetID});
  VerifyPublicRulesetIds(extensions_by_name["extension_2"],
                         {kDefaultRulesetID});
  VerifyPublicRulesetIds(extensions_by_name["extension_3"], {"ruleset_1"});

  EXPECT_THAT(allocated_rule_counts,
              UnorderedElementsAre(std::make_pair("extension_1", 1),
                                   std::make_pair("extension_2", 1)));
}

// Test that an extension will not have its allocation reclaimed on load if it
// is the first load after a packed update.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestGlobalRulesBrowserTest_Packed,
                       PackedUpdateAndReload) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetLoadType());

  // Load the extension with a background page so updateEnabledRulesets can be
  // called.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  std::vector<TestRulesetInfo> rulesets;
  rulesets.emplace_back(
      "ruleset_1", ToListValue({CreateGenericRule(1), CreateGenericRule(2)}));
  rulesets.emplace_back("ruleset_2", ToListValue({CreateGenericRule(3)}),
                        false /* enabled */);

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, "test_extension", {} /* hosts */));

  // Check that the extension only has |ruleset_1| enabled.
  VerifyPublicRulesetIds(last_loaded_extension(), {"ruleset_1"});

  // There should be one rule allocated.
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 1);

  // Enable |ruleset_2| and verify the allocation.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {}, {"ruleset_2"}));
  VerifyPublicRulesetIds(last_loaded_extension(), {"ruleset_1", "ruleset_2"});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 2);

  // Update the extension. This should keep the allocated rule count in prefs
  // but not the set of enabled rulesets.
  UpdateLastLoadedExtension(
      rulesets, "test_extension2", {} /* hosts */,
      0 /* expected_extensions_with_rulesets_count_change */,
      false /* has_dynamic_ruleset */, false /* is_delayed_update */);

  VerifyPublicRulesetIds(last_loaded_extension(), {"ruleset_1"});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 2);

  // Disable then enable the extension to trigger another extension load.
  DisableExtension(last_loaded_extension_id());
  WaitForExtensionsWithRulesetsCount(0);
  EnableExtension(last_loaded_extension_id());
  WaitForExtensionsWithRulesetsCount(1);

  // Since this is the second load after an update, the unused allocation should
  // be reclaimed.
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 1);
}

// Tests that excess allocation is no longer kept after an update once the
// current allocation exceeds the allocation from before the update.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestGlobalRulesBrowserTest_Packed,
                       UpdateEnabledRulesetsAfterPackedUpdate) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetLoadType());

  // Load the extension with a background page so updateEnabledRulesets can be
  // called.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  std::vector<TestRulesetInfo> rulesets;
  rulesets.emplace_back("ruleset_1", ToListValue({CreateGenericRule(1)}));
  rulesets.emplace_back("ruleset_2", ToListValue({CreateGenericRule(2)}),
                        false /* enabled */);
  rulesets.emplace_back("big_ruleset",
                        ToListValue({CreateGenericRule(3), CreateGenericRule(4),
                                     CreateGenericRule(5)}),
                        false /* enabled */);

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, "test_extension", {} /* hosts */));

  // Enable |ruleset_2|.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {}, {"ruleset_2"}));
  VerifyPublicRulesetIds(last_loaded_extension(), {"ruleset_1", "ruleset_2"});

  // There should be one rule allocated.
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 1);

  // Update the extension. This should keep the allocated rule count in prefs
  // but not the set of enabled rulesets.
  UpdateLastLoadedExtension(
      rulesets, "test_extension2", {} /* hosts */,
      0 /* expected_extensions_with_rulesets_count_change */,
      false /* has_dynamic_ruleset */, false /* is_delayed_update */);

  VerifyPublicRulesetIds(last_loaded_extension(), {"ruleset_1"});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 1);
  VerifyKeepExcessAllocation(last_loaded_extension_id(), true);

  // Adding |big_ruleset| should fail right now, and should not change the
  // allocation.
  ASSERT_NO_FATAL_FAILURE(UpdateEnabledRulesetsAndFail(
      last_loaded_extension_id(), {}, {"big_ruleset"},
      kEnabledRulesetsRuleCountExceeded));

  VerifyPublicRulesetIds(last_loaded_extension(), {"ruleset_1"});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 1);
  VerifyKeepExcessAllocation(last_loaded_extension_id(), true);

  // Removing |ruleset_1| should not change the allocation.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {"ruleset_1"}, {}));
  VerifyPublicRulesetIds(last_loaded_extension(), {});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 1);
  VerifyKeepExcessAllocation(last_loaded_extension_id(), true);

  // Adding |big_ruleset| should succeed now, and should increase the allocation
  // past the value from just before the update.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {}, {"big_ruleset"}));
  VerifyPublicRulesetIds(last_loaded_extension(), {"big_ruleset"});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 2);
  VerifyKeepExcessAllocation(last_loaded_extension_id(), false);

  // Removing |big_ruleset| should now drop the allocation down to 0 since the
  // excess allocation has been exceeded and no longer needs to be kept.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {"big_ruleset"}, {}));
  VerifyPublicRulesetIds(last_loaded_extension(), {});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), std::nullopt);
  VerifyKeepExcessAllocation(last_loaded_extension_id(), false);
}

// Test that GetAvailableStaticRuleCount includes the excess unused allocation
// after an extension update.
// TODO(crbug.com/40883375): Deflake and re-enable.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestGlobalRulesBrowserTest_Packed,
                       DISABLED_GetAvailableStaticRuleCountAfterPackedUpdate) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetLoadType());

  // Load the extension with a background page so updateEnabledRulesets can be
  // called.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  std::vector<TestRulesetInfo> rulesets;
  rulesets.emplace_back("ruleset_1", ToListValue({CreateGenericRule(1)}));
  rulesets.emplace_back("ruleset_2", ToListValue({CreateGenericRule(2)}));
  rulesets.emplace_back("ruleset_3", ToListValue({CreateGenericRule(3)}),
                        false /* enabled */);

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, "test_extension", {} /* hosts */));

  // Enable |ruleset_3|.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {}, {"ruleset_3"}));
  VerifyPublicRulesetIds(last_loaded_extension(),
                         {"ruleset_1", "ruleset_2", "ruleset_3"});

  // There should be two rules allocated.
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 2);

  // There should not be any more rules available.
  EXPECT_EQ("0", GetAvailableStaticRuleCount(last_loaded_extension_id()));

  // Update the extension. This should keep the allocated rule count in prefs
  // but not the set of enabled rulesets.
  UpdateLastLoadedExtension(
      rulesets, "test_extension2", {} /* hosts */,
      0 /* expected_extensions_with_rulesets_count_change */,
      false /* has_dynamic_ruleset */, false /* is_delayed_update */);

  VerifyPublicRulesetIds(last_loaded_extension(), {"ruleset_1", "ruleset_2"});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 2);

  // Since there are two rules enabled, and one rule that's allocated but
  // unused, there should be one rule available.
  EXPECT_EQ("1", GetAvailableStaticRuleCount(last_loaded_extension_id()));

  // Disabling |ruleset_1| and |ruleset_2| should not decrease the allocation,
  // but should increase the number of rules available.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {"ruleset_2"}, {}));
  VerifyPublicRulesetIds(last_loaded_extension(), {"ruleset_1"});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 2);
  EXPECT_EQ("2", GetAvailableStaticRuleCount(last_loaded_extension_id()));

  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(last_loaded_extension_id(), {"ruleset_1"}, {}));
  VerifyPublicRulesetIds(last_loaded_extension(), {});
  VerifyExtensionAllocationInPrefs(last_loaded_extension_id(), 2);
  EXPECT_EQ("3", GetAvailableStaticRuleCount(last_loaded_extension_id()));
}

// Tests the "requestMethods" and "excludedRequestMethods" property of a
// declarative rule condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_Methods) {
  struct {
    int id;
    std::string url_filter;
    std::vector<std::string> request_methods;
    std::vector<std::string> excluded_request_methods;
  } rules_data[] = {{1, "default", {}, {}},
                    {2, "included", {"head", "put", "other"}, {}},
                    {3, "excluded", {}, {"options", "patch", "other"}},
                    {4, "combination", {"get"}, {"put"}}};

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule(rule_data.id);
    rule.condition->url_filter = rule_data.url_filter;

    // An empty list is not allowed for the "requestMethods" property.
    if (!rule_data.request_methods.empty())
      rule.condition->request_methods = rule_data.request_methods;

    rule.condition->excluded_request_methods =
        rule_data.excluded_request_methods;
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  struct {
    std::string path;
    std::string expected_blocked_request_methods;
  } test_cases[] = {
      {"default", "DELETE,GET,HEAD,OPTIONS,PATCH,POST,PUT,UNKNOWN"},
      {"included", "HEAD,PUT,UNKNOWN"},
      {"excluded", "DELETE,GET,HEAD,POST,PUT"},
      {"combination", "GET"}};

  static constexpr char kPerformRequestWithAllMethodsScript[] = R"(
    {
      const allRequestMethods = ["DELETE", "GET", "HEAD", "OPTIONS", "PATCH",
                                 "POST", "PUT", "UNKNOWN"];
      const url = "/empty.html?%s";

      let blockedMethods = [];

      Promise.allSettled(
        allRequestMethods.map(method =>
          fetch(url, {method}).catch(() => { blockedMethods.push(method); })
        )
      ).then(() => blockedMethods.sort().join());
    }
  )";

  GURL url = embedded_test_server()->GetURL("abc.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* main_frame = GetPrimaryMainFrame();

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Path: %s", test_case.path.c_str()));
    EXPECT_EQ(
        test_case.expected_blocked_request_methods,
        content::EvalJs(main_frame,
                        base::StringPrintf(kPerformRequestWithAllMethodsScript,
                                           test_case.path.c_str())));
  }
}

// Tests that the "requestMethods" and "excludedRequestMethods" properties of a
// rule condition are considered properly for WebSocket requests.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_WebSocket) {
  // Load an extension with some DNR rules that have different request method
  // conditions.
  std::vector<TestRule> rules;

  TestRule rule1 = CreateGenericRule(1);
  rule1.condition->url_filter = "default";
  rules.push_back(rule1);

  TestRule rule2 = CreateGenericRule(2);
  rule2.condition->url_filter = "all_methods";
  rule2.condition->request_methods = {"delete", "get",  "head", "options",
                                      "patch",  "post", "put"};
  rules.push_back(rule2);

  TestRule rule3 = CreateGenericRule(3);
  rule3.condition->url_filter = "some_methods";
  rule3.condition->request_methods = {"get", "put"};
  rules.push_back(rule3);

  TestRule rule4 = CreateGenericRule(4);
  rule4.condition->url_filter = "all_methods_excluded";
  rule4.condition->excluded_request_methods = {
      "delete", "get", "head", "options", "patch", "post", "put"};
  rules.push_back(rule4);

  TestRule rule5 = CreateGenericRule(5);
  rule5.condition->url_filter = "some_methods_excluded";
  rule5.condition->excluded_request_methods = {"get", "put"};
  rules.push_back(rule5);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Start a web socket test server.
  net::SpawnedTestServer websocket_test_server(
      net::SpawnedTestServer::TYPE_WS, net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(websocket_test_server.Start());
  std::string websocket_url =
      websocket_test_server.GetURL("echo-with-no-extension").spec();

  static constexpr char kOpenWebSocketsScript[] = R"(
    {
      const websocketUrl = "%s";
      const testCases = ["default", "all_methods", "some_methods",
                         "all_methods_excluded", "some_methods_excluded"];

      let blockedTestCases = [];

      Promise.allSettled(
        testCases.map(testCase =>
          new Promise(resolve =>
          {
            let websocket = new WebSocket(websocketUrl + "?" + testCase);
            websocket.addEventListener("open", event =>
            {
              websocket.close();
              resolve();
            });
            websocket.addEventListener("error", event =>
            {
              blockedTestCases.push(testCase);
              resolve();
            });
          })
        )
      ).then(() => blockedTestCases.sort().join());
    }
  )";

  content::RenderFrameHost* main_frame = GetPrimaryMainFrame();

  EXPECT_EQ(
      "all_methods_excluded,default,some_methods_excluded",
      content::EvalJs(main_frame, base::StringPrintf(kOpenWebSocketsScript,
                                                     websocket_url.c_str())));
}

class DeclarativeNetRequestWebTransportTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DeclarativeNetRequestWebTransportTest() { webtransport_server_.Start(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DeclarativeNetRequestBrowserTest::SetUpCommandLine(command_line);
    webtransport_server_.SetUpCommandLine(command_line);
  }

 protected:
  content::WebTransportSimpleTestServer webtransport_server_;
};

// Tests that the "requestMethods" and "excludedRequestMethods" properties of a
// rule condition are considered properly for WebTransport requests.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestWebTransportTest, BlockRequests) {
  ASSERT_TRUE(https_server()->Start());

  // Load an extension with some rules that have different request method
  // conditions.
  std::vector<TestRule> rules;

  // We need to prefix "echo" so that the server accepts it.
  TestRule rule1 = CreateGenericRule(1);
  rule1.condition->url_filter = "echo1_default";
  rules.push_back(rule1);

  TestRule rule2 = CreateGenericRule(2);
  rule2.condition->url_filter = "echo2_include";
  rule2.condition->request_methods = {"connect"};
  rules.push_back(rule2);

  TestRule rule4 = CreateGenericRule(3);
  rule4.condition->url_filter = "echo3_exclude";
  rule4.condition->excluded_request_methods = {"connect"};
  rules.push_back(rule4);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/echo")));

  static constexpr char kOpenWebTransportScript[] = R"((
    async () =>
    {
      const testCases = ["echo1_default", "echo2_include", "echo3_exclude" ];

      let blockedTestCases = [];

      await Promise.allSettled(
        testCases.map(testCase =>
          new Promise(async (resolve) =>
          {
            try {
              const transport = new WebTransport(
                `https://localhost:%d/${testCase}`);
              await transport.ready;
            } catch (e) {
              blockedTestCases.push(testCase);
            }
            resolve();
          })
        )
      );
      return blockedTestCases.sort().join();
    }
  )())";

  content::RenderFrameHost* main_frame = GetPrimaryMainFrame();
  EXPECT_EQ("echo1_default,echo2_include",
            content::EvalJs(main_frame,
                            base::StringPrintf(
                                kOpenWebTransportScript,
                                webtransport_server_.server_address().port())));
}

// Tests that FLEDGE requests can be blocked by the declarativeNetRequest API,
// and that if they try to redirect requests, the request is blocked, instead of
// being redirected.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, FledgeAuctionScripts) {
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations(
      privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());
  // Mark all Privacy Sandbox APIs as attested since the test case is testing
  // behaviors not related to attestations.
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAllPrivacySandboxAttestedForTesting(true);

  static constexpr char kAddedHeaderName[] = "Header-Name";
  static constexpr char kAddedHeaderValue[] = "Header-Value";

  ASSERT_TRUE(https_server()->Start());

  PrivacySandboxSettingsFactory::GetForProfile(profile())
      ->SetAllPrivacySandboxAllowedForTesting();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/interest_group/fenced_frame.html")));

  GURL bidding_logic_url =
      https_server()->GetURL("/interest_group/bidding_logic.js");
  GURL decision_logic_url =
      https_server()->GetURL("/interest_group/decision_logic.js");
  GURL bidder_report_url = https_server()->GetURL("/echo?bidder_report");
  GURL decision_report_url = https_server()->GetURL("/echo?decision_report");

  // URL of the winning ad. Isn't actually tested anywhere in this test, but a
  // fenced frame is navigated to it, so best to use a URL with known behavior.
  // A random hostname will end up trying to connect to localhost, which could
  // end up talking to some other server running locally.
  GURL ad_url = https_server()->GetURL("/echo");

  // Add an interest group.
  EXPECT_EQ("done", content::EvalJs(
                        web_contents(),
                        content::JsReplace(
                            R"(
                              (function() {
                                navigator.joinAdInterestGroup({
                                  name: 'cars',
                                  owner: $1,
                                  biddingLogicURL: $2,
                                  userBiddingSignals: [],
                                  ads: [{
                                    renderURL: 'https://example.com/render',
                                    metadata: {ad: 'metadata', here: [1, 2, 3]}
                                  }]
                                }, /*joinDurationSec=*/ 300);
                                return 'done';
                              })();
                            )",
                            url::Origin::Create(bidding_logic_url).Serialize(),
                            bidding_logic_url)));

  // Create an extension to add a header to all requests.
  TestRule custom_response_header_rule = CreateModifyHeadersRule(
      1 /* id */, 1 /* priority */, "*",
      // request_headers
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo(kAddedHeaderName, "set", kAddedHeaderValue)}),
      std::nullopt);
  // CreateModifyHeadersRule() applies to subframes only by default, so clear
  // that.
  custom_response_header_rule.condition->resource_types = std::nullopt;

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({custom_response_header_rule}, "test_extension",
                             {URLPattern::kAllUrlsPattern}));

  std::string run_auction_command = content::JsReplace(
      R"(
         (async function() {
           let config = await navigator.runAdAuction({
             seller: $1,
             decisionLogicURL: $2,
             interestGroupBuyers: [$1],
           });
           document.querySelector('fencedframe').config =
              new FencedFrameConfig(config);
           return config;
         })()
      )",
      url::Origin::Create(decision_logic_url).Serialize(),
      decision_logic_url.spec());

  // The auction should return a unique URN URL.
  GURL ad_urn(
      content::EvalJs(web_contents(), run_auction_command).ExtractString());
  EXPECT_TRUE(ad_urn.SchemeIs(url::kUrnScheme));

  // Wait to see both the report request of both worklets.
  WaitForRequest(bidder_report_url);
  WaitForRequest(decision_report_url);
  // Clear observed URLs.
  std::map<GURL, net::test_server::HttpRequest> requests =
      GetAndResetRequestsToServer();

  // Make sure the add headers rule was applied to all requests related to the
  // auction.
  for (const GURL& expected_url : {bidding_logic_url, decision_logic_url,
                                   bidder_report_url, decision_report_url}) {
    auto request = requests.find(expected_url);
    ASSERT_NE(request, requests.end());
    auto added_header = request->second.headers.find(kAddedHeaderName);
    ASSERT_NE(added_header, request->second.headers.end());
    EXPECT_EQ(kAddedHeaderValue, added_header->second);
  }

  // Now there are no pending requests for the auction. Add a rule to block the
  // bidder's report URL.
  TestRule block_report_rule = CreateGenericRule();
  block_report_rule.condition->url_filter = bidder_report_url.spec() + "^";
  block_report_rule.id = 2;
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {block_report_rule}, "test_extension2", {URLPattern::kAllUrlsPattern}));

  // Run the auction again.
  ad_urn = GURL(
      content::EvalJs(web_contents(), run_auction_command).ExtractString());
  EXPECT_TRUE(ad_urn.SchemeIs(url::kUrnScheme));

  // Wait for the decision script's report URL to be requested.
  WaitForRequest(decision_report_url);
  // The bidder script should be blocked. Unfortunately, there's no way to wait
  // for the bidder script to not be requested. Instead, just wait for an
  // addition "tiny timeout" delay, and make sure it was not requested.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
  EXPECT_EQ(0u, GetAndResetRequestsToServer().count(bidder_report_url));

  // Load a second extension which redirects requests for the bidding script to
  // a URL that serves an identical bidding script.
  TestRule redirect_bidding_logic_rule = CreateGenericRule();
  redirect_bidding_logic_rule.condition->url_filter =
      bidding_logic_url.spec() + "^";
  redirect_bidding_logic_rule.id = 3;
  redirect_bidding_logic_rule.action->type = "redirect";
  redirect_bidding_logic_rule.action->redirect.emplace();
  redirect_bidding_logic_rule.action->redirect->url =
      https_server()->GetURL("/interest_group/bidding_logic2.js").spec();

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({redirect_bidding_logic_rule}, "test_extension3",
                             {URLPattern::kAllUrlsPattern}));

  // Redirecting a bidder script, even to another bidder script, should cause
  // the request to fail, which causes the entire auction to fail, since there's
  // only one bidder script.
  EXPECT_EQ(nullptr, content::EvalJs(web_contents(), run_auction_command));
}

class DeclarativeNetRequestBackForwardCacheBrowserTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DeclarativeNetRequestBackForwardCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{features::kBackForwardCache, {}}}),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  // Setups the back forward cache with one entry (a.com) and returns a
  // RenderFrameDeletedObserver that watches for it being destroyed.
  std::unique_ptr<content::RenderFrameDeletedObserver>
  NavigateForBackForwardCache() {
    GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
    GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

    // 1) Navigate to A.
    content::RenderFrameHost* render_frame_host_a =
        ui_test_utils::NavigateToURL(browser(), url_a);
    auto delete_observer_render_frame_host_a =
        std::make_unique<content::RenderFrameDeletedObserver>(
            render_frame_host_a);

    // 2) Navigate to B.
    content::RenderFrameHost* render_frame_host_b =
        ui_test_utils::NavigateToURL(browser(), url_b);

    // Ensure that |render_frame_host_a| is in the cache.
    EXPECT_FALSE(delete_observer_render_frame_host_a->deleted());
    EXPECT_NE(render_frame_host_a, render_frame_host_b);
    EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
    return delete_observer_render_frame_host_a;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Ensure that Back Forward is cleared on adding dynamic rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBackForwardCacheBrowserTest,
                       BackForwardCacheClearedOnAddingDynamicRules) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Now block requests to script.js.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("script.js");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  auto bfcache_render_frame_host_delete_observer =
      NavigateForBackForwardCache();
  const ExtensionId& extension_id = last_loaded_extension_id();

  // Add dynamic rule.
  rule.condition->url_filter = std::string("dynamic.com");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  // Expect that |render_frame_host_a| is destroyed as the cache would get
  // cleared due to addition of new rule.
  bfcache_render_frame_host_delete_observer->WaitUntilDeleted();
}

// Ensure that Back Forward is cleared on updating session rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBackForwardCacheBrowserTest,
                       BackForwardCacheClearedOnUpdatingSessionRules) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Now block requests to script.js.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("script.js");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  auto bfcache_render_frame_host_delete_observer =
      NavigateForBackForwardCache();
  const ExtensionId& extension_id = last_loaded_extension_id();

  // Add session-scoped rule to block requests to "session.example".
  rule.condition->url_filter = std::string("session.example");
  ASSERT_NO_FATAL_FAILURE(UpdateSessionRules(extension_id, {}, {rule}));

  // Expect that |render_frame_host_a| is destroyed as the cache would get
  // cleared due to addition of new rule.
  bfcache_render_frame_host_delete_observer->WaitUntilDeleted();
}

// Ensure that Back Forward is cleared on updating enabled rulesets.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBackForwardCacheBrowserTest,
                       BackForwardCacheClearedOnUpdatingEnabledRulesets) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  std::vector<TestRulesetInfo> rulesets;
  rulesets.emplace_back("ruleset_1", ToListValue({CreateGenericRule(1)}));
  rulesets.emplace_back("ruleset_2", ToListValue({CreateGenericRule(2)}),
                        false /* enabled */);

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, "test_extension", {} /* hosts */));

  auto bfcache_render_frame_host_delete_observer =
      NavigateForBackForwardCache();
  const ExtensionId& extension_id = last_loaded_extension_id();

  // Enable |ruleset_2|.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(extension_id, {}, {"ruleset_2"}));

  // Expect that |render_frame_host_a| is destroyed as the cache would get
  // cleared due to addition of new ruleset.
  bfcache_render_frame_host_delete_observer->WaitUntilDeleted();
}

// Ensure that Back Forward is cleared on new extension.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBackForwardCacheBrowserTest,
                       BackForwardCacheClearedOnAddExtension) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  auto bfcache_render_frame_host_delete_observer =
      NavigateForBackForwardCache();

  // Now block requests to script.js.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("script.js");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Expect that |render_frame_host_a| is destroyed as the cache would get
  // cleared due to addition of new rule.
  bfcache_render_frame_host_delete_observer->WaitUntilDeleted();
}

class DeclarativeNetRequestControllableResponseTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DeclarativeNetRequestControllableResponseTest() = default;

  void SetUpOnMainThread() override {
    // The controllable response handler needs to be set up first before the
    // test server can be started, which is why the SetUpOnMainThread from the
    // super class which starts the test server is called afterwards here.
    controllable_http_response_.emplace(embedded_test_server(),
                                        "/pages_with_script/index.html");
    DeclarativeNetRequestBrowserTest::SetUpOnMainThread();
  }

 protected:
  net::test_server::ControllableHttpResponse& controllable_http_response() {
    return *controllable_http_response_;
  }

 private:
  std::optional<net::test_server::ControllableHttpResponse>
      controllable_http_response_;
};

// Test that DNR actions from an extension are cleaned up when the extension is
// disabled. This fixes a crash/bug where DNR actions such as modifyHeaders
// which are created on the OnBeforeRequest phase when an extension was enabled
// are supposed to take effect in a later request stage but the extension may no
// longer be enabled at that point.
// Regression for crbug.com/40072083 which would've caused a crash in this test.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestControllableResponseTest,
                       EraseActionsOnExtensionDisabled) {
  // Create an extension with a rule that modifies response headers. This
  // extension will match its rule during OnBeforeRequest but the rule would not
  // be applied until OnHeadersReceived which is after when the request is sent
  // and the response has started.
  TestRule headers_rule = CreateModifyHeadersRule(
      kMinValidID, kMinValidPriority, "google.com", std::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("resp-header", "append", "resp-value")}));
  headers_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {headers_rule}, "extension_1", {URLPattern::kAllUrlsPattern}));
  const ExtensionId& extension_id = last_loaded_extension_id();

  const GURL page_url = embedded_test_server()->GetURL(
      "google.com", "/pages_with_script/index.html");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  controllable_http_response().WaitForRequest();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  controllable_http_response().Send(net::HTTP_OK, "text/html",
                                    "<html>Hello, World!</html>");

  // Disable the extension after the response has been sent but before the
  // request has finished.
  DisableExtension(extension_id);

  controllable_http_response().Done();

  // Block until the web contents have been fully loaded to be certain that the
  // web request event router finishes processing all events after the request
  // has finished, such as OnHeadersReceived, and that the extension will be
  // disabled at that point. The test should not crash at this point.
  // TODO(crbug.com/40072083): Follow up with verifying the values of response
  // headers. This may require initiating a request from a JS script injected
  // into the page.
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_FALSE(extension_service()->IsExtensionEnabled(extension_id));
}

class DNRMatchResponseHeadersBrowserTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DNRMatchResponseHeadersBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kDeclarativeNetRequestResponseHeaderMatching);
  }

 private:
  // TODO(crbug.com/40727004): Once feature is launched to stable and feature
  // flag can be removed, replace usages of this test class with just
  // DeclarativeNetRequestBrowserTest.
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedCurrentChannel current_channel_override_{version_info::Channel::DEV};
};

// Test that requests matching rules' response header conditions will be
// blocked.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest, BlockRequests) {
  struct {
    std::string filter;
    int id;
    std::string type;
    std::vector<TestHeaderCondition> response_header_condition;
  } rules_data[] = {
      // A rule that allows all requests to setcookie.com if the set-cookie
      // header is present with value "cookie=oreo".
      {"setcookie.com", 1, "allow",
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("set-cookie", {"cookie=oreo"}, {})})},
      // A rule that blocks all requests to setcookie.com if the set-cookie
      // header is present. Note that if both this and the allow rule above
      // matches a request, the allow rule has higher priority and will override
      // this rule due to its action type.
      {"setcookie.com", 2, "block",
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("set-cookie", {}, {})})},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule(rule_data.id);
    rule.action->type = rule_data.type;

    rule.condition->url_filter = rule_data.filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});

    rule.condition->response_headers = rule_data.response_header_condition;
    rules.push_back(std::move(rule));
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(std::move(rules)));

  struct {
    std::string hostname;
    std::string path;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      // First test case does not have a set-cookie header and will not match
      // with any rules.
      {"setcookie.com", "/pages_with_script/index.html", true},
      // Second test case will be blocked as it matches with the block rule with
      // id 2.
      {"setcookie.com", "/set-cookie?cookie=sugar", false},
      // Third test case will not be blocked since it matches with the allow
      // rule with id 1 which has higher "priority" than the block rule due to
      // its action type.
      {"setcookie.com", "/set-cookie?cookie=oreo", true}};

  // Verify that the extension correctly intercepts network requests.
  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    content::TestNavigationObserver nav_observer(web_contents());
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());

    auto expected_code = test_case.expect_main_frame_loaded
                             ? net::OK
                             : net::ERR_BLOCKED_BY_CLIENT;
    EXPECT_EQ(expected_code, nav_observer.last_net_error_code());
  }
}

// Ensures that any <img> elements blocked by the API are collapsed based on
// response header matching.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest, ImageCollapsed) {
  // Loads a page with an image and returns whether the image was collapsed.
  auto is_image_collapsed = [this](const std::string& host_name) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(host_name, "/image.html")));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    const std::string script = "!!window.imageCollapsed;";
    return content::EvalJs(web_contents(), script).ExtractBool();
  };

  // Initially the image shouldn't be collapsed.
  EXPECT_FALSE(is_image_collapsed("matchheader.com"));
  EXPECT_FALSE(is_image_collapsed("matchnoheader.com"));

  // Load an extension which blocks all images on matchheader.com if there
  // exists a set-cookie header, and all on matchnoheaders.com if there is no
  // set-cookie header. Note that the image.html path will not have a set-cookie
  // header.
  TestRule match_header_rule = CreateGenericRule(kMinValidID);
  match_header_rule.condition->url_filter = "matchheader.com";
  match_header_rule.condition->resource_types =
      std::vector<std::string>({"image"});
  match_header_rule.condition->response_headers =
      std::vector<TestHeaderCondition>(
          {TestHeaderCondition("set-cookie", {}, {})});

  TestRule match_no_header_rule = CreateGenericRule(kMinValidID + 1);
  match_no_header_rule.condition->url_filter = "matchnoheader.com";
  match_no_header_rule.condition->resource_types =
      std::vector<std::string>({"image"});
  match_no_header_rule.condition->excluded_response_headers =
      std::vector<TestHeaderCondition>(
          {TestHeaderCondition("set-cookie", {}, {})});

  std::vector<TestRule> rules;
  rules.push_back(std::move(match_header_rule));
  rules.push_back(std::move(match_no_header_rule));
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(std::move(rules)));

  // Verify that only `match_no_header_rule` will match the request and cause
  // the image to be collapsed.
  EXPECT_FALSE(is_image_collapsed("matchheader.com"));
  EXPECT_TRUE(is_image_collapsed("matchnoheader.com"));
}

// Test that requests can be redirected and upgraded based on response headers.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest, RedirectAndUpgrade) {
  GURL match_header_url = embedded_test_server()->GetURL(
      "matchheader.com", "/pages_with_script/index.html");
  GURL denied_url = embedded_test_server()->GetURL(
      "denied.com", "/pages_with_script/index.html");
  GURL upgraded_url = embedded_test_server()->GetURL(
      "upgraded.com", "/pages_with_script/index.html");

  struct {
    std::string filter;
    int id;
    int priority;
    std::string type;
    std::optional<std::string> redirect_url;
    std::optional<std::vector<TestHeaderCondition>> response_header_condition;
  } rules_data[] = {
      // A rule for requests made to "matchheader.com" which redirects to
      // `denied_url` if a set-cookie header is present but its value is not
      // equal to "upgrade-token=valid".
      {"matchheader.com", 1, kMinValidPriority, "redirect", denied_url.spec(),
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("set-cookie", {}, {"upgrade-token=valid"})})},

      // A rule for requests made to "matchheader.com" which upgrades from HTTP
      // to HTTPS if a set-cookie header is present with a value of
      // "upgrade-token=valid".
      {"matchheader.com", 2, kMinValidPriority, "upgradeScheme", std::nullopt,
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("set-cookie", {"upgrade-token=valid"}, {})})},

      // A rule which redirects all URLs with https scheme to `upgraded_url`.
      // Used to test if requests have been upgraded.
      // TODO(kelvinjiang): See if we can eliminate this rule by using
      // https_server().
      {"|https*", 3, kMinValidPriority + 100, "redirect", upgraded_url.spec(),
       std::nullopt}};

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule(rule_data.id);
    rule.action->type = rule_data.type;
    rule.priority = rule_data.priority;

    rule.condition->url_filter = rule_data.filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});

    if (rule_data.redirect_url) {
      rule.action->redirect.emplace();
      rule.action->redirect->url = rule_data.redirect_url;
    }

    if (rule_data.response_header_condition) {
      rule.condition->response_headers = rule_data.response_header_condition;
    }
    rules.push_back(std::move(rule));
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  struct {
    std::string hostname;
    std::string path;
    GURL expected_final_url;
  } test_cases[] = {
      // Test a request that does not have a set-cookie header, so no rules
      // match and a redirect does not happen.
      {"matchheader.com", "/pages_with_script/index.html", match_header_url},

      // Test a request that does not have a "valid" upgrade token cookie, so it
      // gets redirected.
      {"matchheader.com", "/set-cookie?upgrade-token=other", denied_url},

      // Test a request that has a "valid" upgrade token that will match with
      // the upgrade rule with id 2: HTTPS requests can be verified in this
      // testing environment by redirecting them to `upgraded_url`.
      {"matchheader.com", "/set-cookie?upgrade-token=valid", upgraded_url},
  };

  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

    const GURL& final_url = web_contents()->GetLastCommittedURL();
    EXPECT_EQ(test_case.expected_final_url, final_url);
  }
}

// Test interactions between rules that match in the onBeforeRequest phase vs
// the onHeadersReceived phase.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest,
                       DifferentRequestPhaseRuleInteractions) {
  GURL redirected_url = embedded_test_server()->GetURL(
      "redirected.com", "/pages_with_script/index.html");

  struct {
    std::string filter;
    int id;
    int priority;
    std::string type;
    std::optional<std::string> redirect_url = std::nullopt;
    std::optional<std::vector<TestHeaderCondition>>
        excluded_response_header_condition = std::nullopt;
  } rules_data[] = {
      // For google.com, there is a block rule in onBeforeRequest and a redirect
      // rule in onHeadersReceived. The block rule should take precedence even
      // though the redirect rule has higher priority, because
      // the block rule will block the request in an earlier request stage than
      // when the redirect rule will be matched.
      {"google.com", 1, kMinValidPriority, "block"},
      {"google.com", 2, kMinValidPriority + 10, "redirect",
       redirected_url.spec(),
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("nonsense-header", {}, {})})},

      // For example.com, there is a redirect rule in onBeforeRequest and a
      // block rule in onHeadersReceived. The redirect rule should take
      // precedence because it redirects the request in an earlier request stage
      // than when the block rule would be matched.
      {"example.com", 3, kMinValidPriority, "redirect", redirected_url.spec()},
      {"example.com", 4, kMinValidPriority + 10, "block", std::nullopt,
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("nonsense-header", {}, {})})},

      // For abc.com, there is an allow rule in onBeforeRequest and a block rule
      // in onHeadersReceived. The block rule should take precedence because its
      // priority exceeds that of the allow rule's.
      {"abc.com", 5, kMinValidPriority, "allow"},
      {"abc.com", 6, kMinValidPriority + 10, "block", std::nullopt,
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("nonsense-header", {}, {})})},

      // For def.com, there is an allow rule in onBeforeRequest and a block rule
      // in onHeadersReceived. The allow rule should take precedence because its
      // priority exceeds that of the block rule's even though the allow rule is
      // matched in an earlier request stage.
      {"def.com", 7, kMinValidPriority + 10, "allow"},
      {"def.com", 8, kMinValidPriority, "block", std::nullopt,
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("nonsense-header", {}, {})})},

      // For ghi.com, there is a block rule in onBeforeRequest and an allow rule
      // with a higher priority in onHeadersReceived. Any request to ghi.com
      // should be blocked because there is enough info to match the block rule
      // in onBeforeRequest, without the request ever continuing onto later
      // stages.
      {"ghi.com", 9, kMinValidPriority, "block"},
      {"ghi.com", 10, kMinValidPriority + 10, "allow", std::nullopt,
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("nonsense-header", {}, {})})},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule(rule_data.id);
    rule.priority = rule_data.priority;
    rule.action->type = rule_data.type;

    rule.condition->url_filter = rule_data.filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});

    if (rule_data.redirect_url) {
      rule.action->redirect.emplace();
      rule.action->redirect->url = rule_data.redirect_url;
    }

    if (rule_data.excluded_response_header_condition) {
      rule.condition->excluded_response_headers =
          rule_data.excluded_response_header_condition;
    }
    rules.push_back(std::move(rule));
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  auto get_url = [this](const std::string& hostname) {
    return embedded_test_server()->GetURL(hostname,
                                          "/pages_with_script/index.html");
  };

  GURL google_url = embedded_test_server()->GetURL(
      "google.com", "/pages_with_script/index.html");
  GURL example_url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html");
  struct {
    std::string hostname;
    std::string path;
    bool expect_main_frame_loaded;
    GURL expected_final_url;
  } test_cases[] = {
      // The request should be blocked since rule with id 1 should take action
      // in the onBeforeRequest stage and rule with id 2 should never be
      // matched.
      {"google.com", "/pages_with_script/index.html", false, google_url},

      // The request should be redirected since rule with id 3 should take
      // action in the onBeforeRequest stage and rule with id 4 should never be
      // matched.
      {"example.com", "/pages_with_script/index.html", true, redirected_url},

      // The request should be blocked since the block rule with id 6 has a
      // higher priority than the allow rule with id 5 even though both rules
      // match.
      {"abc.com", "/pages_with_script/index.html", false, get_url("abc.com")},

      // The request should not be blocked since the allow rule with id 7 has a
      // higher priority than the block rule with id 8 which prevents the block
      // rule from taking effect.
      {"def.com", "/pages_with_script/index.html", true, get_url("def.com")},

      // The request should be blocked since the block rule with id 9 blocks the
      // request after matching with it in the onBeforeRequest stage, so it's
      // never sent and won't enter subsequent request stages which contain the
      // allow rule with id 10.
      {"ghi.com", "/pages_with_script/index.html", false, get_url("ghi.com")},
  };

  // Verify that the extension correctly intercepts network requests.
  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
    const GURL& final_url = web_contents()->GetLastCommittedURL();
    EXPECT_EQ(test_case.expected_final_url, final_url);
  }
}

// Test that frames where response header matched allowAllRequests rules will
// prevent lower priority matched rules from taking action on requests made
// under these frames.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest, AllowAllRequests) {
  std::vector<TestHeaderCondition> blank_header_condition =
      std::vector<TestHeaderCondition>(
          {TestHeaderCondition("nonsense-header", {}, {})});

  struct RuleData {
    int id;
    int priority;
    std::string action_type;
    std::string filter;
    bool is_regex_rule = false;
    std::optional<std::vector<std::string>> resource_types = std::nullopt;
    std::optional<std::vector<TestHeaderCondition>>
        excluded_response_header_condition = std::nullopt;
  } rule_data[] = {
      // Create 3 allowAllRequests rules:
      // One with priority 3 that matches the main frame, in the onBeforeRequest
      // stage.
      {1, 3, "allowAllRequests", "page_with_two_frames.html", false,
       std::vector<std::string>({"main_frame"})},
      // One with priority 5 that matches both frame 1 and 2, in the
      // onHeadersReceived stage.
      {2, 5, "allowAllRequests", "frame=(1|2)", true,
       std::vector<std::string>({"sub_frame"}), blank_header_condition},
      // One with priority 7 that matches frame 2, in the onBeforeRequest stage.
      {3, 7, "allowAllRequests", "frame=2", true,
       std::vector<std::string>({"sub_frame"})},

      // Create 3 block rules, each blocking only a particular image file, with
      // different priorities.
      {4, 2, "block", "image.png", true, std::vector<std::string>({"image"})},
      {5, 4, "block", "image_2.png", true, std::vector<std::string>({"image"})},
      {6, 6, "block", "image_3.png", true, std::vector<std::string>({"image"})},
  };

  std::vector<TestRule> rules;
  for (const auto& rule : rule_data) {
    TestRule test_rule = CreateGenericRule();
    test_rule.id = rule.id;
    test_rule.priority = rule.priority;
    test_rule.action->type = rule.action_type;
    test_rule.condition->url_filter.reset();
    if (rule.is_regex_rule) {
      test_rule.condition->regex_filter = rule.filter;
    } else {
      test_rule.condition->url_filter = rule.filter;
    }
    test_rule.condition->resource_types = rule.resource_types;

    if (rule.excluded_response_header_condition) {
      test_rule.condition->excluded_response_headers =
          rule.excluded_response_header_condition;
    }

    rules.push_back(std::move(test_rule));
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(std::move(rules)));

  // Navigate to a page with two sub frames.
  GURL page_url = embedded_test_server()->GetURL("example.com",
                                                 "/page_with_two_frames.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  // Attempts to add an image element for the image denoted by `image_path` to
  // the specified `frame`. Returns true if the image was successfully added and
  // loaded and false otherwise (e.g. if the image was not loaded/collapsed) if
  // it was blocked by a DNR rule.
  auto test_image_added = [](content::RenderFrameHost* frame,
                             const std::string& image_path) {
    const char kTestImageScript[] = R"(
      var img = document.createElement('img');
      new Promise(resolve => {
        img.addEventListener('load', () => {
          resolve(true);
        });
        img.addEventListener('error', () => {
          resolve(false);
        });

        img.src = '%s';
        document.body.appendChild(img);
      });
    )";

    return content::EvalJs(
               frame, base::StringPrintf(kTestImageScript, image_path.c_str()))
        .ExtractBool();
  };

  // Attempt to add images onto the main frame. Only `image.png` should be added
  // since the allowAllRequests rule with id 1 exceeds the priority of the block
  // rule for `image.png`.
  content::RenderFrameHost* main_frame = GetPrimaryMainFrame();
  EXPECT_TRUE(test_image_added(main_frame, "subresources/image.png"));
  EXPECT_FALSE(test_image_added(main_frame, "subresources/image_2.png"));
  EXPECT_FALSE(test_image_added(main_frame, "subresources/image_3.png"));

  // Attempt to add images onto `frame_1`. `image.png` and `image_2.png` should
  // be added since the max priority matching allowAllRequests rule for
  // `frame_1` has id 2 (it "wins" over the rule with id 1 that has a lower
  // priority), and its priority exceeds that of the block rules for `image.png`
  // and `image_2.png`.
  content::RenderFrameHost* frame_1 = GetFrameByName("frame1");
  EXPECT_TRUE(test_image_added(frame_1, "subresources/image.png"));
  EXPECT_TRUE(test_image_added(frame_1, "subresources/image_2.png"));
  EXPECT_FALSE(test_image_added(frame_1, "subresources/image_3.png"));

  // Attempt to add images onto `frame_2`. All 3 images should be loaded since
  // the max priority allowAllRequests rule for this frame out-prioritizes all
  // block rules.
  content::RenderFrameHost* frame_2 = GetFrameByName("frame2");
  EXPECT_TRUE(test_image_added(frame_2, "subresources/image.png"));
  EXPECT_TRUE(test_image_added(frame_2, "subresources/image_2.png"));
  EXPECT_TRUE(test_image_added(frame_2, "subresources/image_3.png"));
}

// Test that an onBeforeRequest block rule for a frame will still override an
// onHeadersReceived allowAllRequests rule with a higher priority.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest,
                       AllowAllRequests_BlockedFrame) {
  // Add a block rule for google.com sub frames.
  TestRule block_frame_rule = CreateGenericRule(kMinValidID);
  block_frame_rule.action->type = "block";
  block_frame_rule.condition->url_filter = "google.com";
  block_frame_rule.condition->resource_types =
      std::vector<std::string>({"sub_frame"});

  // Add an allowAllRequests rule that is matched in onHeadersReceived, for all
  // sub frames.
  TestRule allow_frame_rule = CreateGenericRule(kMinValidID + 1);
  allow_frame_rule.priority = kMinValidPriority + 22;
  allow_frame_rule.action->type = "allowAllRequests";
  allow_frame_rule.condition->url_filter = "|https*";
  allow_frame_rule.condition->resource_types =
      std::vector<std::string>({"sub_frame"});
  allow_frame_rule.condition->excluded_response_headers =
      std::vector<TestHeaderCondition>(
          {TestHeaderCondition("nonsense-header", {}, {})});

  std::vector<TestRule> rules;
  rules.push_back(std::move(block_frame_rule));
  rules.push_back(std::move(allow_frame_rule));

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(std::move(rules)));

  const GURL page_url = embedded_test_server()->GetURL(
      "example.com", "/page_with_two_frames.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  const std::string kFrameName1 = "frame1";
  const std::string kFrameName2 = "frame2";

  // Navigate frame1 to example.com: it shouldn't be collapsed since
  // `block_frame_rule` does not match it.
  NavigateFrame(kFrameName1,
                embedded_test_server()->GetURL(
                    "example.com", "/pages_with_script/index.html"));

  // Navigate frame2 to google.com: it should be collapsed since
  // `block_frame_rule` matches it and prevents `allow_frame_rule` from matching
  // despite the latter rule's higher priority.
  NavigateFrame(kFrameName2,
                embedded_test_server()->GetURL(
                    "google.com", "/pages_with_script/index.html"));

  TestFrameCollapse(kFrameName1, false);
  TestFrameCollapse(kFrameName2, true);
}

// Verify that getMatchedRules returns the correct rule matches for rules which
// match on response headers.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest,
                       GetMatchedRules_SingleExtension) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  std::vector<TestHeaderCondition> blank_header_condition =
      std::vector<TestHeaderCondition>(
          {TestHeaderCondition("nonsense-header", {}, {})});

  std::vector<TestHeaderInfo> blank_req_header_action =
      std::vector<TestHeaderInfo>({TestHeaderInfo("req-header", "set", "val")});

  std::vector<TestHeaderInfo> blank_resp_header_action =
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("resp-header", "append", "val")});

  struct RuleData {
    int id;
    int priority;
    std::string action_type;
    std::string filter;
    std::optional<std::vector<TestHeaderCondition>> header_condition =
        std::nullopt;
    std::optional<std::vector<TestHeaderInfo>> request_headers = std::nullopt;
    std::optional<std::vector<TestHeaderInfo>> response_headers = std::nullopt;
  } rule_data[] = {
      // Used for all sub-tests. Allows all main-frame requests at the
      // onHeadersReceived stage.
      {1, 100, "allow", "*", blank_header_condition},

      // Used for sub-test 1.
      {2, 1, "block", "a.test"},

      // Used for sub-test 2.
      {3, 1, "allow", "b.test"},
      {4, 99, "block", "b.test", blank_header_condition},

      // Used for sub-test 3.
      {5, 1000, "allow", "c.test"},
      {6, 1, "block", "c.test", blank_header_condition},
      {7, 1001, "block", "c.test2", blank_header_condition},

      // Used for sub-test 4.
      {8, 1000, "modifyHeaders", "d.test", std::nullopt,
       blank_req_header_action},

      // Used for sub-test 5.
      {9, 1, "modifyHeaders", "e.test", std::nullopt, std::nullopt,
       blank_resp_header_action},
      {10, 200, "modifyHeaders", "e.test2", std::nullopt, std::nullopt,
       blank_resp_header_action},

      // Used for sub-test 6.
      {11, 1, "modifyHeaders", "f.test", blank_header_condition, std::nullopt,
       blank_resp_header_action},
      {12, 200, "modifyHeaders", "f.test2", blank_header_condition,
       std::nullopt, blank_resp_header_action},

      // Used for sub-test 7.
      {13, 1000, "modifyHeaders", "g.test", std::nullopt,
       blank_req_header_action},
      {14, 102, "allow", "g.test"},
      {15, 101, "block", "g.test", blank_header_condition},
  };

  std::vector<TestRule> rules;
  for (const auto& rule : rule_data) {
    TestRule test_rule = CreateGenericRule();
    test_rule.id = rule.id;
    test_rule.priority = rule.priority;
    test_rule.action->type = rule.action_type;
    test_rule.action->request_headers = rule.request_headers;
    test_rule.action->response_headers = rule.response_headers;

    test_rule.condition->url_filter.reset();
    test_rule.condition->url_filter = rule.filter;
    test_rule.condition->resource_types =
        std::vector<std::string>({"main_frame"});
    test_rule.condition->excluded_response_headers = rule.header_condition;

    rules.push_back(std::move(test_rule));
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      std::move(rules), "test_extension", {URLPattern::kAllUrlsPattern}));

  struct {
    std::string hostname;
    std::string expected_matched_rules;
  } test_cases[] = {
      // Sub-test 1:
      // The request is blocked before making it to onHeadersReceived so only
      // rule 2 (block) should match.
      {"a.test", "2"},

      // Sub-test 2:
      // Both allow rules from both phases (1 and 3) should match since rule 1
      // supercedes rule 3 based on priority in the onHeadersReceived phase.
      // Note that rule 1 prevents rule 4 from blocking the request based on
      // priority.
      {"b.test", "1,3"},

      // Sub-test 3:
      // For the first request, only rule 5 is matched since it outprioritizes
      // rule 1 as an allow rule and prevents rule 6 from blocking the request.
      // For the second request, rule 7 outprioritizes rule 1 and 5 and blocks
      // the request but both 5 and 7 are matched since they operate on
      // different request stages.
      {"c.test", "5"},
      {"c.test2", "5,7"},

      // Sub-test 4:
      // Rule 8 only modifies request headers so it is matched in the
      // onBeforeSendHeaders phase, which is before rule 1 is matched.
      {"d.test", "1,8"},

      // Sub-test 5:
      // For the first request, rule 9 is outprioritized by rule 1 so it will
      // not modify response headers, and rule 1 matches.
      // For the second request, rule 10 DOES outprioritize rule 1 and modifies
      // response headers, so only it matches.
      {"e.test", "1"},
      {"e.test2", "10"},

      // Sub-test 6:
      // Same as sub-test 5 except the modify header rules now match on response
      // header conditions in the onHeadersReceived phase.
      {"f.test", "1"},
      {"f.test2", "12"},

      // Sub-test 7:
      // In OnBeforeRequest, rule 13 (modify request headers) matches since it
      // outprioritizes rule 14 (allow). However, rule 14 carries over to
      // OnHeadersReceived where it outprioritizes both rules 1 and 15 (it
      // prevents the latter rule from blocking the request) so it is matched.
      {"g.test", "13,14"},
  };

  for (const auto& test_case : test_cases) {
    base::Time start_time = base::Time::Now();
    GURL url = embedded_test_server()->GetURL(test_case.hostname,
                                              "/pages_with_script/index.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    // Get the rule ids matched for the navigation request sent above.
    std::string actual_rules_matched =
        GetRuleIdsMatched(last_loaded_extension_id(), start_time);

    EXPECT_EQ(test_case.expected_matched_rules, actual_rules_matched);
  }
}

// Verify that getMatchedRules returns the correct rule matches for rules which
// match on response headers between different extensions.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest,
                       GetMatchedRules_MultipleExtensions) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  auto create_rule = [](int id, int priority, const std::string& action_type,
                        const std::string& filter, bool headers_received_rule) {
    TestRule test_rule = CreateGenericRule();
    test_rule.id = id;
    test_rule.priority = priority;
    test_rule.action->type = action_type;

    test_rule.condition->url_filter.reset();
    test_rule.condition->url_filter = filter;
    test_rule.condition->resource_types =
        std::vector<std::string>({"main_frame"});

    if (headers_received_rule) {
      test_rule.condition->excluded_response_headers =
          std::vector<TestHeaderCondition>(
              {TestHeaderCondition("nonsense-header", {}, {})});
    }

    return test_rule;
  };

  auto before_request_allow = create_rule(1, 100, "allow", "google.com", false);
  auto headers_received_allow = create_rule(2, 10, "allow", "google", true);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {std::move(before_request_allow), std::move(headers_received_allow)},
      "test_extension", {}));
  auto extension_1_id = last_loaded_extension_id();

  auto extension_2_allow = create_rule(3, 1, "allow", "google.xyz", false);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({std::move(extension_2_allow)},
                                                 "test_extension_2", {}));
  auto extension_2_id = last_loaded_extension_id();

  // TODO(crbug.com/40727004): Expand this test by introducing a block rule from
  // extension 1 that would've been bypassed by `before_request_allow` in the
  // onHeadersReceived phase.
  struct {
    std::string hostname;
    std::string ext_1_expected_matched_rules;
    std::string ext_2_expected_matched_rules;
  } test_cases[] = {
      // This request only matches `before_request_allow`.
      {"google.com", "1", ""},

      // In onBeforeRequest, `extension_2_allow` takes precedence over
      // `before_request_allow` since extension 2 was more recently installed.
      // Once the request reaches onHeadersReceived, `headers_received_allow`
      // matches, but only `extension_2_allow` should be tracked since it
      // carries over to onHeadersReceived and outprioritizes
      // `headers_received_allow`.
      {"google.xyz", "", "3"},
  };

  for (const auto& test_case : test_cases) {
    base::Time start_time = base::Time::Now();
    GURL url = embedded_test_server()->GetURL(test_case.hostname,
                                              "/pages_with_script/index.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    // Get the rule ids matched for both extensions for the navigation request
    // sent above.
    std::string ext_1_rules_matched =
        GetRuleIdsMatched(extension_1_id, start_time);
    EXPECT_EQ(test_case.ext_1_expected_matched_rules, ext_1_rules_matched);

    std::string ext_2_rules_matched =
        GetRuleIdsMatched(extension_2_id, start_time);
    EXPECT_EQ(test_case.ext_2_expected_matched_rules, ext_2_rules_matched);
  }
}

// Test that modifyHeaders rules matched in both onBeforeRequest and
// onHeadersReceived phases will perform the correct action(s) on the request.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest,
                       ModifyHeaders_SingleExtension) {
  std::vector<TestHeaderCondition> blank_header_condition =
      std::vector<TestHeaderCondition>(
          {TestHeaderCondition("nonsense-header", {}, {})});

  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  struct RuleData {
    int id;
    int priority;
    std::string action_type;
    std::string filter;
    std::optional<std::vector<TestHeaderInfo>> response_headers_action =
        std::nullopt;
    std::optional<std::vector<TestHeaderCondition>> exclude_header_condition =
        std::nullopt;
    std::optional<std::vector<TestHeaderCondition>> header_condition =
        std::nullopt;
  } rule_data[] = {
      // Used for sub-test 1, all parts.
      {1, 2, "modifyHeaders", "a.test",
       std::vector<TestHeaderInfo>(
           {TestHeaderInfo("set-cookie", "set", "key1=val1")})},
      {2, 1, "modifyHeaders", "a.test",
       std::vector<TestHeaderInfo>(
           {TestHeaderInfo("set-cookie", "set", "key2=val2")}),
       blank_header_condition},

      // Used for sub-test 1, part 2.
      {3, 4, "modifyHeaders", "a.test2",
       std::vector<TestHeaderInfo>(
           {TestHeaderInfo("set-cookie", "remove", std::nullopt)}),
       blank_header_condition},

      // Used for sub-test 2, all parts.
      {4, 5, "modifyHeaders", "b.test",
       std::vector<TestHeaderInfo>(
           {TestHeaderInfo("set-cookie", "append", "key4=val4")})},
      {5, 3, "modifyHeaders", "b.test",
       std::vector<TestHeaderInfo>(
           {TestHeaderInfo("set-cookie", "append", "key5=val5")}),
       blank_header_condition},
      {6, 1, "modifyHeaders", "b.test",
       std::vector<TestHeaderInfo>(
           {TestHeaderInfo("set-cookie", "append", "key6=val6")})},

      // Used for sub-test 2, part 2.
      {7, 4, "allow", "b.test2"},

      // Used for sub-test 2, part 3. Note that this rule will match on the
      // original value of response headers, before they are modified by
      // extensions.
      {8, 2, "allow", "b.test3", std::nullopt, std::nullopt,
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("set-cookie", {"orig-key=val"}, {})})},
  };

  std::vector<TestRule> rules;
  for (const auto& rule : rule_data) {
    TestRule test_rule = CreateGenericRule();
    test_rule.id = rule.id;
    test_rule.priority = rule.priority;
    test_rule.action->type = rule.action_type;
    if (rule.action_type == "modifyHeaders") {
      test_rule.action->response_headers = rule.response_headers_action;
    }

    test_rule.condition->url_filter.reset();
    test_rule.condition->url_filter = rule.filter;
    test_rule.condition->resource_types =
        std::vector<std::string>({"main_frame"});
    test_rule.condition->response_headers = rule.header_condition;
    test_rule.condition->excluded_response_headers =
        rule.exclude_header_condition;

    rules.push_back(std::move(test_rule));
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      std::move(rules), "test_extension", {URLPattern::kAllUrlsPattern}));

  struct {
    std::string hostname;
    std::string expected_matched_rules;
    std::string expected_set_cookie_value;
  } test_cases[] = {
      // Sub-test 1: Test that even if rules are matched across different
      // request phases, the highest priority rule takes precedence in modifying
      // headers.

      // Part 1: rule 1 (matched in onBeforeRequest) sets the header.
      {"a.test", "1", "key1=val1"},

      // Part 2: rule 3 (matched in onHeadersReceived) removes the header.
      {"a.test2", "3", ""},

      // Sub-test 2: Test that multiple header actions are applied in descending
      // order of priority, and that matched allow rules will prevent lower
      // priority modifyHeaders rules from taking effect, even if these matches
      // happen in different request phases.

      // Part 1: Rules 4, 5 and 6 match the request and their append operations
      // are applied onto the header with the highest priority rule (rule 4)
      // being applied first.
      {"b.test", "4,5,6", "orig-key=val; key4=val4; key5=val5; key6=val6"},

      // Part 2: Rule 7 (matched in onBeforeRequest) outprioritizes rules 5 and
      // 6.
      {"b.test2", "4", "orig-key=val; key4=val4"},

      // Part 3: Rule 8 (matched in onHeadersReceived) outprioritizes rule 6.
      {"b.test3", "4,5", "orig-key=val; key4=val4; key5=val5"},
  };

  for (const auto& test_case : test_cases) {
    base::Time start_time = base::Time::Now();

    auto set_cookie_url = embedded_test_server()->GetURL(
        test_case.hostname, "/set-cookie?orig-key=val");

    SCOPED_TRACE(
        base::StringPrintf("Testing %s", set_cookie_url.spec().c_str()));

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), set_cookie_url));

    // Get the rule ids matched for the navigation request sent above.
    std::string actual_rules_matched =
        GetRuleIdsMatched(last_loaded_extension_id(), start_time);
    EXPECT_EQ(test_case.expected_matched_rules, actual_rules_matched);

    // Verify the value of the set-cookie header.
    EXPECT_EQ(test_case.expected_set_cookie_value, GetPageCookie());
  }
}

// Test inter-extension interactions for modifyHeaders rules matched in both
// onBeforeRequest and onHeadersReceived phases.
IN_PROC_BROWSER_TEST_P(DNRMatchResponseHeadersBrowserTest,
                       ModifyHeaders_MultipleExtensions) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  auto create_rule =
      [](int id, int priority, const std::string& filter,
         bool headers_received_rule,
         const std::vector<TestHeaderInfo>& response_headers_action) {
        TestRule test_rule = CreateGenericRule();
        test_rule.id = id;
        test_rule.priority = priority;
        test_rule.action->type = "modifyHeaders";
        test_rule.action->response_headers = response_headers_action;

        test_rule.condition->url_filter.reset();
        test_rule.condition->url_filter = filter;
        test_rule.condition->resource_types =
            std::vector<std::string>({"main_frame"});

        if (headers_received_rule) {
          test_rule.condition->excluded_response_headers =
              std::vector<TestHeaderCondition>(
                  {TestHeaderCondition("nonsense-header", {}, {})});
        }

        return test_rule;
      };

  auto ext_1_append =
      create_rule(1, 100, "google", false,
                  {TestHeaderInfo("set-cookie", "append", "key1=ext1")});
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({std::move(ext_1_append)}, "test_extension",
                             {URLPattern::kAllUrlsPattern}));
  auto extension_1_id = last_loaded_extension_id();

  auto ext_2_append =
      create_rule(2, 1, "google.ca", true,
                  {TestHeaderInfo("set-cookie", "append", "key2=ext2")});
  auto ext_2_remove =
      create_rule(3, 1, "google.xyz", true,
                  {TestHeaderInfo("set-cookie", "remove", std::nullopt)});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {std::move(ext_2_append), std::move(ext_2_remove)}, "test_extension_2",
      {URLPattern::kAllUrlsPattern}));
  auto extension_2_id = last_loaded_extension_id();

  struct {
    std::string hostname;
    std::string ext_1_expected_matched_rules;
    std::string ext_2_expected_matched_rules;
    std::string expected_set_cookie_value;
  } test_cases[] = {
      // Only `ext_1_append` matches.
      {"google.com", "1", "", "orig-key=val; key1=ext1"},

      // Both `ext_1_append` and `ext_2_append` match, with `ext_2_append`
      // taking precedence and adding to the header first since its extension
      // was more recently installed.
      {"google.ca", "1", "2", "orig-key=val; key2=ext2; key1=ext1"},

      // Only `ext_2_remove` is applied since it takes precedence over
      // `ext_1_append` (and prevents `ext_1_append` from adding to the header)
      // since its extension was more recently installed.
      {"google.xyz", "", "3", ""},
  };

  for (const auto& test_case : test_cases) {
    base::Time start_time = base::Time::Now();
    auto set_cookie_url = embedded_test_server()->GetURL(
        test_case.hostname, "/set-cookie?orig-key=val");

    SCOPED_TRACE(
        base::StringPrintf("Testing %s", set_cookie_url.spec().c_str()));

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), set_cookie_url));

    // Get the rule ids matched for both extensions for the navigation request
    // sent above.
    std::string ext_1_rules_matched =
        GetRuleIdsMatched(extension_1_id, start_time);
    EXPECT_EQ(test_case.ext_1_expected_matched_rules, ext_1_rules_matched);

    std::string ext_2_rules_matched =
        GetRuleIdsMatched(extension_2_id, start_time);
    EXPECT_EQ(test_case.ext_2_expected_matched_rules, ext_2_rules_matched);

    // Verify the value of the set-cookie header.
    EXPECT_EQ(test_case.expected_set_cookie_value, GetPageCookie());
  }
}

// Fixture for use by tests that care about the --extensions-on-chrome-urls
// switch.
using DeclarativeNetRequestAllowChromeURLsBrowserTest =
    DeclarativeNetRequestBrowserTest;

// Ensure that an extension can block requests that it initiated, but not
// requests that other extensions initiated, unless the
// --extensions-on-chrome-urls switch is used.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestAllowChromeURLsBrowserTest,
                       CrossExtensionRequestBlocking) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission |
                   ConfigFlag::kConfig_HasManifestSandbox);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  constexpr char kFetchTemplate[] = R"(
    fetch('%s').then(response =>
      response.ok ? 'success' : ('Unexpected Server Error: ' + response.status)
    ).catch(e =>
      'failed'
    ).then(result => {
      // EvalJs uses the return value, but
      // ExecuteScriptInBackgroundPageAndReturnString waits for the
      // sendScriptResult call. Both are needed here since EvalJs can't be used
      // for the background ServiceWorkers and
      // ExecuteScriptInBackgroundPageAndReturnString can't be used for the
      // manifest sandbox pages.
      if (typeof chrome?.test?.sendScriptResult === 'function') {
        chrome.test.sendScriptResult(result);
      }
      return result;
    });
  )";

  // Extension 1 - Blocks requests to google.com.
  TestRule blocking_rule = CreateGenericRule(kMinValidID);
  blocking_rule.id = 1;
  blocking_rule.action->type = "block";
  blocking_rule.condition->url_filter = "google.com";
  blocking_rule.condition->resource_types =
      std::vector<std::string>({"xmlhttprequest"});

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({std::move(blocking_rule)}, "test_extension_1",
                             {URLPattern::kAllUrlsPattern}));
  const Extension* extension_1 = last_loaded_extension();

  content::RenderFrameHost* extension_1_sandbox =
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), extension_1->GetResourceURL(kManifestSandboxPageFilepath),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Extension 2 - Doesn't block any requests.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {}, "test_extension_2", {URLPattern::kAllUrlsPattern}));
  const Extension* extension_2 = last_loaded_extension();

  content::RenderFrameHost* extension_2_sandbox =
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), extension_2->GetResourceURL(kManifestSandboxPageFilepath),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  struct {
    std::string hostname;
    bool rule_matches;
  } test_cases[] = {{"google.com", true}, {"google.ca", false}};

  for (const auto& test_case : test_cases) {
    base::Time start_time = base::Time::Now();

    GURL fetch_url =
        embedded_test_server()->GetURL(test_case.hostname, "/cors-ok.txt");

    SCOPED_TRACE(
        base::StringPrintf("Testing %s, (Cross-extension blocking allowed: %d)",
                           fetch_url.spec().c_str(), GetAllowChromeURLs()));

    std::string script =
        base::StringPrintf(kFetchTemplate, fetch_url.spec().c_str());

    bool expected_same_extension_blocked = test_case.rule_matches;
    bool expected_cross_extension_blocked =
        test_case.rule_matches && GetAllowChromeURLs();
    std::string expected_same_extension_outcome =
        expected_same_extension_blocked ? "failed" : "success";
    std::string expected_cross_extension_outcome =
        expected_cross_extension_blocked ? "failed" : "success";
    // Each request is attempted twice, once from the extension's background
    // ServiceWorker and again from the extension's manifest sandbox page. The
    // blocking rule will match both or neither, so the match count is always
    // incremented by two.
    std::string expected_match_count =
        base::NumberToString((expected_same_extension_blocked ? 2 : 0) +
                             (expected_cross_extension_blocked ? 2 : 0));

    // Test requests from the extension background pages.
    std::string actual_same_extension_outcome =
        ExecuteScriptInBackgroundPageAndReturnString(extension_1->id(), script);
    std::string actual_cross_extension_outcome =
        ExecuteScriptInBackgroundPageAndReturnString(extension_2->id(), script);

    EXPECT_EQ(expected_same_extension_outcome, actual_same_extension_outcome);
    EXPECT_EQ(expected_cross_extension_outcome, actual_cross_extension_outcome);

    // Test requests from the extension manifest sandbox pages.
    std::string actual_same_extension_sandbox_outcome =
        content::EvalJs(extension_1_sandbox, script).ExtractString();
    std::string actual_cross_extension_sandbox_outcome =
        content::EvalJs(extension_2_sandbox, script).ExtractString();

    EXPECT_EQ(expected_same_extension_outcome,
              actual_same_extension_sandbox_outcome);
    EXPECT_EQ(expected_cross_extension_outcome,
              actual_cross_extension_sandbox_outcome);

    // Also check the rule match count looks right, should be either "4", "2" or
    // "0".
    EXPECT_EQ(expected_match_count,
              GetMatchedRuleCount(extension_1->id(), std::nullopt /* tab_id */,
                                  start_time));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestBrowserTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestHostPermissionsBrowserTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));
INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestResourceTypeBrowserTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));
INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestSubresourceWebBundlesBrowserTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestBrowserTest_Packed,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestBrowserTest_Unpacked,
    ::testing::Combine(::testing::Values(ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestGlobalRulesBrowserTest_Packed,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestAllowAllRequestsBrowserTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestWebTransportTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestBackForwardCacheBrowserTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestControllableResponseTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DNRMatchResponseHeadersBrowserTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeclarativeNetRequestAllowChromeURLsBrowserTest,
    ::testing::Combine(::testing::Values(ExtensionLoadType::PACKED,
                                         ExtensionLoadType::UNPACKED),
                       ::testing::Bool()));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
