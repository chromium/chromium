// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_commit_deferring_condition.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {
namespace {

using ::testing::_;
using ::testing::Return;

webui::mojom::NavigationConfirmationResponsePtr
MakeNavigationConfirmationResponse(bool allowed) {
  auto response = webui::mojom::NavigationConfirmationResponse::New();
  response->result =
      webui::mojom::ConfirmationRequestResult::NewPermissionGranted(allowed);
  return response;
}

class ActorCommitDeferringConditionTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicActor, {}},
         {kGlicCrossOriginNavigationGating,
          {{"confirm_navigation_to_new_origins", "true"}}}},
        /*disabled_features=*/{});
    ChromeRenderViewHostTestHarness::SetUp();
  }

  ActorTask* CreateTask() {
    ActorKeyedService* service = ActorKeyedService::Get(profile());
    auto mock_ui_event_dispatcher =
        std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
    ON_CALL(*mock_ui_event_dispatcher, OnActorTaskAsyncChange)
        .WillByDefault([](const ui::UiEventDispatcher::ActorTaskAsyncChange&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });
    TaskId task_id = service->CreateTaskForTesting(
        std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
        NoEnterprisePolicyChecker(), /*options=*/nullptr,
        mock_delegate_.GetWeakPtr());
    return service->GetTask(task_id);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockActorTaskDelegate> mock_delegate_;
};

TEST_F(ActorCommitDeferringConditionTest,
       MaybeCreate_NormalNavigation_ReturnsNull) {
  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  AddTabToTask(tab_state.tab, *task);

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com"), main_rfh());
  handle.set_is_served_from_bfcache(false);

  EXPECT_EQ(
      ActorCommitDeferringCondition::MaybeCreate(
          handle, content::CommitDeferringCondition::NavigationType::kOther),
      nullptr);
}

TEST_F(ActorCommitDeferringConditionTest,
       MaybeCreate_SubframeNavigation_ReturnsNull) {
  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  AddTabToTask(tab_state.tab, *task);

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com"), main_rfh());
  handle.set_is_served_from_bfcache(true);
  handle.set_is_in_primary_main_frame(false);

  EXPECT_EQ(
      ActorCommitDeferringCondition::MaybeCreate(
          handle, content::CommitDeferringCondition::NavigationType::kOther),
      nullptr);
}

TEST_F(ActorCommitDeferringConditionTest,
       MaybeCreate_NoWebContents_ReturnsNull) {
  testing::NiceMock<content::MockNavigationHandle> handle;
  handle.set_is_served_from_bfcache(true);

  EXPECT_EQ(
      ActorCommitDeferringCondition::MaybeCreate(
          handle, content::CommitDeferringCondition::NavigationType::kOther),
      nullptr);
}

TEST_F(ActorCommitDeferringConditionTest,
       MaybeCreate_NoTabInterface_ReturnsNull) {
  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com"), main_rfh());
  handle.set_is_served_from_bfcache(true);

  EXPECT_EQ(
      ActorCommitDeferringCondition::MaybeCreate(
          handle, content::CommitDeferringCondition::NavigationType::kOther),
      nullptr);
}

TEST_F(ActorCommitDeferringConditionTest,
       MaybeCreate_TabNotInActorTask_ReturnsNull) {
  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  // Do NOT add tab to task.

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com"), main_rfh());
  handle.set_is_served_from_bfcache(true);

  EXPECT_EQ(
      ActorCommitDeferringCondition::MaybeCreate(
          handle, content::CommitDeferringCondition::NavigationType::kOther),
      nullptr);
}

TEST_F(ActorCommitDeferringConditionTest,
       MaybeCreate_PrerenderedPageActivation_ReturnsCondition) {
  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  AddTabToTask(tab_state.tab, *task);

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com"), main_rfh());
  handle.set_is_in_primary_main_frame(true);

  auto condition = ActorCommitDeferringCondition::MaybeCreate(
      handle, content::CommitDeferringCondition::NavigationType::
                  kPrerenderedPageActivation);
  ASSERT_NE(condition, nullptr);
  EXPECT_STREQ(condition->TraceEventName(), "ActorCommitDeferringCondition");
}

TEST_F(ActorCommitDeferringConditionTest,
       MaybeCreate_BFCacheRestore_ReturnsCondition) {
  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  AddTabToTask(tab_state.tab, *task);

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com"), main_rfh());
  handle.set_is_served_from_bfcache(true);
  handle.set_is_in_primary_main_frame(true);

  auto condition = ActorCommitDeferringCondition::MaybeCreate(
      handle, content::CommitDeferringCondition::NavigationType::kOther);
  ASSERT_NE(condition, nullptr);
  EXPECT_STREQ(condition->TraceEventName(), "ActorCommitDeferringCondition");
}

TEST_F(ActorCommitDeferringConditionTest,
       WillCommitNavigation_TaskDestroyed_Proceeds) {
  ActorKeyedService* service = ActorKeyedService::Get(profile());
  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);
  TaskId task_id = task->id();

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  AddTabToTask(tab_state.tab, *task);

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com"), main_rfh());
  handle.set_is_served_from_bfcache(true);
  handle.set_is_in_primary_main_frame(true);

  auto condition = ActorCommitDeferringCondition::MaybeCreate(
      handle, content::CommitDeferringCondition::NavigationType::kOther);
  ASSERT_NE(condition, nullptr);

  // Stop and destroy the task, waiting for pending deletes to complete.
  service->StopTask(task_id, ActorTask::StoppedReason::kTaskComplete);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return service->GetTask(task_id) == nullptr; }));

  EXPECT_EQ(condition->WillCommitNavigation(
                base::MakeExpectedNotRunClosure(FROM_HERE)),
            content::CommitDeferringCondition::Result::kProceed);
}

TEST_F(ActorCommitDeferringConditionTest,
       WillCommitNavigation_BFCacheRestore_SameOrigin_Proceeds) {
  NavigateAndCommit(GURL("https://example.com/page1"));

  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  AddTabToTask(tab_state.tab, *task);

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com/page2"), main_rfh());
  handle.set_is_served_from_bfcache(true);
  handle.set_is_in_primary_main_frame(true);

  auto condition = ActorCommitDeferringCondition::MaybeCreate(
      handle, content::CommitDeferringCondition::NavigationType::kOther);
  ASSERT_NE(condition, nullptr);

  base::test::TestFuture<void> resume_future;
  EXPECT_EQ(condition->WillCommitNavigation(resume_future.GetCallback()),
            content::CommitDeferringCondition::Result::kDefer);
  EXPECT_TRUE(resume_future.Wait());
}

TEST_F(ActorCommitDeferringConditionTest,
       WillCommitNavigation_BFCacheRestore_CrossOrigin_Granted) {
  NavigateAndCommit(GURL("https://example.com/page1"));

  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  AddTabToTask(tab_state.tab, *task);

  const GURL destination_url("https://other-domain.com/page2");
  testing::NiceMock<content::MockNavigationHandle> handle(destination_url,
                                                          main_rfh());
  handle.set_is_served_from_bfcache(true);
  handle.set_is_in_primary_main_frame(true);

  EXPECT_CALL(mock_delegate_,
              RequestToConfirmNavigation(
                  task->id(), url::Origin::Create(destination_url), _))
      .WillOnce(base::test::RunOnceCallback<2>(
          MakeNavigationConfirmationResponse(/*allowed=*/true)));

  auto condition = ActorCommitDeferringCondition::MaybeCreate(
      handle, content::CommitDeferringCondition::NavigationType::kOther);
  ASSERT_NE(condition, nullptr);

  base::test::TestFuture<void> resume_future;
  EXPECT_EQ(condition->WillCommitNavigation(resume_future.GetCallback()),
            content::CommitDeferringCondition::Result::kDefer);
  EXPECT_TRUE(resume_future.Wait());
}

TEST_F(ActorCommitDeferringConditionTest,
       WillCommitNavigation_BFCacheRestore_CrossOrigin_Denied) {
  NavigateAndCommit(GURL("https://example.com/page1"));

  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  AddTabToTask(tab_state.tab, *task);

  const GURL destination_url("https://other-domain.com/page2");
  testing::NiceMock<content::MockNavigationHandle> handle(destination_url,
                                                          main_rfh());
  handle.set_is_served_from_bfcache(true);
  handle.set_is_in_primary_main_frame(true);

  EXPECT_CALL(mock_delegate_,
              RequestToConfirmNavigation(
                  task->id(), url::Origin::Create(destination_url), _))
      .WillOnce(base::test::RunOnceCallback<2>(
          MakeNavigationConfirmationResponse(/*allowed=*/false)));

  auto condition = ActorCommitDeferringCondition::MaybeCreate(
      handle, content::CommitDeferringCondition::NavigationType::kOther);
  ASSERT_NE(condition, nullptr);

  EXPECT_EQ(condition->WillCommitNavigation(
                base::MakeExpectedNotRunClosure(FROM_HERE)),
            content::CommitDeferringCondition::Result::kDefer);
}

class ActorCommitDeferringConditionGatingDisabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActor},
        /*disabled_features=*/{kGlicPageActivationGating});
    ChromeRenderViewHostTestHarness::SetUp();
  }

  ActorTask* CreateTask() {
    ActorKeyedService* service = ActorKeyedService::Get(profile());
    auto mock_ui_event_dispatcher =
        std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
    ON_CALL(*mock_ui_event_dispatcher, OnActorTaskAsyncChange)
        .WillByDefault([](const ui::UiEventDispatcher::ActorTaskAsyncChange&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });
    TaskId task_id = service->CreateTaskForTesting(
        std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
        NoEnterprisePolicyChecker(), /*options=*/nullptr,
        mock_delegate_.GetWeakPtr());
    return service->GetTask(task_id);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockActorTaskDelegate> mock_delegate_;
};

TEST_F(ActorCommitDeferringConditionGatingDisabledTest,
       WillCommitNavigation_DisabledGating_ProceedsImmediately) {
  ActorTask* task = CreateTask();
  ASSERT_TRUE(task);

  TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);
  AddTabToTask(tab_state.tab, *task);

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com"), main_rfh());
  handle.set_is_served_from_bfcache(true);
  handle.set_is_in_primary_main_frame(true);

  EXPECT_EQ(
      ActorCommitDeferringCondition::MaybeCreate(
          handle, content::CommitDeferringCondition::NavigationType::kOther),
      nullptr);
}

}  // namespace
}  // namespace actor
