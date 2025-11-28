// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tools_test_util.h"

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/rect_f.h"

namespace actor {

actor_login::Credential MakeTestCredential(
    const std::u16string& username,
    const GURL& url,
    bool immediately_available_to_login) {
  actor_login::Credential credential;
  credential.username = username;
  // TODO(crbug.com/441231531): Clarify the format.
  credential.source_site_or_app =
      base::UTF8ToUTF16(url.GetWithEmptyPath().spec());
  credential.request_origin = url::Origin::Create(url);
  credential.type = actor_login::CredentialType::kPassword;
  credential.immediatelyAvailableToLogin = immediately_available_to_login;
  return credential;
}

MockActorLoginService::MockActorLoginService() = default;

MockActorLoginService::~MockActorLoginService() = default;

void MockActorLoginService::GetCredentials(
    tabs::TabInterface* tab,
    base::WeakPtr<actor_login::ActorLoginQualityLoggerInterface> mqls_logger,
    actor_login::CredentialsOrErrorReply callback) {
  std::move(callback).Run(credentials_);
}

void MockActorLoginService::AttemptLogin(
    tabs::TabInterface* tab,
    const actor_login::Credential& credential,
    bool should_store_permission,
    base::WeakPtr<actor_login::ActorLoginQualityLoggerInterface> mqls_logger,
    actor_login::LoginStatusResultOrErrorReply callback) {
  last_credential_used_ = credential;
  last_permission_was_permanent_ = should_store_permission;
  std::move(callback).Run(login_status_);
}

void MockActorLoginService::SetCredentials(
    const actor_login::CredentialsOrError& credentials) {
  credentials_ = credentials;
}

void MockActorLoginService::SetCredential(
    const actor_login::Credential& credential) {
  SetCredentials(std::vector{credential});
}

void MockActorLoginService::SetLoginStatus(
    actor_login::LoginStatusResultOrError login_status) {
  login_status_ = login_status;
}

const std::optional<actor_login::Credential>&
MockActorLoginService::last_credential_used() const {
  return last_credential_used_;
}
bool MockActorLoginService::last_permission_was_permanent() const {
  return last_permission_was_permanent_;
}

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

  auto* actor_service = ActorKeyedService::Get(browser()->profile());
  actor_service->GetPolicyChecker().SetActOnWebForTesting(
      ShouldForceActOnWeb());
  task_id_ = CreateNewTask();

  // Optimization guide uses this histogram to signal initialization in tests.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester_for_init_,
      "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

  InitActionBlocklist(browser()->profile());

  // Simulate the component loading, as the implementation checks it, but the
  // actual list is set via the command line.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
      ->MaybeUpdateHintsComponent(
          {base::Version("123"),
           temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dont_care"))});
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

std::unique_ptr<ExecutionEngine> ActorToolsTest::CreateExecutionEngine(
    Profile* profile) {
  return std::make_unique<ExecutionEngine>(profile);
}

bool ActorToolsTest::ShouldForceActOnWeb() {
  return true;
}

TaskId ActorToolsTest::CreateNewTask() {
  auto execution_engine = CreateExecutionEngine(browser()->profile());
  auto event_dispatcher = ui::NewUiEventDispatcher(
      ActorKeyedService::Get(browser()->profile())->GetActorUiStateManager());
  auto actor_task = std::make_unique<ActorTask>(browser()->profile(),
                                                std::move(execution_engine),
                                                std::move(event_dispatcher));
  return ActorKeyedService::Get(browser()->profile())
      ->AddActiveTask(std::move(actor_task));
}

void ActorToolsTest::SetPageContent(
    base::OnceClosure quit_closure,
    optimization_guide::AIPageContentResultOrError page_content) {
  auto apc = std::move(page_content->proto);
  auto* tab_data = ActorTabData::From(active_tab());
  tab_data->DidObserveContent(apc);
  std::move(quit_closure).Run();
}

void ActorToolsTest::GetPageApc() {
  base::RunLoop run_loop;
  auto options = optimization_guide::ActionableAIPageContentOptions(
      /*on_critical_path =*/true);
  options->max_meta_elements = 32;
  GetAIPageContent(
      web_contents(), std::move(options),
      base::BindOnce(&ActorToolsTest::SetPageContent, base::Unretained(this),
                     run_loop.QuitClosure()));

  run_loop.Run();
}

gfx::RectF GetBoundingClientRect(content::RenderFrameHost& rfh,
                                 std::string_view query) {
  double width =
      content::EvalJs(
          &rfh, content::JsReplace(
                    "document.querySelector($1).getBoundingClientRect().width",
                    query))
          .ExtractDouble();
  double height =
      content::EvalJs(
          &rfh, content::JsReplace(
                    "document.querySelector($1).getBoundingClientRect().height",
                    query))
          .ExtractDouble();
  double x =
      content::EvalJs(
          &rfh,
          content::JsReplace(
              "document.querySelector($1).getBoundingClientRect().x", query))
          .ExtractDouble();
  double y =
      content::EvalJs(
          &rfh,
          content::JsReplace(
              "document.querySelector($1).getBoundingClientRect().y", query))
          .ExtractDouble();

  return gfx::RectF(x, y, width, height);
}

std::string DescribePaintStabilityMode(features::ActorPaintStabilityMode mode) {
  switch (mode) {
    case features::ActorPaintStabilityMode::kDisabled:
      return "Disabled";
    case features::ActorPaintStabilityMode::kLogOnly:
      return "LogOnly";
    case features::ActorPaintStabilityMode::kEnabled:
      return "Enabled";
  }
}

}  // namespace actor
