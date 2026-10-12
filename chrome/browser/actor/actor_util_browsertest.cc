// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_util.h"

#include <memory>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/actor/core/task_source_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

namespace actor {

namespace {

// A tool request that behaves like a Wait action but allows us to control
// RequiresOpeningWebContents.
class TestToolRequest : public WaitToolRequest {
 public:
  explicit TestToolRequest(bool requires_opening_web_contents)
      : WaitToolRequest(base::Hours(1)),
        requires_opening_web_contents_(requires_opening_web_contents) {}

  bool RequiresOpeningWebContents() const override {
    return requires_opening_web_contents_;
  }

 private:
  const bool requires_opening_web_contents_;
};

}  // namespace

class ActorUtilBrowserTest : public PlatformBrowserTest {
 public:
  ActorUtilBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicActor,
                              features::kGlicActorPolicyControlExemption},
        /*disabled_features=*/{features::kGlicWarming});
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/517620369): Decouple test from Glic eligibility criteria.
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      GTEST_SKIP() << "Actor requires Android S+ to run";
    }
#endif
    PlatformBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  tabs::TabInterface* active_tab() {
    return chrome_test_utils::GetActiveTab(this);
  }

  content::WebContents* web_contents() { return active_tab()->GetContents(); }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(GetProfile());
  }

  glic::GlicKeyedService* glic_keyed_service() {
    return glic::GlicKeyedService::Get(GetProfile());
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 private:
  glic::GlicTestEnvironment glic_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that HasActorTaskPreventingNewWebContents returns true when a task is
// acting and the current action doesn't allow opening new web contents.
IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest, PreventsNewWebContents) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  // Navigate to an initial page first.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));

  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  actor::AddTabToTask(*active_tab(), *task);

  auto test_request = std::make_unique<TestToolRequest>(
      /*requires_opening_web_contents=*/false);

  PerformActionsFuture result_future;
  actor_keyed_service()->PerformActions(
      task_id,
      ToRequestList<std::unique_ptr<ToolRequest>>(std::move(test_request)),
      ActorTaskMetadata(), result_future.GetCallback());

  // Wait for the engine to reach kToolInvoke state.
  base::RunLoop run_loop;
  ExecutionEngineStateWaiter waiter(run_loop.QuitClosure(),
                                    task->GetExecutionEngine(),
                                    ExecutionEngine::State::kToolInvoke);
  run_loop.Run();

  // Verify that the task prevents opening new web contents.
  EXPECT_TRUE(actor::HasActorTaskPreventingNewWebContents(main_frame()));

  // Verify that window.open is overridden to load in the same tab.
  const GURL new_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationObserver observer(web_contents());

  // Use ExecJs to ensure a user gesture is provided.
  EXPECT_TRUE(content::ExecJs(
      main_frame(),
      base::StringPrintf("window.open('%s')", new_url.spec().c_str())));
  observer.Wait();

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), new_url);
}

// Tests that HasActorTaskPreventingNewWebContents returns false when a task is
// acting but the current action explicitly allows opening new web contents.
IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest, AllowsNewWebContents) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  actor::AddTabToTask(*active_tab(), *task);

  auto test_request = std::make_unique<TestToolRequest>(
      /*requires_opening_web_contents=*/true);

  PerformActionsFuture result_future;
  actor_keyed_service()->PerformActions(
      task_id,
      ToRequestList<std::unique_ptr<ToolRequest>>(std::move(test_request)),
      ActorTaskMetadata(), result_future.GetCallback());

  // Wait for the engine to reach kToolInvoke state.
  base::RunLoop run_loop;
  ExecutionEngineStateWaiter waiter(run_loop.QuitClosure(),
                                    task->GetExecutionEngine(),
                                    ExecutionEngine::State::kToolInvoke);
  run_loop.Run();

  // Verify that the engine is in kToolInvoke.
  EXPECT_EQ(task->GetExecutionEngine().state(),
            ExecutionEngine::State::kToolInvoke);

  // Verify that the task does NOT prevent opening new web contents.
  EXPECT_FALSE(actor::HasActorTaskPreventingNewWebContents(main_frame()));
}

// Tests that HasActorTaskPreventingNewWebContents returns false when no task is
// acting on the tab.
IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest, NoTaskNoPrevention) {
  EXPECT_FALSE(actor::HasActorTaskPreventingNewWebContents(main_frame()));
}

IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest,
                       GetGlicInstanceForTaskNoGlicInstance) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TaskSourceInfo(TaskSourceInfo::Client::kBrowserActuator, "session-1"),
      NoEnterprisePolicyChecker());
  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);
  actor::AddTabToTask(*active_tab(), *task);

  EXPECT_EQ(GetGlicInstanceForTask(*task), nullptr);
  EXPECT_FALSE(HasGlicInstanceForTask(*task));
}

IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest,
                       GetGlicInstanceForTaskByConversationId) {
  glic::GlicInvokeOptions options(
      glic::Target(*active_tab(), glic::ConversationId("conv-123")),
      glic::mojom::InvocationSource::kOsButton);
  options.fre_completion_wait_mode = glic::FreCompletionWaitMode::kNever;
  base::WeakPtr<glic::GlicInstance> instance =
      glic_keyed_service()->Invoke(std::move(options));
  ASSERT_TRUE(instance);

  TaskId task_id = actor_keyed_service()->CreateTask(
      TaskSourceInfo(TaskSourceInfo::Client::kBrowserActuator, "conv-123"),
      NoEnterprisePolicyChecker());
  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);

  EXPECT_EQ(GetGlicInstanceForTask(*task), instance.get());
  EXPECT_TRUE(HasGlicInstanceForTask(*task));
}

IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest,
                       GetGlicInstanceForTaskIgnoresUnrelatedTabInstance) {
  glic::GlicInvokeOptions options(
      glic::Target(*active_tab(), glic::ConversationId("conv-456")),
      glic::mojom::InvocationSource::kOsButton);
  options.fre_completion_wait_mode = glic::FreCompletionWaitMode::kNever;
  base::WeakPtr<glic::GlicInstance> instance =
      glic_keyed_service()->Invoke(std::move(options));
  ASSERT_TRUE(instance);

  TaskId task_id = actor_keyed_service()->CreateTask(
      TaskSourceInfo(TaskSourceInfo::Client::kBrowserActuator, "other-conv"),
      NoEnterprisePolicyChecker());
  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);
  actor::AddTabToTask(*active_tab(), *task);

  EXPECT_EQ(GetGlicInstanceForTask(*task), nullptr);
  EXPECT_FALSE(HasGlicInstanceForTask(*task));
}

IN_PROC_BROWSER_TEST_F(ActorUtilBrowserTest, GetGlicInstanceForTaskByTaskId) {
  glic::GlicInvokeOptions options(
      glic::Target(*active_tab(), glic::NewConversation()),
      glic::mojom::InvocationSource::kOsButton);
  options.fre_completion_wait_mode = glic::FreCompletionWaitMode::kNever;
  base::WeakPtr<glic::GlicInstance> instance =
      glic_keyed_service()->Invoke(std::move(options));
  ASSERT_TRUE(instance);
  ASSERT_TRUE(instance->GetActorTaskManager());

  mojo::Remote<glic::mojom::ActorHandler> actor_handler;
  mojo::MessagePipe client_pipe;
  instance->GetActorTaskManager()->Bind(
      actor_handler.BindNewPipeAndPassReceiver(),
      mojo::PendingRemote<glic::mojom::ActorClient>(
          std::move(client_pipe.handle0), 0));

  base::test::TestFuture<
      base::expected<int32_t, glic::mojom::CreateTaskErrorReason>>
      create_task_future;
  instance->GetActorTaskManager()->GetClientSessionForTesting()->CreateTask(
      actor::webui::mojom::TaskOptions::New(),
      create_task_future.GetCallback());
  ASSERT_TRUE(create_task_future.Get().has_value());

  ActorTask* task =
      actor_keyed_service()->GetTask(TaskId(create_task_future.Get().value()));
  ASSERT_TRUE(task);
  ASSERT_FALSE(task->source_info().id.has_value());

  EXPECT_EQ(GetGlicInstanceForTask(*task), instance.get());
  EXPECT_TRUE(HasGlicInstanceForTask(*task));
}

}  // namespace actor
