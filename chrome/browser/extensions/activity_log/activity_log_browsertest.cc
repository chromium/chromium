// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

// Only the prerender tests are in this file. To add tests for activity
// logging please see:
//    chrome/test/data/extensions/api_test/activity_log_private/README

class ActivityLogPrerenderTest : public ExtensionApiTest {
 protected:
  // Make sure the activity log is turned on.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    prerender::PrerenderManager::SetMode(
        prerender::PrerenderManager::DEPRECATED_PRERENDER_MODE_ENABLED);
  }

  static void Prerender_Arguments(
      const std::string& extension_id,
      uint16_t port,
      const base::Closure& quit_when_idle_closure,
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    quit_when_idle_closure.Run();

    ASSERT_TRUE(i->size());
    scoped_refptr<Action> last = i->front();

    ASSERT_EQ(extension_id, last->extension_id());
    ASSERT_EQ(Action::ACTION_CONTENT_SCRIPT, last->action_type());
    ASSERT_EQ("[\"/google_cs.js\"]",
              ActivityLogPolicy::Util::Serialize(last->args()));
    ASSERT_EQ(
        base::StringPrintf("http://www.google.com.bo:%u/title1.html", port),
        last->SerializePageUrl());
    ASSERT_EQ(base::StringPrintf("www.google.com.bo:%u/title1.html", port),
              last->page_title());
    ASSERT_EQ("{\"prerender\":true}",
              ActivityLogPolicy::Util::Serialize(last->other()));
    ASSERT_EQ("", last->api_name());
    ASSERT_EQ("", last->SerializeArgUrl());
  }
};

// https://crbug.com/724553
#if defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestScriptInjected DISABLED_TestScriptInjected
#else
#define MAYBE_TestScriptInjected TestScriptInjected
#endif  // defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)

IN_PROC_BROWSER_TEST_F(ActivityLogPrerenderTest, MAYBE_TestScriptInjected) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  uint16_t port = embedded_test_server()->port();

  // Get the extension (chrome/test/data/extensions/activity_log)
  const Extension* ext =
      LoadExtension(test_data_dir_.AppendASCII("activity_log"));
  ASSERT_TRUE(ext);

  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  activity_log->SetWatchdogAppActiveForTesting(true);
  ASSERT_TRUE(activity_log);

  // Disable rate limiting in PrerenderManager
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile());
  ASSERT_TRUE(prerender_manager);
  prerender_manager->mutable_config().rate_limit_enabled = false;
  // Increase prerenderer limits, otherwise this test fails
  // on Windows XP.
  prerender_manager->mutable_config().max_bytes = 1000 * 1024 * 1024;
  prerender_manager->mutable_config().time_to_live =
      base::TimeDelta::FromMinutes(10);
  prerender_manager->mutable_config().abandon_time_to_live =
      base::TimeDelta::FromMinutes(10);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::WindowedNotificationObserver page_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  GURL url(base::StringPrintf("http://www.google.com.bo:%u/title1.html", port));

  const gfx::Size kSize(640, 480);
  std::unique_ptr<prerender::PrerenderHandle> prerender_handle(
      prerender_manager->AddPrerenderFromOmnibox(
          url,
          web_contents->GetController().GetDefaultSessionStorageNamespace(),
          kSize));

  page_observer.Wait();

  base::RunLoop run_loop;
  activity_log->GetFilteredActions(
      ext->id(), Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(ActivityLogPrerenderTest::Prerender_Arguments, ext->id(),
                     port, run_loop.QuitWhenIdleClosure()));

  // Allow invocation of Prerender_Arguments
  run_loop.Run();
}

}  // namespace extensions
