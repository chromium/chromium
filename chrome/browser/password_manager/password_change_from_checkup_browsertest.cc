// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/run_until.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
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
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
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
    : public InProcessBrowserTest {
 public:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateMockOptimizationGuideService));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("example.com", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  autofill::test::AutofillBrowserTestEnvironment autofill_environment_;
  glic::GlicTestEnvironment glic_test_env_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeFromCheckupDelegateBrowserTest,
                       StartsFlowAndVerifiesActorTab) {
  Profile* profile = browser()->profile();
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  auto delegate = std::make_unique<PasswordChangeFromCheckupDelegate>();
  EXPECT_FALSE(delegate->HasActorTaskSubscriptionForTesting());
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  delegate->StartPasswordChangeFlow(CreateCredentialUIEntry(url),
                                    web_contents->GetWeakPtr());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return delegate->HasActorTaskSubscriptionForTesting(); }));

  EXPECT_TRUE(delegate->HasActorTaskSubscriptionForTesting());
  // Simulate the actor starting and then finishing a task.
  actor::TaskId task_id =
      actor_service->CreateTask(actor::NoEnterprisePolicyChecker());

  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);

  // Waiting for the actor task to reach `kFinished` state and unsubscribe.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !delegate->HasActorTaskSubscriptionForTesting(); }));
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

  auto delegate = std::make_unique<PasswordChangeFromCheckupDelegate>();
  GURL url = embedded_test_server()->GetURL(
      "example.com", "/password/update_form_empty_fields.html");

  delegate->StartPasswordChangeFlow(CreateCredentialUIEntry(url),
                                    original_web_contents->GetWeakPtr());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return delegate->HasActorTaskSubscriptionForTesting(); }));

  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PasswordsNavigationObserver observer(new_web_contents);
  EXPECT_TRUE(observer.Wait());
  ASSERT_TRUE(content::NavigateToURL(new_web_contents, url));

  base::RunLoop run_loop;
  // Simulate the filler submitting the form with Model Execution Service.
  EXPECT_CALL(*mock_opt_guide,
              ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                               kPasswordChangeSubmission,
                           testing::_, testing::_, testing::_))
      .WillOnce(testing::DoAll(
          testing::Invoke(&run_loop, &base::RunLoop::Quit),
          testing::WithArg<3>([](auto callback) {
            optimization_guide::proto::PasswordChangeResponse response;
            response.mutable_submit_form_data()->set_dom_node_id_to_click(1);
            auto result =
                optimization_guide::OptimizationGuideModelExecutionResult(
                    optimization_guide::AnyWrapProto(response), nullptr);
            std::move(callback).Run(std::move(result), nullptr);
          })));
  actor::TaskId task_id =
      actor_service->CreateTask(actor::NoEnterprisePolicyChecker());
  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
  run_loop.Run();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !delegate->HasActorTaskSubscriptionForTesting(); }));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return delegate->IsCleanedUpAfterTaskFinishedForTesting(); }));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeFromCheckupDelegateBrowserTest,
                       FlowRequiresUserIntervention) {
  Profile* profile = browser()->profile();
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);

  content::WebContents* originator_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(originator_contents);

  auto delegate = std::make_unique<PasswordChangeFromCheckupDelegate>();
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  delegate->StartPasswordChangeFlow(CreateCredentialUIEntry(url),
                                    originator_contents->GetWeakPtr());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return delegate->HasActorTaskSubscriptionForTesting(); }));
  // A new tab for the actutation is opened.
  content::WebContents* actuation_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(originator_contents, actuation_contents);

  actor::TaskId task_id =
      actor_service->CreateTask(actor::NoEnterprisePolicyChecker());
  int originator_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(originator_contents);
  browser()->tab_strip_model()->ActivateTabAt(originator_index);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            originator_contents);

  // Simulate an interruption state.
  actor_service->NotifyTaskStateChanged(
      task_id, actor::ActorTask::State::kPausedByActor);

  // The delegate should have caught the interruption and force the actuation
  // tab into focus.
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            actuation_contents);
  EXPECT_EQ(delegate->GetActorTaskState(),
            actor::ActorTask::State::kPausedByActor);

  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeFromCheckupDelegateBrowserTest,
                       AfterUserInterventionOriginatorFocused) {
  Profile* profile = browser()->profile();
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);

  content::WebContents* originator_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto delegate = std::make_unique<PasswordChangeFromCheckupDelegate>();
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  delegate->StartPasswordChangeFlow(CreateCredentialUIEntry(url),
                                    originator_contents->GetWeakPtr());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return delegate->HasActorTaskSubscriptionForTesting(); }));
  actor::TaskId task_id =
      actor_service->CreateTask(actor::NoEnterprisePolicyChecker());
  content::WebContents* actuation_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int originator_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(originator_contents);
  browser()->tab_strip_model()->ActivateTabAt(originator_index);
  // Simulate an interruption state.
  actor_service->NotifyTaskStateChanged(
      task_id, actor::ActorTask::State::kPausedByActor);
  // Verify actuation tab is focused due to the interruption.
  ASSERT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            actuation_contents);

  // Simulate the user resuming the task.
  actor_service->NotifyTaskStateChanged(task_id,
                                        actor::ActorTask::State::kActing);

  // The delegate should have caught that the task was resumed and force the
  // originator tab into focus.
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            originator_contents);
  EXPECT_EQ(delegate->GetActorTaskState(), actor::ActorTask::State::kActing);

  // Clean up.
  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
}
