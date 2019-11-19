// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/activity_log.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension_builder.h"
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

class ActivityLogTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual bool enable_activity_logging_switch() const { return true; }
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SetActivityLogTaskRunnerForTesting(
        base::ThreadTaskRunnerHandle::Get().get());

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
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    SetActivityLogTaskRunnerForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
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
    const base::DictionaryValue* other = NULL;
    int dom_verb = -1;

    ASSERT_EQ(4U, i->size());
    scoped_refptr<Action> action = i->at(0);
    ASSERT_EQ("XMLHttpRequest.open", action->api_name());
    ASSERT_EQ("[\"POST\",\"\\u003Carg_url>\"]",
              ActivityLogPolicy::Util::Serialize(action->args()));
    ASSERT_EQ("http://api.google.com/", action->arg_url().spec());
    // Test that the dom_verb field was changed to XHR (from METHOD).  This
    // could be tested on all retrieved XHR actions but it would be redundant,
    // so just test once.
    other = action->other();
    ASSERT_TRUE(other);
    ASSERT_TRUE(other->GetInteger(activity_log_constants::kActionDomVerb,
                                  &dom_verb));
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
    size_t api_calls_size = base::size(kUrlApiCalls);
    const base::DictionaryValue* other = NULL;
    int dom_verb = -1;

    ASSERT_EQ(api_calls_size, actions->size());

    for (size_t i = 0; i < actions->size(); i++) {
      scoped_refptr<Action> action = actions->at(i);
      ASSERT_EQ(kExtensionId, action->extension_id());
      ASSERT_EQ(Action::ACTION_DOM_ACCESS, action->action_type());
      ASSERT_EQ(kUrlApiCalls[i], action->api_name());
      ASSERT_EQ("[\"\\u003Carg_url>\"]",
                ActivityLogPolicy::Util::Serialize(action->args()));
      ASSERT_EQ("http://www.google.co.uk/", action->arg_url().spec());
      other = action->other();
      ASSERT_TRUE(other);
      ASSERT_TRUE(
          other->GetInteger(activity_log_constants::kActionDomVerb, &dom_verb));
      ASSERT_EQ(DomActionType::SETTER, dom_verb);
    }
  }

  ExtensionService* extension_service_;
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
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  extension_service_->AddExtension(extension.get());
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  EXPECT_TRUE(activity_log->ShouldLog(extension->id()));
  ASSERT_TRUE(GetDatabaseEnabled());
  GURL url("http://www.google.com");

  prerender::test_utils::RestorePrerenderMode restore_prerender_mode;
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::DEPRECATED_PRERENDER_MODE_ENABLED);
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile());

  const gfx::Size kSize(640, 480);
  std::unique_ptr<prerender::PrerenderHandle> prerender_handle(
      prerender_manager->AddPrerenderFromOmnibox(
          url,
          web_contents()->GetController().GetDefaultSessionStorageNamespace(),
          kSize));

  const std::vector<content::WebContents*> contentses =
      prerender_manager->GetAllPrerenderingContents();
  ASSERT_EQ(1U, contentses.size());
  content::WebContents *contents = contentses[0];
  ASSERT_TRUE(prerender_manager->IsWebContentsPrerendering(contents, NULL));

  activity_log->OnScriptsExecuted(contents, {{extension->id(), {"script"}}},
                                  url);

  activity_log->GetFilteredActions(
      extension->id(), Action::ACTION_ANY, "", "", "", 0,
      base::BindOnce(ActivityLogTest::Arguments_Prerender));

  prerender_manager->CancelAllPrerenders();
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
  action->mutable_args()->AppendString("POST");
  action->mutable_args()->AppendString("http://api.google.com/");
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::METHOD);
  activity_log->LogAction(action);

  // Submit a DOM API call with a relative URL in the argument, which should be
  // resolved relative to the page URL.
  action = new Action(kExtensionId,
                      now - base::TimeDelta::FromSeconds(1),
                      Action::ACTION_DOM_ACCESS,
                      "XMLHttpRequest.open");
  action->set_page_url(GURL("http://www.google.com/"));
  action->mutable_args()->AppendString("POST");
  action->mutable_args()->AppendString("/api/");
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::METHOD);
  activity_log->LogAction(action);

  // Submit a DOM API call with a relative URL but no base page URL against
  // which to resolve.
  action = new Action(kExtensionId,
                      now - base::TimeDelta::FromSeconds(2),
                      Action::ACTION_DOM_ACCESS,
                      "XMLHttpRequest.open");
  action->mutable_args()->AppendString("POST");
  action->mutable_args()->AppendString("/api/");
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::METHOD);
  activity_log->LogAction(action);

  // Submit an API call with an embedded URL.
  action = new Action(kExtensionId,
                      now - base::TimeDelta::FromSeconds(3),
                      Action::ACTION_API_CALL,
                      "windows.create");
  action->set_args(
      ListBuilder()
          .Append(
              DictionaryBuilder().Set("url", "http://www.google.co.uk").Build())
          .Build());
  activity_log->LogAction(action);

  activity_log->GetFilteredActions(
      kExtensionId, Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(ActivityLogTest::RetrieveActions_ArgUrlExtraction));
}

TEST_F(ActivityLogTest, UninstalledExtension) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
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
      NULL, extension.get(), extensions::UNINSTALL_REASON_FOR_TESTING);
  activity_log->GetFilteredActions(
      extension->id(), Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(ActivityLogTest::RetrieveActions_LogAndFetchActions0));
}

TEST_F(ActivityLogTest, ArgUrlApiCalls) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  base::Time now = base::Time::Now();
  int api_calls_size = base::size(kUrlApiCalls);
  scoped_refptr<Action> action;

  for (int i = 0; i < api_calls_size; i++) {
    action = new Action(kExtensionId,
                        now - base::TimeDelta::FromSeconds(i),
                        Action::ACTION_DOM_ACCESS,
                        kUrlApiCalls[i]);
    action->mutable_args()->AppendString("http://www.google.co.uk");
    action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
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
  const char kWhitelistedExtensionId[] = "eplckmlabaanikjjcgnigddmagoglhmp";
  scoped_refptr<const Extension> activity_log_extension =
      ExtensionBuilder("Test").SetID(kWhitelistedExtensionId).Build();
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
