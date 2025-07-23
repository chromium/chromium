// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tools_test_util.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/display/display_switches.h"

namespace actor {

ActorToolsTest::ActorToolsTest() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                            features::kGlicActor},
      /*disabled_features=*/{features::kGlicWarming, kGlicActionAllowlist});
}

ActorToolsTest::~ActorToolsTest() = default;

void ActorToolsTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(embedded_https_test_server().Start());
  auto execution_engine =
      std::make_unique<ExecutionEngine>(browser()->profile());
  auto event_dispatcher = ui::NewUiEventDispatcher(
      ActorKeyedService::Get(browser()->profile())->GetActorUiStateManager());
  auto actor_task = std::make_unique<ActorTask>(browser()->profile(),
                                                std::move(execution_engine),
                                                std::move(event_dispatcher));
  task_id_ = ActorKeyedService::Get(browser()->profile())
                 ->AddActiveTask(std::move(actor_task));

  // Optimization guide uses this histogram to signal initialization in tests.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester_for_init_,
      "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

  InitActionBlocklist(browser()->profile());
}

void ActorToolsTest::SetUpCommandLine(base::CommandLine* command_line) {
  InProcessBrowserTest::SetUpCommandLine(command_line);
  SetUpBlocklist(command_line, "blocked.example.com");
  command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
}

void ActorToolsTest::TearDownOnMainThread() {
  // The ActorTask owned ExecutionEngine has a pointer to the profile, which
  // must be released before the browser is torn down to avoid a dangling
  // pointer.
  ActorKeyedService::Get(browser()->profile())->ResetForTesting();
}

void ActorToolsTest::GoBack() {
  content::TestNavigationObserver observer(web_contents());
  web_contents()->GetController().GoBack();
  observer.Wait();
}

void ActorToolsTest::TinyWait() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

content::WebContents* ActorToolsTest::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

tabs::TabInterface* ActorToolsTest::active_tab() {
  return tabs::TabInterface::GetFromContents(web_contents());
}

content::RenderFrameHost* ActorToolsTest::main_frame() {
  return web_contents()->GetPrimaryMainFrame();
}

ExecutionEngine& ActorToolsTest::execution_engine() {
  return *actor_task().GetExecutionEngine();
}

ActorTask& ActorToolsTest::actor_task() const {
  CHECK(task_id_);
  return *ActorKeyedService::Get(browser()->profile())->GetTask(task_id_);
}

}  // namespace actor
