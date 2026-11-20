// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/activity_log.h"

#include <stddef.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/scoped_isolated_mode_override_for_testing.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace {

const char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

constexpr auto kUrlApiCalls = std::to_array<const char*>({
    "HTMLButtonElement.formAction", "HTMLEmbedElement.src",
    "HTMLFormElement.action",       "HTMLFrameElement.src",
    "HTMLHtmlElement.manifest",     "HTMLIFrameElement.src",
    "HTMLImageElement.longDesc",    "HTMLImageElement.src",
    "HTMLImageElement.lowsrc",      "HTMLInputElement.formAction",
    "HTMLInputElement.src",         "HTMLLinkElement.href",
    "HTMLMediaElement.src",         "HTMLMediaElement.currentSrc",
    "HTMLModElement.cite",          "HTMLObjectElement.data",
    "HTMLQuoteElement.cite",        "HTMLScriptElement.src",
    "HTMLSourceElement.src",        "HTMLTrackElement.src",
    "HTMLVideoElement.poster",
});

}  // namespace

namespace extensions {

// Class that implements the binding of a new Renderer mojom interface and
// can receive callbacks on it for testing validation.
class InterceptingRendererStartupHelper : public RendererStartupHelper,
                                          public mojom::Renderer {
 public:
  explicit InterceptingRendererStartupHelper(
      content::BrowserContext* browser_context)
      : RendererStartupHelper(browser_context) {}

  // Values received by SetPolicyActivityLoggingEnabled() for the process with
  // `process_id`, in order.
  std::vector<bool> policy_activity_logging_updates(
      content::ChildProcessId process_id) const {
    auto it = policy_activity_logging_updates_.find(process_id);
    return it == policy_activity_logging_updates_.end() ? std::vector<bool>()
                                                        : it->second;
  }

 protected:
  mojo::PendingAssociatedRemote<mojom::Renderer> BindNewRendererRemote(
      content::RenderProcessHost* process) override {
    mojo::AssociatedRemote<mojom::Renderer> remote;
    receivers_.Add(this, remote.BindNewEndpointAndPassDedicatedReceiver(),
                   process->GetID());
    return remote.Unbind();
  }

 private:
  // mojom::Renderer implementation:
  void ActivateExtension(const std::string& extension_id) override {}
  void SetActivityLoggingEnabled(bool enabled) override {}
  void SetPolicyActivityLoggingEnabled(bool enabled) override {
    policy_activity_logging_updates_[receivers_.current_context()].push_back(
        enabled);
  }
  void LoadExtensions(
      std::vector<mojom::ExtensionLoadedParamsPtr> loaded_extensions) override {
  }
  void UnloadExtension(const std::string& extension_id) override {}
  void SuspendExtension(
      const std::string& extension_id,
      mojom::Renderer::SuspendExtensionCallback callback) override {
    std::move(callback).Run();
  }
  void CancelSuspendExtension(const std::string& extension_id) override {}
  void SetDeveloperMode(bool current_developer_mode) override {}
  void SetUserScriptsAllowed(const std::string& extension_id,
                             bool allowed) override {}
  void SetSessionInfo(version_info::Channel channel,
                      mojom::FeatureSessionType session) override {}
  void SetSystemFont(const std::string& font_family,
                     const std::string& font_size) override {}
  void SetWebViewPartitionID(const std::string& partition_id) override {}
  void SetScriptingAllowlist(
      const std::vector<std::string>& extension_ids) override {}
  void UpdateUserScriptWorlds(
      std::vector<mojom::UserScriptWorldInfoPtr> info) override {}
  void ClearUserScriptWorldConfig(
      const std::string& extension_id,
      const std::optional<std::string>& world_id) override {}
  void ShouldSuspend(ShouldSuspendCallback callback) override {
    std::move(callback).Run();
  }
  void TransferBlobs(TransferBlobsCallback callback) override {
    std::move(callback).Run();
  }
  void UpdatePermissions(const std::string& extension_id,
                         PermissionSet active_permissions,
                         PermissionSet withheld_permissions,
                         URLPatternSet policy_blocked_hosts,
                         URLPatternSet policy_allowed_hosts,
                         bool uses_default_policy_host_restrictions) override {}
  void UpdateDefaultPolicyHostRestrictions(
      URLPatternSet default_policy_blocked_hosts,
      URLPatternSet default_policy_allowed_hosts) override {}
  void UpdateUserHostRestrictions(URLPatternSet user_blocked_hosts,
                                  URLPatternSet user_allowed_hosts) override {}
  void UpdateTabSpecificPermissions(const std::string& extension_id,
                                    URLPatternSet new_hosts,
                                    int tab_id,
                                    bool update_origin_allowlist) override {}
  void UpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory,
                         mojom::HostIDPtr host_id) override {}
  void ClearTabSpecificPermissions(
      const std::vector<std::string>& extension_ids,
      int tab_id,
      bool update_origin_allowlist) override {}
  void WatchPages(const std::vector<std::string>& css_selectors) override {}

  mojo::AssociatedReceiverSet<mojom::Renderer, content::ChildProcessId>
      receivers_;
  std::map<content::ChildProcessId, std::vector<bool>>
      policy_activity_logging_updates_;
};

class ActivityLogTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual bool enable_activity_logging_switch() const { return true; }
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SetActivityLogTaskRunnerForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault().get());

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (enable_activity_logging_switch()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableExtensionActivityLogging);
    }
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogTesting);
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()))
        ->CreateExtensionService(&command_line, base::FilePath(), false);

    RendererStartupHelperFactory::GetForBrowserContext(profile())
        ->OnRenderProcessHostCreated(
            static_cast<content::RenderProcessHost*>(process()));

    base::RunLoop().RunUntilIdle();
  }

  static std::unique_ptr<KeyedService> BuildFakeRendererStartupHelper(
      content::BrowserContext* context) {
    return std::make_unique<InterceptingRendererStartupHelper>(context);
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        RendererStartupHelperFactory::GetInstance(),
        base::BindRepeating(&BuildFakeRendererStartupHelper)}};
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
    SetActivityLogTaskRunnerForTesting(nullptr);
  }

  static void RetrieveActions_LogAndFetchActions0(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(0u, i->size());
  }

  static void RetrieveActions_LogAndFetchActions1(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(1u, i->size());
  }

  static void RetrieveActions_LogAndFetchActions2(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(2u, i->size());
  }

  void SetPolicy(bool log_arguments) {
    ActivityLog* activity_log = ActivityLog::GetInstance(profile());
    if (log_arguments)
      activity_log->SetDatabasePolicy(ActivityLogPolicy::POLICY_FULLSTREAM);
    else
      activity_log->SetDatabasePolicy(ActivityLogPolicy::POLICY_COUNTS);
  }

  bool GetDatabaseEnabled() {
    ActivityLog* activity_log = ActivityLog::GetInstance(profile());
    return activity_log->IsDatabaseEnabled();
  }

  bool GetWatchdogActive() {
    ActivityLog* activity_log = ActivityLog::GetInstance(profile());
    return activity_log->IsWatchdogAppActive();
  }

  static void Arguments_Prerender(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(1U, i->size());
    scoped_refptr<Action> last = i->front();

    ASSERT_EQ("odlameecjipmbmbejkplpemijjgpljce", last->extension_id());
    ASSERT_EQ(Action::ACTION_CONTENT_SCRIPT, last->action_type());
    ASSERT_EQ("[\"script\"]",
              ActivityLogPolicy::Util::Serialize(last->args()));
    ASSERT_EQ("http://www.google.com/", last->SerializePageUrl());
    ASSERT_EQ("{\"prerender\":true}",
              ActivityLogPolicy::Util::Serialize(last->other()));
    ASSERT_EQ("", last->api_name());
    ASSERT_EQ("", last->page_title());
    ASSERT_EQ("", last->SerializeArgUrl());
  }

  static void RetrieveActions_ArgUrlExtraction(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(4U, i->size());
    scoped_refptr<Action> action = i->at(0);
    ASSERT_EQ("XMLHttpRequest.open", action->api_name());
    ASSERT_EQ("[\"POST\",\"\\u003Carg_url>\"]",
              ActivityLogPolicy::Util::Serialize(action->args()));
    ASSERT_EQ("http://api.google.com/", action->arg_url().spec());
    // Test that the dom_verb field was changed to XHR (from METHOD).  This
    // could be tested on all retrieved XHR actions but it would be redundant,
    // so just test once.
    ASSERT_TRUE(action->other());
    const base::DictValue& other = *action->other();
    std::optional<int> dom_verb =
        other.FindInt(activity_log_constants::kActionDomVerb);
    ASSERT_EQ(DomActionType::XHR, dom_verb);

    action = i->at(1);
    ASSERT_EQ("XMLHttpRequest.open", action->api_name());
    ASSERT_EQ("[\"POST\",\"\\u003Carg_url>\"]",
              ActivityLogPolicy::Util::Serialize(action->args()));
    ASSERT_EQ("http://www.google.com/api/", action->arg_url().spec());

    action = i->at(2);
    ASSERT_EQ("XMLHttpRequest.open", action->api_name());
    ASSERT_EQ("[\"POST\",\"/api/\"]",
              ActivityLogPolicy::Util::Serialize(action->args()));
    ASSERT_FALSE(action->arg_url().is_valid());

    action = i->at(3);
    ASSERT_EQ("windows.create", action->api_name());
    ASSERT_EQ("[{\"url\":\"\\u003Carg_url>\"}]",
              ActivityLogPolicy::Util::Serialize(action->args()));
    ASSERT_EQ("http://www.google.co.uk/", action->arg_url().spec());
  }

  static void RetrieveActions_ArgUrlApiCalls(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> actions) {
    size_t api_calls_size = std::size(kUrlApiCalls);
    ASSERT_EQ(api_calls_size, actions->size());

    for (size_t i = 0; i < actions->size(); i++) {
      scoped_refptr<Action> action = actions->at(i);
      ASSERT_EQ(kExtensionId, action->extension_id());
      ASSERT_EQ(Action::ACTION_DOM_ACCESS, action->action_type());
      ASSERT_EQ(kUrlApiCalls[i], action->api_name());
      ASSERT_EQ("[\"\\u003Carg_url>\"]",
                ActivityLogPolicy::Util::Serialize(action->args()));
      ASSERT_EQ("http://www.google.co.uk/", action->arg_url().spec());
      ASSERT_TRUE(action->other());
      const base::DictValue& other = *action->other();
      std::optional<int> dom_verb =
          other.FindInt(activity_log_constants::kActionDomVerb);
      ASSERT_EQ(DomActionType::SETTER, dom_verb);
    }
  }
};

TEST_F(ActivityLogTest, Construct) {
  ASSERT_TRUE(GetDatabaseEnabled());
  ASSERT_FALSE(GetWatchdogActive());
}

TEST_F(ActivityLogTest, LogAndFetchActions) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  ASSERT_TRUE(GetDatabaseEnabled());

  // Write some API calls
  scoped_refptr<Action> action = new Action(kExtensionId,
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "tabs.testMethod");
  activity_log->LogAction(action);
  action = new Action(kExtensionId,
                      base::Time::Now(),
                      Action::ACTION_DOM_ACCESS,
                      "document.write");
  action->set_page_url(GURL("http://www.google.com"));
  activity_log->LogAction(action);

  activity_log->GetFilteredActions(
      kExtensionId, Action::ACTION_ANY, "", "", "", 0,
      base::BindOnce(ActivityLogTest::RetrieveActions_LogAndFetchActions2));
}

TEST_F(ActivityLogTest, LogPrerender) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::DictValue()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2))
          .Build();
  ExtensionRegistrar::Get(profile())->AddExtension(extension);
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  EXPECT_TRUE(activity_log->ShouldLog(extension->id(),
                                      Action::ACTION_CONTENT_SCRIPT, ""));
  ASSERT_TRUE(GetDatabaseEnabled());
  GURL url("http://www.google.com");

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile());

  const gfx::Size kSize(640, 480);
  std::unique_ptr<prerender::NoStatePrefetchHandle> no_state_prefetch_handle =
      no_state_prefetch_manager->StartPrefetchingFromLinkRelPrerender(
          /*process_id=*/-1, /*route_id=*/-1, url,
          blink::mojom::PrerenderTriggerType::kLinkRelPrerender,
          content::Referrer(), url::Origin::Create(url),
          /*initiator_navigation_state=*/nullptr, kSize);

  const std::vector<content::WebContents*> contentses =
      no_state_prefetch_manager->GetAllNoStatePrefetchingContentsForTesting();
  ASSERT_EQ(1U, contentses.size());
  content::WebContents *contents = contentses[0];
  ASSERT_TRUE(no_state_prefetch_manager->IsWebContentsPrefetching(contents));

  activity_log->OnScriptsExecuted(contents, {{extension->id(), {"script"}}},
                                  url);

  activity_log->GetFilteredActions(
      extension->id(), Action::ACTION_ANY, "", "", "", 0,
      base::BindOnce(ActivityLogTest::Arguments_Prerender));

  no_state_prefetch_manager->CancelAllPrerenders();
}

TEST_F(ActivityLogTest, ArgUrlExtraction) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::Time now = base::Time::Now();

  // Submit a DOM API call which should have its URL extracted into the arg_url
  // field.
  EXPECT_TRUE(activity_log->ShouldLog(kExtensionId, Action::ACTION_DOM_ACCESS,
                                      "XMLHttpRequest.open"));
  scoped_refptr<Action> action = new Action(kExtensionId,
                                            now,
                                            Action::ACTION_DOM_ACCESS,
                                            "XMLHttpRequest.open");
  action->set_page_url(GURL("http://www.google.com/"));
  action->mutable_args().Append("POST");
  action->mutable_args().Append("http://api.google.com/");
  action->mutable_other().Set(activity_log_constants::kActionDomVerb,
                              DomActionType::METHOD);
  activity_log->LogAction(action);

  // Submit a DOM API call with a relative URL in the argument, which should be
  // resolved relative to the page URL.
  action = new Action(kExtensionId, now - base::Seconds(1),
                      Action::ACTION_DOM_ACCESS, "XMLHttpRequest.open");
  action->set_page_url(GURL("http://www.google.com/"));
  action->mutable_args().Append("POST");
  action->mutable_args().Append("/api/");
  action->mutable_other().Set(activity_log_constants::kActionDomVerb,
                              DomActionType::METHOD);
  activity_log->LogAction(action);

  // Submit a DOM API call with a relative URL but no base page URL against
  // which to resolve.
  action = new Action(kExtensionId, now - base::Seconds(2),
                      Action::ACTION_DOM_ACCESS, "XMLHttpRequest.open");
  action->mutable_args().Append("POST");
  action->mutable_args().Append("/api/");
  action->mutable_other().Set(activity_log_constants::kActionDomVerb,
                              DomActionType::METHOD);
  activity_log->LogAction(action);

  // Submit an API call with an embedded URL.
  action = new Action(kExtensionId, now - base::Seconds(3),
                      Action::ACTION_API_CALL, "windows.create");
  base::ListValue list;
  auto item = base::DictValue().Set("url", "http://www.google.co.uk");
  list.Append(std::move(item));
  action->set_args(std::move(list));
  activity_log->LogAction(action);

  activity_log->GetFilteredActions(
      kExtensionId, Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(ActivityLogTest::RetrieveActions_ArgUrlExtraction));
}

class MockWindowController : public WindowController {
 public:
  MockWindowController(Profile* profile, content::WebContents* contents)
      : WindowController(nullptr, profile), contents_(contents) {
    WindowControllerList::GetInstance()->AddExtensionWindow(this);
  }
  ~MockWindowController() override {
    WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
  }
  int GetWindowId() const override { return 1; }
  std::string GetWindowTypeText() const override { return "normal"; }
  void SetFullscreenMode(bool is_fullscreen,
                         const GURL& extension_url) const override {}
  content::WebContents* GetActiveTab() const override { return contents_; }
  int GetTabCount() const override { return 1; }
  content::WebContents* GetWebContentsAt(int i) const override {
    return contents_;
  }
  bool IsVisibleToTabsAPIForExtension(
      const Extension* extension,
      bool include_dev_tools_windows) const override {
    return true;
  }
  base::DictValue CreateWindowValueForExtension(
      const Extension* extension,
      PopulateTabBehavior populate_tab_behavior,
      mojom::ContextType context) const override {
    return base::DictValue();
  }
  base::ListValue CreateTabList(const Extension* extension,
                                mojom::ContextType context) const override {
    return base::ListValue();
  }
  bool OpenOptionsPage(const Extension* extension,
                       const GURL& url,
                       bool open_in_tab) override {
    return false;
  }
  BrowserWindowInterface* GetBrowserWindowInterface() override {
    return &browser_window_interface_;
  }

 private:
  raw_ptr<content::WebContents> contents_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
};

TEST_F(ActivityLogTest, ExtractUrls_ScriptingExecuteScript) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::Time now = base::Time::Now();

  // Set up a tab with a URL and a SessionTabHelper.
  GURL url("http://www.google.com");
  NavigateAndCommit(url);
  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), sessions::SessionTabHelper::DelegateLookup());
  int tab_id = ExtensionTabUtil::GetTabId(web_contents());
  ASSERT_NE(-1, tab_id);

  // Set up a WindowController so GetTabById can find the tab.
  MockWindowController window_controller(profile(), web_contents());

  // Submit a scripting.executeScript call with a nested tab ID.
  scoped_refptr<Action> action = new Action(
      kExtensionId, now, Action::ACTION_API_CALL, "scripting.executeScript");
  base::DictValue target;
  target.Set("tabId", tab_id);
  base::DictValue arg;
  arg.Set("target", std::move(target));
  base::ListValue args;
  args.Append(std::move(arg));
  action->set_args(std::move(args));
  activity_log->LogAction(action);

  activity_log->GetFilteredActions(
      kExtensionId, Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce([](std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
        ASSERT_EQ(1U, i->size());
        scoped_refptr<Action> action = i->at(0);
        ASSERT_EQ("scripting.executeScript", action->api_name());
        // Verify tab ID was replaced.
        ASSERT_EQ("[{\"target\":{\"tabId\":\"\\u003Carg_url>\"}}]",
                  ActivityLogPolicy::Util::Serialize(action->args()));
        // Verify URL was extracted.
        ASSERT_EQ("http://www.google.com/", action->arg_url().spec());
      }));
}

TEST_F(ActivityLogTest, UninstalledExtension) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::DictValue()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2))
          .Build();

  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  ASSERT_TRUE(GetDatabaseEnabled());

  // Write some API calls
  scoped_refptr<Action> action =
      new Action(extension->id(), base::Time::Now(), Action::ACTION_API_CALL,
                 "tabs.testMethod");
  activity_log->LogAction(action);
  action = new Action(extension->id(), base::Time::Now(),
                      Action::ACTION_DOM_ACCESS, "document.write");
  action->set_page_url(GURL("http://www.google.com"));

  activity_log->OnExtensionUninstalled(
      nullptr, extension.get(), extensions::UNINSTALL_REASON_FOR_TESTING);
  activity_log->GetFilteredActions(
      extension->id(), Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(ActivityLogTest::RetrieveActions_LogAndFetchActions0));
}

TEST_F(ActivityLogTest, ArgUrlApiCalls) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::Time now = base::Time::Now();
  int api_calls_size = std::size(kUrlApiCalls);
  scoped_refptr<Action> action;

  for (int i = 0; i < api_calls_size; i++) {
    action = new Action(kExtensionId, now - base::Seconds(i),
                        Action::ACTION_DOM_ACCESS, kUrlApiCalls[i]);
    action->mutable_args().Append("http://www.google.co.uk");
    action->mutable_other().Set(activity_log_constants::kActionDomVerb,
                                DomActionType::SETTER);
    activity_log->LogAction(action);
  }

  activity_log->GetFilteredActions(
      kExtensionId, Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(ActivityLogTest::RetrieveActions_ArgUrlApiCalls));
}

TEST_F(ActivityLogTest, DeleteActivitiesByExtension) {
  const std::string kOtherExtensionId = std::string(32, 'c');

  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  ASSERT_TRUE(GetDatabaseEnabled());

  scoped_refptr<Action> action =
      base::MakeRefCounted<Action>(kExtensionId, base::Time::Now(),
                                   Action::ACTION_API_CALL, "tabs.testMethod");
  activity_log->LogAction(action);

  action =
      base::MakeRefCounted<Action>(kOtherExtensionId, base::Time::Now(),
                                   Action::ACTION_DOM_ACCESS, "document.write");
  action->set_page_url(GURL("http://www.google.com"));
  activity_log->LogAction(action);

  activity_log->RemoveExtensionData(kExtensionId);
  activity_log->GetFilteredActions(
      kExtensionId, Action::ACTION_ANY, "", "", "", 0,
      base::BindOnce(ActivityLogTest::RetrieveActions_LogAndFetchActions0));
  activity_log->GetFilteredActions(
      kOtherExtensionId, Action::ACTION_ANY, "", "", "", 0,
      base::BindOnce(ActivityLogTest::RetrieveActions_LogAndFetchActions1));
}

class ActivityLogTestWithoutSwitch : public ActivityLogTest {
 public:
  ActivityLogTestWithoutSwitch() = default;
  ~ActivityLogTestWithoutSwitch() override = default;
  bool enable_activity_logging_switch() const override { return false; }
};

TEST_F(ActivityLogTestWithoutSwitch, TestShouldLog) {
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()))
      ->SetReady();
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();
  ExtensionRegistrar::Get(profile())->AddExtension(empty_extension);
  // Since the command line switch for logging isn't enabled and there's no
  // watchdog app active, the activity log shouldn't log anything.
  EXPECT_FALSE(activity_log->ShouldLog(empty_extension->id(),
                                       Action::ACTION_API_CALL, "tabs.query"));
  const char kAllowlistedExtensionId[] = "eplckmlabaanikjjcgnigddmagoglhmp";
  scoped_refptr<const Extension> activity_log_extension =
      ExtensionBuilder("Test").SetID(kAllowlistedExtensionId).Build();
  ExtensionRegistrar::Get(profile())->AddExtension(activity_log_extension);
  // Loading a watchdog app means the activity log should log other extension
  // activities...
  EXPECT_TRUE(activity_log->ShouldLog(empty_extension->id(),
                                      Action::ACTION_API_CALL, "tabs.query"));
  // ... but not those of the watchdog app...
  EXPECT_FALSE(activity_log->ShouldLog(activity_log_extension->id(),
                                       Action::ACTION_API_CALL, "tabs.query"));
  // ... or activities from the browser/extensions page, represented by an empty
  // extension ID.
  EXPECT_FALSE(activity_log->ShouldLog(std::string(), Action::ACTION_API_CALL,
                                       "tabs.query"));
  ExtensionRegistrar::Get(profile())->DisableExtension(
      activity_log_extension->id(), {disable_reason::DISABLE_USER_ACTION});
  // Disabling the watchdog app means that we're back to never logging anything.
  EXPECT_FALSE(activity_log->ShouldLog(empty_extension->id(),
                                       Action::ACTION_API_CALL, "tabs.query"));
}

TEST_F(ActivityLogTestWithoutSwitch, TelemetryActivation) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::test::ScopedFeatureList feature_list;

  // 1. Initially false.
  EXPECT_FALSE(activity_log->IsTelemetryLoggingActive());

  // 2. Enable flag + add a subscriber -> true.
  feature_list.InitAndEnableFeature(
      extensions_features::kEnterpriseExtensionDOMActivityTelemetry);
  base::CallbackListSubscription subscription =
      activity_log->AddTelemetrySubscriber(profile(), base::DoNothing());
  EXPECT_TRUE(activity_log->IsTelemetryLoggingActive());

  // 3. Remove the subscriber -> false.
  subscription = {};
  EXPECT_FALSE(activity_log->IsTelemetryLoggingActive());

  // 4. Add a subscriber with flag OFF -> false.
  base::test::ScopedFeatureList feature_list2;
  feature_list2.InitAndDisableFeature(
      extensions_features::kEnterpriseExtensionDOMActivityTelemetry);
  subscription =
      activity_log->AddTelemetrySubscriber(profile(), base::DoNothing());
  EXPECT_FALSE(activity_log->IsTelemetryLoggingActive());
}

TEST_F(ActivityLogTestWithoutSwitch, TelemetryNotifiesAllSubscribers) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kEnterpriseExtensionDOMActivityTelemetry);
  int first_count = 0;
  int second_count = 0;
  base::CallbackListSubscription first_subscription =
      activity_log->AddTelemetrySubscriber(
          profile(), base::BindLambdaForTesting(
                         [&](scoped_refptr<Action> action) { ++first_count; }));
  base::CallbackListSubscription second_subscription =
      activity_log->AddTelemetrySubscriber(
          profile(),
          base::BindLambdaForTesting(
              [&](scoped_refptr<Action> action) { ++second_count; }));

  activity_log->LogAction(base::MakeRefCounted<Action>(
      kExtensionId, base::Time::Now(), Action::ACTION_DOM_ACCESS,
      "Document.cookie"));

  EXPECT_EQ(first_count, 1);
  EXPECT_EQ(second_count, 1);
}

TEST_F(ActivityLogTestWithoutSwitch,
       TelemetryStaysActiveUntilLastSubscriberRemoved) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kEnterpriseExtensionDOMActivityTelemetry);
  base::CallbackListSubscription first_subscription =
      activity_log->AddTelemetrySubscriber(profile(), base::DoNothing());
  base::CallbackListSubscription second_subscription =
      activity_log->AddTelemetrySubscriber(profile(), base::DoNothing());

  first_subscription = {};
  EXPECT_TRUE(activity_log->IsTelemetryLoggingActive());

  second_subscription = {};
  EXPECT_FALSE(activity_log->IsTelemetryLoggingActive());
}

TEST_F(ActivityLogTestWithoutSwitch,
       TelemetryNotifiesRenderersOnlyWhenActiveStateChanges) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kEnterpriseExtensionDOMActivityTelemetry);
  auto* renderer_startup_helper =
      static_cast<InterceptingRendererStartupHelper*>(
          RendererStartupHelperFactory::GetForBrowserContext(profile()));

  base::CallbackListSubscription first_subscription =
      activity_log->AddTelemetrySubscriber(profile(), base::DoNothing());
  base::CallbackListSubscription second_subscription =
      activity_log->AddTelemetrySubscriber(profile(), base::DoNothing());
  first_subscription = {};
  second_subscription = {};

  renderer_startup_helper->FlushAllForTesting();
  EXPECT_THAT(renderer_startup_helper->policy_activity_logging_updates(
                  process()->GetID()),
              testing::ElementsAre(true, false));
}

TEST_F(ActivityLogTestWithoutSwitch,
       TelemetrySubscriberCanUnsubscribeOnNotify) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kEnterpriseExtensionDOMActivityTelemetry);
  int count = 0;
  base::CallbackListSubscription subscription;
  subscription = activity_log->AddTelemetrySubscriber(
      profile(), base::BindLambdaForTesting([&](scoped_refptr<Action> action) {
        ++count;
        subscription = {};
      }));

  activity_log->LogAction(base::MakeRefCounted<Action>(
      kExtensionId, base::Time::Now(), Action::ACTION_DOM_ACCESS,
      "Document.cookie"));

  EXPECT_EQ(count, 1);
  EXPECT_FALSE(activity_log->IsTelemetryLoggingActive());
}

TEST_F(ActivityLogTestWithoutSwitch, TelemetryShouldLog) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kEnterpriseExtensionDOMActivityTelemetry);

  const char kExtensionId[] = "ext-1";
  EXPECT_FALSE(activity_log->ShouldLog(kExtensionId, Action::ACTION_API_CALL,
                                       "tabs.query"));

  base::CallbackListSubscription subscription =
      activity_log->AddTelemetrySubscriber(profile(), base::DoNothing());

  // 1. DOM access should always be logged.
  EXPECT_TRUE(activity_log->ShouldLog(kExtensionId, Action::ACTION_DOM_ACCESS,
                                      "Document.cookie"));

  // 2. High-risk APIs should be logged.
  EXPECT_TRUE(activity_log->ShouldLog(kExtensionId, Action::ACTION_API_CALL,
                                      "scripting.executeScript"));

  // 3. Low-risk APIs and Content Scripts should be dropped.
  EXPECT_FALSE(activity_log->ShouldLog(
      kExtensionId, Action::ACTION_CONTENT_SCRIPT, std::string()));
  EXPECT_FALSE(activity_log->ShouldLog(kExtensionId, Action::ACTION_API_CALL,
                                       "storage.get"));
  EXPECT_FALSE(activity_log->ShouldLog(kExtensionId, Action::ACTION_API_EVENT,
                                       "tabs.onUpdated"));

  // Verify database is NOT enabled.
  EXPECT_FALSE(GetDatabaseEnabled());

  subscription = {};
  EXPECT_FALSE(activity_log->ShouldLog(kExtensionId, Action::ACTION_API_CALL,
                                       "scripting.executeScript"));
}

class ActivityLogIsolatedModeTest : public ActivityLogTest,
                                    public ActivityLog::Observer {
 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    ActivityLogTest::SetUp();
    ASSERT_TRUE(isolated_profile()->IsEnterpriseIsolatedModeProfile());

    // Observers are only notified while the ActivityLog has listeners.
    ActivityLog* activity_log = ActivityLog::GetInstance(profile());
    activity_log->SetHasListeners(true);
    activity_log_observation_.Observe(activity_log);
  }

  void TearDown() override {
    activity_log_observation_.Reset();
    // The tabs must be destroyed before their profiles.
    window_controllers_.clear();
    tabs_.clear();
    ActivityLogTest::TearDown();
  }

  std::unique_ptr<TestingProfile> CreateTestingProfile() override {
    profile_ =
        profile_manager_.CreateTestingProfile("profile", GetTestingFactories());
    return nullptr;
  }

  content::BrowserContext* GetBrowserContext() override { return profile_; }

  void DestroyBrowserContext() override {
    // Run the tasks that the RenderProcessHost teardown may have posted while
    // the profile is still alive.
    base::RunLoop().RunUntilIdle();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  void OnExtensionActivity(scoped_refptr<Action> action) override {
    actions_.push_back(std::move(action));
  }

  Profile* isolated_profile() {
    return profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  scoped_refptr<Action> WaitForAction() {
    if (!base::test::RunUntil([&] { return !actions_.empty(); })) {
      return nullptr;
    }
    return actions_.front();
  }

  content::WebContents* CreateTab(Profile* profile, const GURL& url = GURL()) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);
    sessions::SessionTabHelper::CreateForWebContents(
        contents.get(), sessions::SessionTabHelper::DelegateLookup());
    if (url.is_valid()) {
      content::WebContentsTester::For(contents.get())->NavigateAndCommit(url);
    }
    window_controllers_.push_back(
        std::make_unique<MockWindowController>(profile, contents.get()));
    tabs_.push_back(std::move(contents));
    return tabs_.back().get();
  }

  static void ExecuteScript(Profile* source, content::WebContents* tab) {
    activity_monitor::OnApiFunctionCalled(
        source, kExtensionId, "scripting.executeScript",
        base::ListValue().Append(base::DictValue().Set(
            "target",
            base::DictValue().Set("tabId", ExtensionTabUtil::GetTabId(tab)))));
  }

  void InjectContentScript(content::WebContents* tab) {
    scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
    ExtensionRegistry::Get(profile())->AddEnabled(extension);
    ActivityLog::GetInstance(profile())->OnScriptsExecuted(
        tab, {{extension->id(), {"script"}}}, GURL("https://example.com"));
  }

  static void LogDomAction(Profile* context, const GURL& url) {
    ExtensionsBrowserClient::Get()->AddDOMActionToActivityLog(
        context, kExtensionId, "Document.cookie", base::ListValue(), url,
        std::u16string(), DomActionType::GETTER);
  }

 private:
  enterprise_isolated_mode::ScopedIsolatedModeOverrideForTesting
      scoped_isolated_mode_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::vector<std::unique_ptr<content::WebContents>> tabs_;
  std::vector<std::unique_ptr<MockWindowController>> window_controllers_;
  std::vector<scoped_refptr<Action>> actions_;
  base::ScopedObservation<ActivityLog, ActivityLog::Observer>
      activity_log_observation_{this};
};

TEST_F(ActivityLogIsolatedModeTest, CloneCopiesIsolatedMode) {
  auto action = base::MakeRefCounted<Action>(kExtensionId, base::Time::Now(),
                                             Action::ACTION_DOM_ACCESS,
                                             "Document.cookie");
  action->set_isolated_mode(true);

  EXPECT_TRUE(action->Clone()->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagSetForIsolatedModeTargetTabFromRegularProfile) {
  content::WebContents* tab =
      CreateTab(isolated_profile(), GURL("https://example.com"));

  ExecuteScript(profile(), tab);

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  ASSERT_TRUE(action->arg_url().is_valid());
  EXPECT_TRUE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagSetForIsolatedModeTargetTabWithoutUrl) {
  content::WebContents* tab = CreateTab(isolated_profile());
  ASSERT_FALSE(tab->GetURL().is_valid());

  ExecuteScript(profile(), tab);

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest, IsolatedModeFlagNotSetForRegularTargetTab) {
  content::WebContents* tab = CreateTab(profile(), GURL("https://example.com"));

  ExecuteScript(profile(), tab);

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  ASSERT_TRUE(action->arg_url().is_valid());
  EXPECT_FALSE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagSetForTabListWithIsolatedModeTab) {
  content::WebContents* isolated_tab =
      CreateTab(isolated_profile(), GURL("https://example.com"));
  content::WebContents* regular_tab =
      CreateTab(profile(), GURL("https://example.org"));

  activity_monitor::OnApiFunctionCalled(
      profile(), kExtensionId, "tabs.remove",
      base::ListValue().Append(
          base::ListValue()
              .Append(ExtensionTabUtil::GetTabId(regular_tab))
              .Append(ExtensionTabUtil::GetTabId(isolated_tab))
              .Append(ExtensionTabUtil::GetTabId(regular_tab))));

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagSetForEventOnIsolatedModeTab) {
  content::WebContents* tab =
      CreateTab(isolated_profile(), GURL("https://example.com"));

  activity_monitor::OnApiEventDispatched(
      profile(), kExtensionId, "tabs.onUpdated",
      base::ListValue().Append(ExtensionTabUtil::GetTabId(tab)));

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagSetForApiCallFromIsolatedModeProfile) {
  activity_monitor::OnApiFunctionCalled(isolated_profile(), kExtensionId,
                                        "tabs.query", base::ListValue());

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagNotSetForApiCallFromOtherOffTheRecordProfile) {
  Profile* other_otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  ASSERT_TRUE(other_otr_profile->IsOffTheRecord());
  ASSERT_FALSE(other_otr_profile->IsEnterpriseIsolatedModeProfile());

  activity_monitor::OnApiFunctionCalled(other_otr_profile, kExtensionId,
                                        "tabs.query", base::ListValue());

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  EXPECT_FALSE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagSetForWebRequestFromIsolatedModeProfile) {
  activity_monitor::OnWebRequestApiUsed(
      isolated_profile(), kExtensionId, GURL("https://example.com"),
      /*is_incognito=*/true, "onBeforeRequest", base::DictValue());

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagSetForIsolatedModeContentScript) {
  content::WebContents* tab = CreateTab(isolated_profile());

  InjectContentScript(tab);

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagNotSetForOtherOffTheRecordContentScript) {
  Profile* other_otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  content::WebContents* tab = CreateTab(other_otr_profile);

  InjectContentScript(tab);

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  ASSERT_TRUE(action->page_incognito());
  EXPECT_FALSE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagSetForIsolatedModeDomActionWithoutUrl) {
  LogDomAction(isolated_profile(), GURL());

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->isolated_mode());
}

TEST_F(ActivityLogIsolatedModeTest,
       IsolatedModeFlagNotSetForOtherOffTheRecordDomAction) {
  Profile* other_otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  LogDomAction(other_otr_profile, GURL("https://example.com"));

  scoped_refptr<Action> action = WaitForAction();
  ASSERT_TRUE(action);
  ASSERT_TRUE(action->page_incognito());
  EXPECT_FALSE(action->isolated_mode());
}

// Sets the features of a test fixture. It must be the first base class of the
// fixture, so that the features are set before the other base classes create
// the task environment and the profiles.
class ScopedFeaturesMixin {
 protected:
  explicit ScopedFeaturesMixin(
      const base::flat_map<base::test::FeatureRef, bool>& feature_states) {
    feature_list_.InitWithFeatureStates(feature_states);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Enables DOM activity telemetry. The primary Off-The-Record profile is a
// regular Incognito profile.
class ActivityLogTelemetryContextTest : private ScopedFeaturesMixin,
                                        public ActivityLogTestWithoutSwitch {
 protected:
  explicit ActivityLogTelemetryContextTest(bool milestone2_enabled = true)
      : ScopedFeaturesMixin(
            {{extensions_features::kEnterpriseExtensionDOMActivityTelemetry,
              true},
             {enterprise_isolated_mode::kEnterpriseIsolatedModeMilestone2,
              milestone2_enabled}}) {}

  ActivityLog* activity_log() { return ActivityLog::GetInstance(profile()); }

  InterceptingRendererStartupHelper* renderer_startup_helper() {
    return static_cast<InterceptingRendererStartupHelper*>(
        RendererStartupHelperFactory::GetForBrowserContext(profile()));
  }

  // Adds a telemetry subscriber for `context` that appends to `actions`.
  [[nodiscard]] base::CallbackListSubscription AddTelemetrySubscriber(
      content::BrowserContext* context,
      std::vector<scoped_refptr<Action>>& actions) {
    return activity_log()->AddTelemetrySubscriber(
        context,
        base::BindLambdaForTesting([&actions](scoped_refptr<Action> action) {
          actions.push_back(std::move(action));
        }));
  }

  static scoped_refptr<Action> CreateDomAction() {
    return base::MakeRefCounted<Action>(kExtensionId, base::Time::Now(),
                                        Action::ACTION_DOM_ACCESS,
                                        "Document.cookie");
  }
};

TEST_F(ActivityLogTelemetryContextTest,
       TelemetryIsActiveForIncognitoContextWithRegularSubscriber) {
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito_profile->IsIncognitoProfile());
  base::CallbackListSubscription subscription =
      activity_log()->AddTelemetrySubscriber(profile(), base::DoNothing());

  EXPECT_TRUE(
      activity_log()->IsTelemetryLoggingActiveForContext(incognito_profile));
}

TEST_F(ActivityLogTelemetryContextTest,
       TelemetryKeepsIncognitoActivityRegular) {
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito_profile->IsIncognitoProfile());
  std::vector<scoped_refptr<Action>> regular_actions;
  base::CallbackListSubscription regular_subscription =
      AddTelemetrySubscriber(profile(), regular_actions);

  scoped_refptr<Action> action = CreateDomAction();
  action->set_page_url(GURL("https://example.com"));
  action->set_page_incognito(true);
  activity_log()->LogAction(action);

  EXPECT_THAT(regular_actions, testing::ElementsAre(action));
}

// The primary Off-The-Record profile is an Enterprise Isolated Mode profile.
class ActivityLogIsolatedModeTelemetryTest
    : public ActivityLogTelemetryContextTest {
 protected:
  explicit ActivityLogIsolatedModeTelemetryTest(bool milestone2_enabled = true)
      : ActivityLogTelemetryContextTest(milestone2_enabled) {}

  void SetUp() override {
    ActivityLogTelemetryContextTest::SetUp();
    isolated_profile_ =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    ASSERT_TRUE(isolated_profile_->IsEnterpriseIsolatedModeProfile());
    ASSERT_EQ(ActivityLog::GetInstance(isolated_profile_), activity_log());
  }

  void TearDown() override {
    isolated_profile_ = nullptr;
    ActivityLogTelemetryContextTest::TearDown();
  }

  Profile* isolated_profile() { return isolated_profile_; }

 private:
  enterprise_isolated_mode::ScopedIsolatedModeOverrideForTesting
      scoped_isolated_mode_;
  raw_ptr<Profile> isolated_profile_ = nullptr;
};

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryIsActiveOnlyForSubscribedContext) {
  std::vector<scoped_refptr<Action>> isolated_actions;
  base::CallbackListSubscription isolated_subscription =
      AddTelemetrySubscriber(isolated_profile(), isolated_actions);

  EXPECT_TRUE(
      activity_log()->IsTelemetryLoggingActiveForContext(isolated_profile()));
  EXPECT_FALSE(activity_log()->IsTelemetryLoggingActiveForContext(profile()));
}

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryLoggingIsActiveWithOnlyIsolatedModeSubscriber) {
  ASSERT_FALSE(activity_log()->IsTelemetryLoggingActive());
  ASSERT_FALSE(activity_log()->ShouldLog(
      kExtensionId, Action::ACTION_DOM_ACCESS, "Document.cookie"));

  base::CallbackListSubscription isolated_subscription =
      activity_log()->AddTelemetrySubscriber(isolated_profile(),
                                             base::DoNothing());

  EXPECT_TRUE(activity_log()->IsTelemetryLoggingActive());
  EXPECT_TRUE(activity_log()->ShouldLog(kExtensionId, Action::ACTION_DOM_ACCESS,
                                        "Document.cookie"));
}

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryRoutesIsolatedModeActionToIsolatedSubscribers) {
  std::vector<scoped_refptr<Action>> regular_actions;
  std::vector<scoped_refptr<Action>> isolated_actions;
  base::CallbackListSubscription regular_subscription =
      AddTelemetrySubscriber(profile(), regular_actions);
  base::CallbackListSubscription isolated_subscription =
      AddTelemetrySubscriber(isolated_profile(), isolated_actions);

  scoped_refptr<Action> action = CreateDomAction();
  action->set_isolated_mode(true);
  activity_log()->LogAction(action);

  EXPECT_THAT(regular_actions, testing::IsEmpty());
  EXPECT_THAT(isolated_actions, testing::ElementsAre(action));
}

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryDropsIsolatedModeActionWithoutIsolatedSubscribers) {
  std::vector<scoped_refptr<Action>> regular_actions;
  base::CallbackListSubscription regular_subscription =
      AddTelemetrySubscriber(profile(), regular_actions);

  scoped_refptr<Action> isolated_action = CreateDomAction();
  isolated_action->set_isolated_mode(true);
  activity_log()->LogAction(isolated_action);
  scoped_refptr<Action> regular_action = CreateDomAction();
  activity_log()->LogAction(regular_action);

  // The regular subscriber only gets the regular action.
  EXPECT_THAT(regular_actions, testing::ElementsAre(regular_action));
}

TEST_F(
    ActivityLogIsolatedModeTelemetryTest,
    TelemetryRoutesExecuteScriptOnUrlLessIsolatedModeTabToIsolatedSubscribers) {
  std::vector<scoped_refptr<Action>> regular_actions;
  std::vector<scoped_refptr<Action>> isolated_actions;
  base::CallbackListSubscription regular_subscription =
      AddTelemetrySubscriber(profile(), regular_actions);
  base::CallbackListSubscription isolated_subscription =
      AddTelemetrySubscriber(isolated_profile(), isolated_actions);
  std::unique_ptr<content::WebContents> tab =
      content::WebContentsTester::CreateTestWebContents(isolated_profile(),
                                                        nullptr);
  sessions::SessionTabHelper::CreateForWebContents(
      tab.get(), sessions::SessionTabHelper::DelegateLookup());
  ASSERT_FALSE(tab->GetURL().is_valid());
  MockWindowController window_controller(isolated_profile(), tab.get());

  auto action = base::MakeRefCounted<Action>(kExtensionId, base::Time::Now(),
                                             Action::ACTION_API_CALL,
                                             "scripting.executeScript");
  action->mutable_args().Append(base::DictValue().Set(
      "target",
      base::DictValue().Set("tabId", ExtensionTabUtil::GetTabId(tab.get()))));
  activity_log()->LogAction(action);

  EXPECT_THAT(regular_actions, testing::IsEmpty());
  EXPECT_THAT(isolated_actions, testing::ElementsAre(action));
}

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryIgnoresPageIncognitoForRouting) {
  std::vector<scoped_refptr<Action>> regular_actions;
  std::vector<scoped_refptr<Action>> isolated_actions;
  base::CallbackListSubscription regular_subscription =
      AddTelemetrySubscriber(profile(), regular_actions);
  base::CallbackListSubscription isolated_subscription =
      AddTelemetrySubscriber(isolated_profile(), isolated_actions);

  scoped_refptr<Action> action = CreateDomAction();
  action->set_page_url(GURL("https://example.com"));
  action->set_page_incognito(true);
  activity_log()->LogAction(action);

  EXPECT_THAT(regular_actions, testing::ElementsAre(action));
  EXPECT_THAT(isolated_actions, testing::IsEmpty());
}

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryKeepsRegularActionsInRegularSubscribers) {
  std::vector<scoped_refptr<Action>> regular_actions;
  std::vector<scoped_refptr<Action>> isolated_actions;
  base::CallbackListSubscription regular_subscription =
      AddTelemetrySubscriber(profile(), regular_actions);
  base::CallbackListSubscription isolated_subscription =
      AddTelemetrySubscriber(isolated_profile(), isolated_actions);

  scoped_refptr<Action> action = CreateDomAction();
  activity_log()->LogAction(action);

  EXPECT_THAT(regular_actions, testing::ElementsAre(action));
  EXPECT_THAT(isolated_actions, testing::IsEmpty());
}

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryRemovingIsolatedSubscriberKeepsRegularActive) {
  std::vector<scoped_refptr<Action>> regular_actions;
  std::vector<scoped_refptr<Action>> isolated_actions;
  base::CallbackListSubscription regular_subscription =
      AddTelemetrySubscriber(profile(), regular_actions);
  base::CallbackListSubscription isolated_subscription =
      AddTelemetrySubscriber(isolated_profile(), isolated_actions);

  isolated_subscription = {};

  EXPECT_TRUE(activity_log()->IsTelemetryLoggingActiveForContext(profile()));
  EXPECT_FALSE(
      activity_log()->IsTelemetryLoggingActiveForContext(isolated_profile()));
}

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryNotifiesIsolatedModeRenderersOfIsolatedSubscribers) {
  content::MockRenderProcessHost isolated_process(isolated_profile());
  isolated_process.Init();
  renderer_startup_helper()->OnRenderProcessHostCreated(&isolated_process);

  base::CallbackListSubscription isolated_subscription =
      activity_log()->AddTelemetrySubscriber(isolated_profile(),
                                             base::DoNothing());
  isolated_subscription = {};

  renderer_startup_helper()->FlushAllForTesting();
  EXPECT_THAT(renderer_startup_helper()->policy_activity_logging_updates(
                  isolated_process.GetID()),
              testing::ElementsAre(true, false));
}

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryDoesNotNotifyRegularRenderersOfIsolatedSubscribers) {
  content::MockRenderProcessHost isolated_process(isolated_profile());
  isolated_process.Init();
  renderer_startup_helper()->OnRenderProcessHostCreated(&isolated_process);

  base::CallbackListSubscription isolated_subscription =
      activity_log()->AddTelemetrySubscriber(isolated_profile(),
                                             base::DoNothing());
  isolated_subscription = {};

  renderer_startup_helper()->FlushAllForTesting();
  ASSERT_THAT(renderer_startup_helper()->policy_activity_logging_updates(
                  isolated_process.GetID()),
              testing::ElementsAre(true, false));
  EXPECT_THAT(renderer_startup_helper()->policy_activity_logging_updates(
                  process()->GetID()),
              testing::IsEmpty());
}

TEST_F(ActivityLogIsolatedModeTelemetryTest,
       TelemetryDoesNotNotifyIsolatedModeRenderersOfRegularSubscribers) {
  content::MockRenderProcessHost isolated_process(isolated_profile());
  isolated_process.Init();
  renderer_startup_helper()->OnRenderProcessHostCreated(&isolated_process);

  base::CallbackListSubscription regular_subscription =
      activity_log()->AddTelemetrySubscriber(profile(), base::DoNothing());
  regular_subscription = {};

  renderer_startup_helper()->FlushAllForTesting();
  ASSERT_THAT(renderer_startup_helper()->policy_activity_logging_updates(
                  process()->GetID()),
              testing::ElementsAre(true, false));
  EXPECT_THAT(renderer_startup_helper()->policy_activity_logging_updates(
                  isolated_process.GetID()),
              testing::IsEmpty());
}

class ActivityLogIsolatedModeWithoutMilestone2Test
    : public ActivityLogIsolatedModeTelemetryTest {
 protected:
  ActivityLogIsolatedModeWithoutMilestone2Test()
      : ActivityLogIsolatedModeTelemetryTest(/*milestone2_enabled=*/false) {}
};

TEST_F(ActivityLogIsolatedModeWithoutMilestone2Test,
       TelemetryRoutesIsolatedModeActionToRegularSubscribers) {
  std::vector<scoped_refptr<Action>> regular_actions;
  base::CallbackListSubscription regular_subscription =
      AddTelemetrySubscriber(profile(), regular_actions);

  scoped_refptr<Action> action = CreateDomAction();
  action->set_isolated_mode(true);
  activity_log()->LogAction(action);

  EXPECT_THAT(regular_actions, testing::ElementsAre(action));
}

TEST_F(ActivityLogIsolatedModeWithoutMilestone2Test,
       TelemetryIsActiveForIsolatedModeContextWithRegularSubscriber) {
  std::vector<scoped_refptr<Action>> regular_actions;
  base::CallbackListSubscription regular_subscription =
      AddTelemetrySubscriber(profile(), regular_actions);

  EXPECT_TRUE(
      activity_log()->IsTelemetryLoggingActiveForContext(isolated_profile()));
}

TEST_F(ActivityLogIsolatedModeWithoutMilestone2Test,
       TelemetryDoesNotNotifyIsolatedModeRenderersWithoutMilestone2) {
  content::MockRenderProcessHost isolated_process(isolated_profile());
  isolated_process.Init();
  renderer_startup_helper()->OnRenderProcessHostCreated(&isolated_process);

  base::CallbackListSubscription subscription =
      activity_log()->AddTelemetrySubscriber(profile(), base::DoNothing());
  subscription = {};

  renderer_startup_helper()->FlushAllForTesting();
  ASSERT_THAT(renderer_startup_helper()->policy_activity_logging_updates(
                  process()->GetID()),
              testing::ElementsAre(true, false));
  EXPECT_THAT(renderer_startup_helper()->policy_activity_logging_updates(
                  isolated_process.GetID()),
              testing::IsEmpty());
}

}  // namespace extensions
