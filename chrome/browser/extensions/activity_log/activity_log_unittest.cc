// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/activity_log/activity_log.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

const char* const kUrlApiCalls[] = {
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
    "HTMLVideoElement.poster"};

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

 protected:
  mojo::PendingAssociatedRemote<mojom::Renderer> BindNewRendererRemote(
      content::RenderProcessHost* process) override {
    mojo::AssociatedRemote<mojom::Renderer> remote;
    receivers_.Add(this, remote.BindNewEndpointAndPassDedicatedReceiver());
    return remote.Unbind();
  }

 private:
  // mojom::Renderer implementation:
  void ActivateExtension(const std::string& extension_id) override {}
  void SetActivityLoggingEnabled(bool enabled) override {}
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
  void SetSessionInfo(version_info::Channel channel,
                      mojom::FeatureSessionType session,
                      bool is_lock_screen_context) override {}
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

  mojo::AssociatedReceiverSet<mojom::Renderer> receivers_;
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
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile()))->CreateExtensionService
            (&command_line, base::FilePath(), false);

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
    ASSERT_EQ(0, static_cast<int>(i->size()));
  }

  static void RetrieveActions_LogAndFetchActions1(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(1, static_cast<int>(i->size()));
  }

  static void RetrieveActions_LogAndFetchActions2(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
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
    const base::Value::Dict& other = *action->other();
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
      const base::Value::Dict& other = *action->other();
      std::optional<int> dom_verb =
          other.FindInt(activity_log_constants::kActionDomVerb);
      ASSERT_EQ(DomActionType::SETTER, dom_verb);
    }
  }

  raw_ptr<ExtensionService, DanglingUntriaged> extension_service_;
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
          .SetManifest(base::Value::Dict()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  EXPECT_TRUE(activity_log->ShouldLog(extension->id()));
  ASSERT_TRUE(GetDatabaseEnabled());
  GURL url("http://www.google.com");

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile());

  const gfx::Size kSize(640, 480);
  std::unique_ptr<prerender::NoStatePrefetchHandle> no_state_prefetch_handle(
      no_state_prefetch_manager->AddSameOriginSpeculation(
          url,
          web_contents()->GetController().GetDefaultSessionStorageNamespace(),
          kSize, url::Origin::Create(url)));

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
  EXPECT_TRUE(activity_log->ShouldLog(kExtensionId));
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
  base::Value::List list;
  auto item = base::Value::Dict().Set("url", "http://www.google.co.uk");
  list.Append(std::move(item));
  action->set_args(std::move(list));
  activity_log->LogAction(action);

  activity_log->GetFilteredActions(
      kExtensionId, Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(ActivityLogTest::RetrieveActions_ArgUrlExtraction));
}

TEST_F(ActivityLogTest, UninstalledExtension) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2))
          .Build();

  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  ASSERT_TRUE(GetDatabaseEnabled());

  // Write some API calls
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "tabs.testMethod");
  activity_log->LogAction(action);
  action = new Action(extension->id(),
                      base::Time::Now(),
                      Action::ACTION_DOM_ACCESS,
                      "document.write");
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
  ActivityLogTestWithoutSwitch() {}
  ~ActivityLogTestWithoutSwitch() override {}
  bool enable_activity_logging_switch() const override { return false; }
};

TEST_F(ActivityLogTestWithoutSwitch, TestShouldLog) {
  static_cast<TestExtensionSystem*>(
      ExtensionSystem::Get(profile()))->SetReady();
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();
  extension_service_->AddExtension(empty_extension.get());
  // Since the command line switch for logging isn't enabled and there's no
  // watchdog app active, the activity log shouldn't log anything.
  EXPECT_FALSE(activity_log->ShouldLog(empty_extension->id()));
  const char kAllowlistedExtensionId[] = "eplckmlabaanikjjcgnigddmagoglhmp";
  scoped_refptr<const Extension> activity_log_extension =
      ExtensionBuilder("Test").SetID(kAllowlistedExtensionId).Build();
  extension_service_->AddExtension(activity_log_extension.get());
  // Loading a watchdog app means the activity log should log other extension
  // activities...
  EXPECT_TRUE(activity_log->ShouldLog(empty_extension->id()));
  // ... but not those of the watchdog app...
  EXPECT_FALSE(activity_log->ShouldLog(activity_log_extension->id()));
  // ... or activities from the browser/extensions page, represented by an empty
  // extension ID.
  EXPECT_FALSE(activity_log->ShouldLog(std::string()));
  extension_service_->DisableExtension(activity_log_extension->id(),
                                       disable_reason::DISABLE_USER_ACTION);
  // Disabling the watchdog app means that we're back to never logging anything.
  EXPECT_FALSE(activity_log->ShouldLog(empty_extension->id()));
}

}  // namespace extensions
