// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/actor_features.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

password_manager::CredentialUIEntry CreateCredentialUIEntry(const GURL& url) {
  password_manager::PasswordForm form;
  form.url = url;
  form.signon_realm = url::Origin::Create(url).GetURL().spec();
  form.username_value = u"testuser";
  form.password_value = u"testpass";
  return password_manager::CredentialUIEntry(form);
}

std::unique_ptr<KeyedService> CreateMockOptimizationGuideService(
    content::BrowserContext* context) {
  return std::make_unique<
      testing::NiceMock<MockOptimizationGuideKeyedService>>();
}

}  // namespace

class PasswordChangeFromCheckupDelegateBrowserTest
    : public PasswordManagerBrowserTestBase {
 public:
  PasswordChangeFromCheckupDelegateBrowserTest() {
    feature_list_.InitWithFeatures(
        {password_manager::features::kPasswordCheckupPrototype,
         autofill::features::debug::kShowDomNodeIDs},
        {});
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateMockOptimizationGuideService));
  }

  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("example.com", "127.0.0.1");
  }

  int GetDomNodeId(content::WebContents* web_contents,
                   const std::string& element_id) {
    const std::string value_get_script = base::StringPrintf(
        "var element = document.getElementById('%s');"
        "var value = element ? Number(element.getAttribute(\"dom-node-id\")) : "
        "-1;"
        "value;",
        element_id.c_str());
    return content::EvalJs(web_contents->GetPrimaryMainFrame(),
                           value_get_script,
                           content::EXECUTE_SCRIPT_NO_USER_GESTURE)
        .ExtractInt();
  }

 private:
  autofill::test::AutofillBrowserTestEnvironment autofill_environment_;
  glic::GlicTestEnvironment glic_test_env_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeFromCheckupDelegateBrowserTest,
                       StartsFlowAndVerifiesActorTab) {
  Profile* profile = browser()->profile();
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  auto delegate = std::make_unique<PasswordChangeFromCheckupDelegate>(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  delegate->StartPasswordChangeFlow(CreateCredentialUIEntry(url),
                                    web_contents->GetWeakPtr());
  auto* actuation_tab = browser()->tab_strip_model()->GetActiveTab();

  // Create task and add the tab to the task.
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor::ActorTask* task = actor_service->GetTask(task_id);

  base::test::TestFuture<actor::mojom::ActionResultPtr> add_tab_future;
  task->AddTab(actuation_tab->GetHandle(), /*stop_task_on_detach=*/true,
               add_tab_future.GetCallback());
  EXPECT_TRUE(add_tab_future.Wait());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return browser()->tab_strip_model()->GetActiveTab() == actuation_tab;
  }));
  actor_service->NotifyTaskStateChanged(*task);

  // Finish the task
  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);

  // Wait for the actor task to finish.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return delegate->GetFindFormTaskState() ==
           actor::ActorTask::State::kFinished;
  }));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeFromCheckupDelegateBrowserTest,
                       FormWaiterFindsFormAndSubmits) {
  Profile* profile = browser()->profile();
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);

  // Grab the Optimization Guide Mock
  auto* mock_opt_guide = static_cast<MockOptimizationGuideKeyedService*>(
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile));

  content::WebContents* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto delegate = std::make_unique<PasswordChangeFromCheckupDelegate>(
      ChromePasswordManagerClient::FromWebContents(original_web_contents));
  GURL url = embedded_test_server()->GetURL(
      "example.com", "/password/update_form_empty_fields.html");

  content::TestNavigationObserver observer(url.GetWithEmptyPath());
  observer.StartWatchingNewWebContents();

  delegate->StartPasswordChangeFlow(CreateCredentialUIEntry(url),
                                    original_web_contents->GetWeakPtr());
  auto* actuation_tab = browser()->tab_strip_model()->GetActiveTab();

  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor::ActorTask* task = actor_service->GetTask(task_id);
  base::test::TestFuture<actor::mojom::ActionResultPtr> add_tab_future;
  task->AddTab(actuation_tab->GetHandle(), /*stop_task_on_detach=*/true,
               add_tab_future.GetCallback());
  EXPECT_TRUE(add_tab_future.Wait());
  actor_service->NotifyTaskStateChanged(*task);

  observer.Wait();

  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(new_web_contents, url));

  base::RunLoop run_loop;
  // Simulate the filler submitting the form with Model Execution Service.
  EXPECT_CALL(*mock_opt_guide,
              ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                               kPasswordChangeSubmission,
                           testing::_, testing::_, testing::_))
      .WillOnce(testing::DoAll(
          testing::Invoke(&run_loop, &base::RunLoop::Quit),
          testing::WithArg<3>([&](auto callback) {
            optimization_guide::proto::PasswordChangeResponse response;
            response.mutable_submit_form_data()->set_dom_node_id_to_click(
                GetDomNodeId(new_web_contents,
                             "chg_submit_wo_username_button"));
            auto result =
                optimization_guide::OptimizationGuideModelExecutionResult(
                    optimization_guide::AnyWrapProto(response), nullptr);
            std::move(callback).Run(std::move(result), nullptr);
          })));
  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
  run_loop.Run();

  // After the form is submitted a verification task is created
  // and finished.
  actor::TaskId verification_task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->StopTask(verification_task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);

  // Wait for the new password to be saved.
  WaitForPasswordStore();
  CheckThatCredentialsStored(/*username=*/"testuser", /*password=*/"testpass");
}

IN_PROC_BROWSER_TEST_F(PasswordChangeFromCheckupDelegateBrowserTest,
                       FlowStopsOnUserIntervention) {
  Profile* profile = browser()->profile();
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);

  content::WebContents* originator_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(originator_contents);

  auto delegate = std::make_unique<PasswordChangeFromCheckupDelegate>(
      ChromePasswordManagerClient::FromWebContents(originator_contents));
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  delegate->StartPasswordChangeFlow(CreateCredentialUIEntry(url),
                                    originator_contents->GetWeakPtr());
  // A new tab for the actuation is opened.
  auto* actuation_tab = browser()->tab_strip_model()->GetActiveTab();
  // Create task and add the tab to it.
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());

  actor::ActorTask* task = actor_service->GetTask(task_id);
  base::test::TestFuture<actor::mojom::ActionResultPtr> add_tab_future;
  task->AddTab(actuation_tab->GetHandle(), /*stop_task_on_detach=*/true,
               add_tab_future.GetCallback());
  EXPECT_TRUE(add_tab_future.Wait());
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return browser()->tab_strip_model()->GetActiveTab() == actuation_tab;
  }));

  // Simulate an interruption state.
  task->SetState(actor::ActorTask::State::kReflecting);
  task->Interrupt();

  // The delegate should have caught the interruption and stopped the task.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return delegate->GetFindFormTaskState() ==
           actor::ActorTask::State::kCancelled;
  }));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeFromCheckupDelegateBrowserTest,
                       AutoSelectsCorrectCredential) {
  Profile* profile = browser()->profile();
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);

  content::WebContents* originator_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start the flow with a specific credential.
  auto delegate = std::make_unique<PasswordChangeFromCheckupDelegate>(
      ChromePasswordManagerClient::FromWebContents(originator_contents));
  const std::string username = "testuser";
  const GURL origin_url = embedded_test_server()->GetURL("example.com", "/");

  delegate->StartPasswordChangeFlow(CreateCredentialUIEntry(origin_url),
                                    originator_contents->GetWeakPtr());
  auto* actuation_tab = browser()->tab_strip_model()->GetActiveTab();

  // Setup the `ActorTask` and associate the tab.
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor::ActorTask* task = actor_service->GetTask(task_id);
  base::test::TestFuture<actor::mojom::ActionResultPtr> add_tab_future;
  task->AddTab(actuation_tab->GetHandle(), /*stop_task_on_detach=*/true,
               add_tab_future.GetCallback());
  ASSERT_TRUE(add_tab_future.Wait());

  actor_service->NotifyTaskStateChanged(*task);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return browser()->tab_strip_model()->GetActiveTab() == actuation_tab;
  }));
  // Simulate the `ExecutionEngine` needing a credential.
  std::vector<actor_login::Credential> credentials;

  // Wrong credential
  actor_login::Credential wrong_user;
  wrong_user.username = u"wrong_user";
  wrong_user.source_site_or_app = base::UTF8ToUTF16(origin_url.spec());
  credentials.push_back(wrong_user);

  // Correct credential
  actor_login::Credential correct_user;
  correct_user.username = base::UTF8ToUTF16(username);
  correct_user.source_site_or_app = base::UTF8ToUTF16(origin_url.spec());
  credentials.push_back(correct_user);

  base::test::TestFuture<actor::webui::mojom::SelectCredentialDialogResponsePtr>
      selection_future;

  task->GetExecutionEngine().PromptToSelectCredential(
      credentials, /*icons=*/{}, selection_future.GetCallback());

  auto response = selection_future.Take();
  ASSERT_TRUE(response->selected_credential_id.has_value());
  EXPECT_EQ(response->selected_credential_id.value(), correct_user.id.value());
  EXPECT_EQ(response->permission_duration,
            actor::webui::mojom::UserGrantedPermissionDuration::kOneTime);

  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeFromCheckupDelegateBrowserTest,
                       OnFindFormTaskStateChangedTracksTaskCorrectly) {
  Profile* profile = browser()->profile();
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);

  content::WebContents* originator_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto delegate = std::make_unique<PasswordChangeFromCheckupDelegate>(
      ChromePasswordManagerClient::FromWebContents(originator_contents));
  const GURL origin_url = embedded_test_server()->GetURL("example.com", "/");

  delegate->StartPasswordChangeFlow(CreateCredentialUIEntry(origin_url),
                                    originator_contents->GetWeakPtr());
  auto* actuation_tab = browser()->tab_strip_model()->GetActiveTab();

  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor::ActorTask* task = actor_service->GetTask(task_id);

  // Fire a state change before the tab is attached to verify that the delegate
  // is not tracking the task yet. This simulates the kCreated notification
  // where HasTab() is false.
  actor_service->NotifyTaskStateChanged(*task);
  EXPECT_FALSE(delegate->GetFindFormTaskState().has_value());

  // Attach the tab to the task to verify that the delegate is tracking the
  // task now.
  base::test::TestFuture<actor::mojom::ActionResultPtr> add_tab_future;
  task->AddTab(actuation_tab->GetHandle(), /*stop_task_on_detach=*/true,
               add_tab_future.GetCallback());
  ASSERT_TRUE(add_tab_future.Wait());
  // Fire a state change after the tab is attached to verify that the delegate
  // is tracking the task now. This simulates the kActing notification where
  // HasTab() is true.
  actor_service->NotifyTaskStateChanged(*task);

  EXPECT_TRUE(delegate->GetFindFormTaskState().has_value());
  EXPECT_EQ(delegate->GetFindFormTaskState().value(), task->GetState());

  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
}
