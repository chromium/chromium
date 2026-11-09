// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_commit_deferring_condition.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_navigation_throttle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/history_tool.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/task_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

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
        /*disabled_features=*/GetDisabledFeatures());
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
  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() const {
    return {};
  }

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

using NavigationConfirmedCallback =
    ActorNavigationThrottle::Delegate::NavigationConfirmedCallback;

class MockNavigationDelegate : public ActorNavigationThrottle::Delegate {
 public:
  MOCK_METHOD(bool,
              MaybeDeferNavigation,
              (tabs::TabInterface*, const GURL&, NavigationConfirmedCallback),
              (override));

  base::WeakPtr<ActorNavigationThrottle::Delegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockNavigationDelegate> weak_factory_{this};
};

// A user Back/Forward restored from the back/forward cache in a tab of an
// acting task.
class ActorCommitDeferringConditionUserBackForwardTest
    : public ActorCommitDeferringConditionTest {
 public:
  void SetUp() override {
    ActorCommitDeferringConditionTest::SetUp();
#if BUILDFLAG(IS_ANDROID)
    // Only desktop form factors confirm user UI navigations on Android.
    base::android::device_info::set_is_desktop_for_testing(true);
#endif
    NavigateAndCommit(GURL("https://example.com/page1"));

    ActorTask* task = CreateTask();
    ASSERT_TRUE(task);
    task_id_ = task->id();
    task->SetNavigationDelegate(delegate_.GetWeakPtr());

    tab_state_.emplace(web_contents());
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &tab_state_->tab);
    AddTabToTask(tab_state_->tab, *task);

    handle_.emplace(destination_url_, main_rfh());
    handle_->set_is_served_from_bfcache(true);
    handle_->set_is_in_primary_main_frame(true);
    handle_->set_is_renderer_initiated(false);
    handle_->set_page_transition(::ui::PageTransitionFromInt(
        ::ui::PAGE_TRANSITION_LINK | ::ui::PAGE_TRANSITION_FORWARD_BACK));
  }

  void TearDown() override {
#if BUILDFLAG(IS_ANDROID)
    base::android::device_info::reset_is_desktop_for_testing();
#endif
    handle_.reset();
    tab_state_.reset();
    ActorCommitDeferringConditionTest::TearDown();
  }

 protected:
  std::unique_ptr<content::CommitDeferringCondition> CreateCondition() {
    return ActorCommitDeferringCondition::MaybeCreate(
        *handle_, content::CommitDeferringCondition::NavigationType::kOther);
  }

  // Shows the confirmation and keeps its callback in `confirm_`.
  void ExpectConfirmation() {
    EXPECT_CALL(delegate_,
                MaybeDeferNavigation(&tab_state_->tab, destination_url_, _))
        .WillOnce([this](tabs::TabInterface*, const GURL&,
                         NavigationConfirmedCallback callback) {
          confirm_ = std::move(callback);
          return true;
        });
  }

  ActorTask* GetTask() {
    return ActorKeyedService::Get(profile())->GetTask(task_id_);
  }

  // Origin gating asks the task delegate to confirm the destination origin.
  void ExpectOriginGating(bool allowed) {
    EXPECT_CALL(mock_delegate_,
                RequestToConfirmNavigation(
                    task_id_, url::Origin::Create(destination_url_), _))
        .WillOnce(base::test::RunOnceCallback<2>(
            MakeNavigationConfirmationResponse(allowed)));
  }

  // Cross-origin to the committed page, so origin gating does not allow the
  // restore without asking.
  const GURL destination_url_{"https://other-domain.com/page2"};
  TaskId task_id_;
  testing::StrictMock<MockNavigationDelegate> delegate_;
  std::optional<TestTabState> tab_state_;
  std::optional<testing::NiceMock<content::MockNavigationHandle>> handle_;
  NavigationConfirmedCallback confirm_;
};

// Leave stops the task and resumes the commit without origin gating.
TEST_F(ActorCommitDeferringConditionUserBackForwardTest,
       LeaveStopsTaskAndResumes) {
  ExpectConfirmation();
  EXPECT_CALL(mock_delegate_, RequestToConfirmNavigation).Times(0);
  auto condition = CreateCondition();
  ASSERT_NE(condition, nullptr);

  base::test::TestFuture<void> resume_future;
  EXPECT_EQ(condition->WillCommitNavigation(resume_future.GetCallback()),
            content::CommitDeferringCondition::Result::kDefer);
  ASSERT_TRUE(confirm_);
  EXPECT_FALSE(resume_future.IsReady());

  std::move(confirm_).Run(true);
  EXPECT_TRUE(resume_future.Wait());
  EXPECT_EQ(GetTask(), nullptr);
}

// Stay keeps the task, and the commit neither resumes nor runs origin gating.
TEST_F(ActorCommitDeferringConditionUserBackForwardTest,
       StayKeepsTaskAndDoesNotResume) {
  ExpectConfirmation();
  EXPECT_CALL(mock_delegate_, RequestToConfirmNavigation).Times(0);
  auto condition = CreateCondition();
  ASSERT_NE(condition, nullptr);

  EXPECT_EQ(condition->WillCommitNavigation(
                base::MakeExpectedNotRunClosure(FROM_HERE)),
            content::CommitDeferringCondition::Result::kDefer);
  ASSERT_TRUE(confirm_);

  std::move(confirm_).Run(false);
  EXPECT_NE(GetTask(), nullptr);
}

// HistoryTool's own Back/Forward skips the confirmation and keeps origin
// gating.
TEST_F(ActorCommitDeferringConditionUserBackForwardTest,
       HistoryToolNavigationSkipsConfirmation) {
  HistoryToolNavigation::CreateForNavigationHandle(*handle_);
  ExpectOriginGating(/*allowed=*/true);
  auto condition = CreateCondition();
  ASSERT_NE(condition, nullptr);

  base::test::TestFuture<void> resume_future;
  EXPECT_EQ(condition->WillCommitNavigation(resume_future.GetCallback()),
            content::CommitDeferringCondition::Result::kDefer);
  EXPECT_TRUE(resume_future.Wait());
}

// If the delegate shows no confirmation, origin gating still resumes the
// commit.
TEST_F(ActorCommitDeferringConditionUserBackForwardTest,
       NoConfirmationFallsBackToGating) {
  EXPECT_CALL(delegate_, MaybeDeferNavigation).WillOnce(Return(false));
  ExpectOriginGating(/*allowed=*/true);
  auto condition = CreateCondition();
  ASSERT_NE(condition, nullptr);

  base::test::TestFuture<void> resume_future;
  EXPECT_EQ(condition->WillCommitNavigation(resume_future.GetCallback()),
            content::CommitDeferringCondition::Result::kDefer);
  EXPECT_TRUE(resume_future.Wait());
}

// A restore started by the page, such as history.back(), is
// renderer-initiated. GetUserBackForwardDelegate() does not treat it as a user
// Back/Forward, so only origin gating runs.
TEST_F(ActorCommitDeferringConditionUserBackForwardTest,
       RendererInitiatedRestoreSkipsConfirmation) {
  handle_->set_is_renderer_initiated(true);
  ExpectOriginGating(/*allowed=*/true);
  auto condition = CreateCondition();
  ASSERT_NE(condition, nullptr);

  base::test::TestFuture<void> resume_future;
  EXPECT_EQ(condition->WillCommitNavigation(resume_future.GetCallback()),
            content::CommitDeferringCondition::Result::kDefer);
  EXPECT_TRUE(resume_future.Wait());
}

#if BUILDFLAG(IS_ANDROID)
// Android phones skip the confirmation, and origin gating still resumes the
// commit.
TEST_F(ActorCommitDeferringConditionUserBackForwardTest,
       PhoneSkipsConfirmation) {
  base::android::device_info::set_is_desktop_for_testing(false);
  ExpectOriginGating(/*allowed=*/true);
  auto condition = CreateCondition();
  ASSERT_NE(condition, nullptr);

  base::test::TestFuture<void> resume_future;
  EXPECT_EQ(condition->WillCommitNavigation(resume_future.GetCallback()),
            content::CommitDeferringCondition::Result::kDefer);
  EXPECT_TRUE(resume_future.Wait());
}
#endif  // BUILDFLAG(IS_ANDROID)

// Same setup with origin gating off. The confirmation must still run.
class ActorCommitDeferringConditionUserBackForwardGatingDisabledTest
    : public ActorCommitDeferringConditionUserBackForwardTest {
 protected:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    return {kGlicPageActivationGating};
  }
};

// Leave stops the task and resumes the commit.
TEST_F(ActorCommitDeferringConditionUserBackForwardGatingDisabledTest,
       LeaveStopsTaskAndResumes) {
  ExpectConfirmation();
  auto condition = CreateCondition();
  ASSERT_NE(condition, nullptr);

  base::test::TestFuture<void> resume_future;
  EXPECT_EQ(condition->WillCommitNavigation(resume_future.GetCallback()),
            content::CommitDeferringCondition::Result::kDefer);
  ASSERT_TRUE(confirm_);

  std::move(confirm_).Run(true);
  EXPECT_TRUE(resume_future.Wait());
  EXPECT_EQ(GetTask(), nullptr);
}

// Without a confirmation and without origin gating, the commit proceeds.
TEST_F(ActorCommitDeferringConditionUserBackForwardGatingDisabledTest,
       NoConfirmationProceeds) {
  EXPECT_CALL(delegate_, MaybeDeferNavigation).WillOnce(Return(false));
  auto condition = CreateCondition();
  ASSERT_NE(condition, nullptr);

  EXPECT_EQ(condition->WillCommitNavigation(
                base::MakeExpectedNotRunClosure(FROM_HERE)),
            content::CommitDeferringCondition::Result::kProceed);
}

// HistoryTool restores are not confirmed, so no condition is created.
TEST_F(ActorCommitDeferringConditionUserBackForwardGatingDisabledTest,
       HistoryToolNavigationCreatesNoCondition) {
  HistoryToolNavigation::CreateForNavigationHandle(*handle_);
  EXPECT_EQ(CreateCondition(), nullptr);
}

// Renderer-initiated restores are not confirmed, so no condition is created.
TEST_F(ActorCommitDeferringConditionUserBackForwardGatingDisabledTest,
       RendererInitiatedRestoreCreatesNoCondition) {
  handle_->set_is_renderer_initiated(true);
  EXPECT_EQ(CreateCondition(), nullptr);
}

#if !BUILDFLAG(IS_ANDROID)
// With kGlicConfirmTabClose off, a user Back/Forward restore is not confirmed.
class ActorCommitDeferringConditionConfirmTabCloseDisabledTest
    : public ActorCommitDeferringConditionUserBackForwardTest {
 protected:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    return {features::kGlicConfirmTabClose};
  }
};

// Origin gating still runs, and the delegate is not asked.
TEST_F(ActorCommitDeferringConditionConfirmTabCloseDisabledTest,
       SkipsConfirmation) {
  ExpectOriginGating(/*allowed=*/true);
  auto condition = CreateCondition();
  ASSERT_NE(condition, nullptr);

  base::test::TestFuture<void> resume_future;
  EXPECT_EQ(condition->WillCommitNavigation(resume_future.GetCallback()),
            content::CommitDeferringCondition::Result::kDefer);
  EXPECT_TRUE(resume_future.Wait());
}

// With origin gating also off, nothing defers the restore.
class ActorCommitDeferringConditionConfirmTabCloseAndGatingDisabledTest
    : public ActorCommitDeferringConditionUserBackForwardTest {
 protected:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    return {features::kGlicConfirmTabClose, kGlicPageActivationGating};
  }
};

TEST_F(ActorCommitDeferringConditionConfirmTabCloseAndGatingDisabledTest,
       CreatesNoCondition) {
  EXPECT_EQ(CreateCondition(), nullptr);
}
#endif  // !BUILDFLAG(IS_ANDROID)

class ActorCommitDeferringConditionGatingDisabledTest
    : public ActorCommitDeferringConditionTest {
 protected:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    return {kGlicPageActivationGating};
  }
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
