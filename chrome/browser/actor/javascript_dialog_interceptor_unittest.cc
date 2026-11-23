// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/javascript_dialog_interceptor.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/task_source_info.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager_delegate.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {
namespace {

class FakeJavaScriptDialogManager : public content::JavaScriptDialogManager {
 public:
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           content::JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {
    *did_suppress_message = false;
    std::move(callback).Run(true, u"");
  }

  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             content::RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override {
    beforeunload_calls_++;
    last_beforeunload_web_contents_ = web_contents;
    last_beforeunload_is_reload_ = is_reload;
    std::move(callback).Run(beforeunload_result_, u"");
  }

  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override {
    handle_calls_++;
    last_handle_web_contents_ = web_contents;
    last_handle_accept_ = accept;
    last_handle_prompt_override_ =
        prompt_override ? std::optional<std::u16string>(*prompt_override)
                        : std::nullopt;
    return handle_return_value_;
  }

  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override {
    cancel_calls_++;
    last_cancel_web_contents_ = web_contents;
    last_cancel_reset_state_ = reset_state;
  }

  int beforeunload_calls_ = 0;
  raw_ptr<content::WebContents> last_beforeunload_web_contents_ = nullptr;
  bool last_beforeunload_is_reload_ = false;
  bool beforeunload_result_ = false;

  int handle_calls_ = 0;
  raw_ptr<content::WebContents> last_handle_web_contents_ = nullptr;
  bool last_handle_accept_ = false;
  std::optional<std::u16string> last_handle_prompt_override_;
  bool handle_return_value_ = true;

  int cancel_calls_ = 0;
  raw_ptr<content::WebContents> last_cancel_web_contents_ = nullptr;
  bool last_cancel_reset_state_ = false;
};

class FakeTabModalDialogManagerDelegate
    : public javascript_dialogs::TabModalDialogManagerDelegate {
 public:
  base::WeakPtr<javascript_dialogs::TabModalDialogView> CreateNewDialog(
      content::WebContents* alerting_web_contents,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_closed_callback) override {
    return nullptr;
  }
  void WillRunDialog() override {}
  void DidCloseDialog() override {}
  void SetTabNeedsAttention(bool attention) override {}
  bool IsWebContentsForemost() override { return false; }
  bool IsApp() override { return false; }
  bool CanShowModalUI() override { return true; }
};

class JavaScriptDialogInterceptorTest : public ChromeRenderViewHostTestHarness {
 protected:
  JavaScriptDialogInterceptorTest() {
    scoped_feature_list_.InitAndEnableFeature(kActorInterceptJavaScriptDialogs);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com/"));
    JavaScriptDialogInterceptor::CreateForWebContents(web_contents());
  }

  JavaScriptDialogInterceptor* interceptor() {
    return JavaScriptDialogInterceptor::FromWebContents(web_contents());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(JavaScriptDialogInterceptorTest, InterceptsAlertConfirmPromptInOrder) {
  struct CallbackResult {
    bool called = false;
    bool accept = false;
    std::u16string prompt_text;
  };

  auto run_dialog = [&](content::JavaScriptDialogType type,
                        const std::u16string& message,
                        const std::u16string& default_prompt = u"") {
    CallbackResult result;
    bool suppressed = true;
    interceptor()->RunJavaScriptDialog(
        web_contents(), main_rfh(), type, message, default_prompt,
        base::BindLambdaForTesting(
            [&](bool accept, const std::u16string& prompt_text) {
              result.called = true;
              result.accept = accept;
              result.prompt_text = prompt_text;
            }),
        &suppressed);
    EXPECT_FALSE(suppressed);
    EXPECT_TRUE(result.called);
    return result;
  };

  const CallbackResult alert_res = run_dialog(
      content::JAVASCRIPT_DIALOG_TYPE_ALERT, u"Successfully added to waitlist");
  EXPECT_TRUE(alert_res.accept);
  EXPECT_TRUE(alert_res.prompt_text.empty());

  const CallbackResult confirm_res =
      run_dialog(content::JAVASCRIPT_DIALOG_TYPE_CONFIRM, u"Delete item?");
  EXPECT_FALSE(confirm_res.accept);
  EXPECT_TRUE(confirm_res.prompt_text.empty());

  const CallbackResult prompt_res = run_dialog(
      content::JAVASCRIPT_DIALOG_TYPE_PROMPT, u"Enter name:", u"Alice");
  EXPECT_FALSE(prompt_res.accept);
  EXPECT_TRUE(prompt_res.prompt_text.empty());

  std::vector<InterceptedJavaScriptDialog> dialogs =
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents());
  ASSERT_EQ(dialogs.size(), 3u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "Successfully added to waitlist");
  EXPECT_EQ(dialogs[1].type, JavaScriptDialogType::kConfirm);
  EXPECT_EQ(dialogs[1].dialog_message, "Delete item?");
  EXPECT_EQ(dialogs[2].type, JavaScriptDialogType::kPrompt);
  EXPECT_EQ(dialogs[2].dialog_message, "Enter name:");

  // Taking drains the buffer.
  EXPECT_TRUE(
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents())
          .empty());
}

TEST_F(JavaScriptDialogInterceptorTest,
       DoesNotCancelFallbackManagerOnIntercept) {
  FakeJavaScriptDialogManager fallback;
  interceptor()->SetFallbackDialogManagerForTesting(&fallback);
  base::ScopedClosureRunner reset_fallback(base::BindOnce(
      &JavaScriptDialogInterceptor::SetFallbackDialogManagerForTesting,
      base::Unretained(interceptor()), nullptr));

  bool suppressed = true;
  bool callback_called = false;
  interceptor()->RunJavaScriptDialog(
      web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
      u"hello", u"",
      base::BindLambdaForTesting(
          [&](bool accept, const std::u16string&) { callback_called = true; }),
      &suppressed);

  EXPECT_EQ(fallback.cancel_calls_, 0);
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(suppressed);
}

TEST_F(JavaScriptDialogInterceptorTest,
       ClearInterceptedDialogsClearsWithoutCreating) {
  std::unique_ptr<content::WebContents> untouched = CreateTestWebContents();
  JavaScriptDialogInterceptor::ClearInterceptedDialogs(*untouched);
  EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(untouched.get()),
            nullptr);

  bool suppressed = true;
  interceptor()->RunJavaScriptDialog(
      web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
      u"stale", u"",
      base::BindLambdaForTesting([](bool, const std::u16string&) {}),
      &suppressed);
  JavaScriptDialogInterceptor::ClearInterceptedDialogs(*web_contents());
  EXPECT_TRUE(
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents())
          .empty());
}

TEST_F(JavaScriptDialogInterceptorTest, CapsBufferedDialogsAndStillDismisses) {
  constexpr size_t kTotalDialogs =
      JavaScriptDialogInterceptor::kMaxInterceptedDialogs + 5;
  size_t dismissed_count = 0;

  for (size_t i = 0; i < kTotalDialogs; ++i) {
    bool suppressed = true;
    interceptor()->RunJavaScriptDialog(
        web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
        base::UTF8ToUTF16("dialog-" + base::NumberToString(i)), u"",
        base::BindLambdaForTesting([&](bool accept, const std::u16string&) {
          EXPECT_TRUE(accept);
          ++dismissed_count;
        }),
        &suppressed);
    EXPECT_FALSE(suppressed);
  }

  EXPECT_EQ(dismissed_count, kTotalDialogs);
  std::vector<InterceptedJavaScriptDialog> dialogs =
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents());
  ASSERT_EQ(dialogs.size(),
            JavaScriptDialogInterceptor::kMaxInterceptedDialogs);
  EXPECT_EQ(dialogs.front().dialog_message, "dialog-0");
  EXPECT_EQ(
      dialogs.back().dialog_message,
      "dialog-" + base::NumberToString(
                      JavaScriptDialogInterceptor::kMaxInterceptedDialogs - 1));
}

TEST_F(JavaScriptDialogInterceptorTest, TruncatesLongMessagesOnUtf8Boundary) {
  // Use a 1-byte ASCII prefix followed by 4-byte UTF-8 emoji sequences so a
  // naive byte slice at kMaxDialogMessageBytes would split a code point.
  std::string long_utf8 = "a";
  const std::string emoji = "\xF0\x9F\x98\x80";  // U+1F600
  while (long_utf8.size() <=
         JavaScriptDialogInterceptor::kMaxDialogMessageBytes + 16) {
    long_utf8 += emoji;
  }

  bool suppressed = true;
  bool callback_called = false;
  interceptor()->RunJavaScriptDialog(
      web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
      base::UTF8ToUTF16(long_utf8), u"",
      base::BindLambdaForTesting(
          [&](bool, const std::u16string&) { callback_called = true; }),
      &suppressed);
  EXPECT_TRUE(callback_called);

  std::vector<InterceptedJavaScriptDialog> dialogs =
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents());
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_LE(dialogs[0].dialog_message.size(),
            JavaScriptDialogInterceptor::kMaxDialogMessageBytes);
  EXPECT_GT(dialogs[0].dialog_message.size(),
            JavaScriptDialogInterceptor::kMaxDialogMessageBytes - 4);
  EXPECT_TRUE(base::IsStringUTF8(dialogs[0].dialog_message));
}

TEST_F(JavaScriptDialogInterceptorTest,
       TruncatesAsciiPrefixBeforeSurrogatePairAtByteBoundary) {
  // Place a 4-byte emoji (UTF-16 surrogate pair) straddling the exact UTF-16
  // prefix limit (kMaxDialogMessageBytes - 1 ASCII bytes + surrogate pair) to
  // verify pre-slicing does not corrupt the boundary code point into U+FFFD.
  std::string utf8_with_boundary_emoji(
      JavaScriptDialogInterceptor::kMaxDialogMessageBytes - 1, 'a');
  utf8_with_boundary_emoji += "\xF0\x9F\x98\x80";  // U+1F600
  utf8_with_boundary_emoji += "tail";

  bool suppressed = true;
  interceptor()->RunJavaScriptDialog(
      web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
      base::UTF8ToUTF16(utf8_with_boundary_emoji), u"",
      base::BindLambdaForTesting([](bool, const std::u16string&) {}),
      &suppressed);

  std::vector<InterceptedJavaScriptDialog> dialogs =
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents());
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].dialog_message,
            std::string(JavaScriptDialogInterceptor::kMaxDialogMessageBytes - 1,
                        'a'));
}

TEST_F(JavaScriptDialogInterceptorTest,
       DelegatesBeforeUnloadHandleAndCancelToFallback) {
  FakeJavaScriptDialogManager fallback;
  fallback.beforeunload_result_ = false;
  fallback.handle_return_value_ = true;
  interceptor()->SetFallbackDialogManagerForTesting(&fallback);
  base::ScopedClosureRunner reset_fallback(base::BindOnce(
      &JavaScriptDialogInterceptor::SetFallbackDialogManagerForTesting,
      base::Unretained(interceptor()), nullptr));

  bool beforeunload_called = false;
  bool beforeunload_proceed = true;
  interceptor()->RunBeforeUnloadDialog(
      web_contents(), main_rfh(), /*is_reload=*/true,
      base::BindLambdaForTesting([&](bool proceed, const std::u16string&) {
        beforeunload_called = true;
        beforeunload_proceed = proceed;
      }));
  EXPECT_TRUE(beforeunload_called);
  EXPECT_FALSE(beforeunload_proceed);
  EXPECT_EQ(fallback.beforeunload_calls_, 1);
  EXPECT_TRUE(fallback.last_beforeunload_is_reload_);

  const std::u16string override_text = u"prompt_value";
  EXPECT_TRUE(interceptor()->HandleJavaScriptDialog(web_contents(), true,
                                                    &override_text));
  EXPECT_EQ(fallback.handle_calls_, 1);
  EXPECT_TRUE(fallback.last_handle_accept_);
  EXPECT_EQ(fallback.last_handle_prompt_override_, override_text);

  interceptor()->CancelDialogs(web_contents(), /*reset_state=*/true);
  EXPECT_EQ(fallback.cancel_calls_, 1);
  EXPECT_TRUE(fallback.last_cancel_reset_state_);
}

TEST_F(JavaScriptDialogInterceptorTest,
       BeforeUnloadAutoProceedsWithoutFallback) {
  bool called = false;
  bool proceed = false;
  interceptor()->RunBeforeUnloadDialog(
      web_contents(), main_rfh(), /*is_reload=*/false,
      base::BindLambdaForTesting([&](bool success, const std::u16string&) {
        called = true;
        proceed = success;
      }));
  EXPECT_TRUE(called);
  EXPECT_TRUE(proceed);
  EXPECT_FALSE(
      interceptor()->HandleJavaScriptDialog(web_contents(), true,
                                            /*prompt_override=*/nullptr));
  interceptor()->CancelDialogs(web_contents(), /*reset_state=*/true);
}

TEST_F(JavaScriptDialogInterceptorTest,
       MaybeGetRequiresActiveControllingTaskAndFeature) {
  EXPECT_EQ(JavaScriptDialogInterceptor::MaybeGetForWebContents(nullptr),
            nullptr);
  EXPECT_EQ(JavaScriptDialogInterceptor::MaybeGetForWebContents(web_contents()),
            nullptr);

  ActorKeyedService* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  ScopedHeadlessActorSurface headless =
      actor_service->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  content::WebContents* headless_contents = handle.Get()->GetWebContents();
  ASSERT_TRUE(headless_contents);

  // Not yet controlled by a task.
  EXPECT_EQ(
      JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents),
      nullptr);
  EXPECT_EQ(headless_contents->GetDelegate()->GetJavaScriptDialogManager(
                headless_contents),
            nullptr);

  const TaskId task_id = actor_service->CreateTask(
      TaskSourceInfo(TaskSourceInfo::Client::kTest, "test_session"),
      GetNullEnterprisePolicyChecker());
  ActorTask* task = actor_service->GetTask(task_id);
  ASSERT_TRUE(task);

  base::test::TestFuture<mojom::ActionResultPtr> add_future;
  task->AddActorSurface(handle, /*stop_task_on_detach=*/true,
                        add_future.GetCallback());
  ASSERT_TRUE(add_future.Wait());

  content::JavaScriptDialogManager* manager =
      JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents);
  ASSERT_NE(manager, nullptr);
  EXPECT_EQ(headless_contents->GetDelegate()->GetJavaScriptDialogManager(
                headless_contents),
            manager);

  // Record a dialog, then pause the task: interceptor should detach and clear
  // buffered dialogs so nothing leaks after actor control exits.
  bool suppressed = true;
  manager->RunJavaScriptDialog(
      headless_contents, headless_contents->GetPrimaryMainFrame(),
      content::JAVASCRIPT_DIALOG_TYPE_ALERT, u"paused?", u"",
      base::BindLambdaForTesting([](bool, const std::u16string&) {}),
      &suppressed);

  task->Pause(/*from_actor=*/false);
  EXPECT_EQ(
      JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents),
      nullptr);
  EXPECT_TRUE(
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*headless_contents)
          .empty());

  // Resuming re-enables interception.
  task->Resume();
  EXPECT_NE(
      JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents),
      nullptr);

  // Interception stays enabled when `kActorInterceptJavaScriptDialogs` is
  // enabled even if `kActorExecuteJsTool` is disabled.
  {
    base::test::ScopedFeatureList execute_js_disabled;
    execute_js_disabled.InitWithFeatures({kActorInterceptJavaScriptDialogs},
                                         {kActorExecuteJsTool});
    EXPECT_NE(
        JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents),
        nullptr);
  }

  // Disabling `kActorInterceptJavaScriptDialogs` disables interception even
  // when `kActorExecuteJsTool` is enabled.
  {
    base::test::ScopedFeatureList intercept_disabled;
    intercept_disabled.InitWithFeatures({kActorExecuteJsTool},
                                        {kActorInterceptJavaScriptDialogs});
    EXPECT_EQ(
        JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents),
        nullptr);
  }

  actor_service->StopTask(task_id, ActorTask::StoppedReason::kTaskComplete);
  EXPECT_EQ(
      JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents),
      nullptr);
}

TEST_F(JavaScriptDialogInterceptorTest,
       RemovesUserDataWhenStoppedOrRemovedDuringInterruptWithUserControl) {
  ActorKeyedService* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  // Case 1: StopTask() while interrupted with user control detaches UserData
  // and clears buffered dialogs.
  {
    ScopedHeadlessActorSurface headless =
        actor_service->CreateHeadlessActorSurface();
    const ActorSurfaceHandle handle = headless.actor_surface_handle();
    content::WebContents* headless_contents = handle.Get()->GetWebContents();
    ASSERT_TRUE(headless_contents);

    const TaskId task_id = actor_service->CreateTask(
        TaskSourceInfo(TaskSourceInfo::Client::kTest, "test_session"),
        GetNullEnterprisePolicyChecker());
    ActorTask* task = actor_service->GetTask(task_id);
    ASSERT_TRUE(task);

    base::test::TestFuture<mojom::ActionResultPtr> add_future;
    task->AddActorSurface(handle, /*stop_task_on_detach=*/true,
                          add_future.GetCallback());
    ASSERT_TRUE(add_future.Wait());
    task->SetState(ActorTask::State::kReflecting);

    content::JavaScriptDialogManager* manager =
        JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents);
    ASSERT_NE(manager, nullptr);
    bool suppressed = true;
    manager->RunJavaScriptDialog(
        headless_contents, headless_contents->GetPrimaryMainFrame(),
        content::JAVASCRIPT_DIALOG_TYPE_ALERT, u"buffered before interrupt",
        u"", base::BindLambdaForTesting([](bool, const std::u16string&) {}),
        &suppressed);
    ASSERT_NE(JavaScriptDialogInterceptor::FromWebContents(headless_contents),
              nullptr);

    task->Interrupt(/*retain_user_control=*/true,
                    ActorTask::InterruptReason::kWaitingUserTakeOver);
    actor_service->StopTask(task_id, ActorTask::StoppedReason::kTaskComplete);

    EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(headless_contents),
              nullptr);
    EXPECT_TRUE(
        JavaScriptDialogInterceptor::TakeInterceptedDialogs(*headless_contents)
            .empty());
  }

  // Case 2: RemoveActorSurface() while interrupted with user control detaches
  // UserData even if an earlier navigation during an interruption detached
  // `ActorControlledTabState` from its observed WebContents.
  {
    ScopedHeadlessActorSurface headless =
        actor_service->CreateHeadlessActorSurface();
    const ActorSurfaceHandle handle = headless.actor_surface_handle();
    content::WebContents* headless_contents = handle.Get()->GetWebContents();
    ASSERT_TRUE(headless_contents);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        headless_contents, GURL("https://example.com/initial"));

    const TaskId task_id = actor_service->CreateTask(
        TaskSourceInfo(TaskSourceInfo::Client::kTest, "test_session"),
        GetNullEnterprisePolicyChecker());
    ActorTask* task = actor_service->GetTask(task_id);
    ASSERT_TRUE(task);

    base::test::TestFuture<mojom::ActionResultPtr> add_future;
    task->AddActorSurface(handle, /*stop_task_on_detach=*/false,
                          add_future.GetCallback());
    ASSERT_TRUE(add_future.Wait());
    task->SetState(ActorTask::State::kReflecting);

    // Navigate during user-control interruption to detach
    // `ActorControlledTabState::web_contents()`, then resume and re-attach an
    // interceptor before interrupting and removing the surface.
    task->Interrupt(/*retain_user_control=*/true,
                    ActorTask::InterruptReason::kWaitingUserTakeOver);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        headless_contents, GURL("https://example.com/next"));
    task->Uninterrupt(ActorTask::State::kReflecting);

    content::JavaScriptDialogManager* manager =
        JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents);
    ASSERT_NE(manager, nullptr);
    bool suppressed = true;
    manager->RunJavaScriptDialog(
        headless_contents, headless_contents->GetPrimaryMainFrame(),
        content::JAVASCRIPT_DIALOG_TYPE_ALERT, u"post-nav alert", u"",
        base::BindLambdaForTesting([](bool, const std::u16string&) {}),
        &suppressed);
    ASSERT_NE(JavaScriptDialogInterceptor::FromWebContents(headless_contents),
              nullptr);

    task->Interrupt(/*retain_user_control=*/true,
                    ActorTask::InterruptReason::kWaitingUserTakeOver);
    task->RemoveActorSurface(handle);

    EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(headless_contents),
              nullptr);
    EXPECT_TRUE(
        JavaScriptDialogInterceptor::TakeInterceptedDialogs(*headless_contents)
            .empty());

    actor_service->StopTask(task_id, ActorTask::StoppedReason::kTaskComplete);
  }
}

TEST_F(JavaScriptDialogInterceptorTest,
       ObserveOnceAndOtherTaskExitsDoNotDropActiveTaskDialogs) {
  ActorKeyedService* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  auto buffer_alert = [](content::WebContents* contents,
                         const std::u16string& message) {
    content::JavaScriptDialogManager* manager =
        JavaScriptDialogInterceptor::MaybeGetForWebContents(contents);
    ASSERT_NE(manager, nullptr);
    bool suppressed = true;
    manager->RunJavaScriptDialog(
        contents, contents->GetPrimaryMainFrame(),
        content::JAVASCRIPT_DIALOG_TYPE_ALERT, message, u"",
        base::BindLambdaForTesting([](bool, const std::u16string&) {}),
        &suppressed);
  };

  // Case 1: Cross-task ObserveActorSurfaceOnce() + StopTask() (which runs
  // ResetToObserveTabsSet()) must not destroy Task A's interceptor or dialogs.
  {
    ScopedHeadlessActorSurface headless =
        actor_service->CreateHeadlessActorSurface();
    const ActorSurfaceHandle handle = headless.actor_surface_handle();
    content::WebContents* contents = handle.Get()->GetWebContents();
    ASSERT_TRUE(contents);

    const TaskId task_a_id = actor_service->CreateTask(
        TaskSourceInfo(TaskSourceInfo::Client::kTest, "task_a"),
        GetNullEnterprisePolicyChecker());
    const TaskId task_b_id = actor_service->CreateTask(
        TaskSourceInfo(TaskSourceInfo::Client::kTest, "task_b"),
        GetNullEnterprisePolicyChecker());
    ActorTask* task_a = actor_service->GetTask(task_a_id);
    ActorTask* task_b = actor_service->GetTask(task_b_id);
    ASSERT_TRUE(task_a);
    ASSERT_TRUE(task_b);

    base::test::TestFuture<mojom::ActionResultPtr> add_future;
    task_a->AddActorSurface(handle, /*stop_task_on_detach=*/false,
                            add_future.GetCallback());
    ASSERT_TRUE(add_future.Wait());
    task_a->SetState(ActorTask::State::kReflecting);

    ASSERT_NO_FATAL_FAILURE(buffer_alert(contents, u"task a dialog"));

    task_b->ObserveActorSurfaceOnce(handle);
    actor_service->StopTask(task_b_id, ActorTask::StoppedReason::kTaskComplete);

    EXPECT_NE(JavaScriptDialogInterceptor::FromWebContents(contents), nullptr);
    auto dialogs =
        JavaScriptDialogInterceptor::TakeInterceptedDialogs(*contents);
    ASSERT_EQ(dialogs.size(), 1u);
    EXPECT_EQ(dialogs[0].dialog_message, "task a dialog");

    actor_service->StopTask(task_a_id, ActorTask::StoppedReason::kTaskComplete);
    EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(contents), nullptr);
  }

  // Case 2: Same-task ObserveActorSurfaceOnce(S1) followed by
  // AddActorSurface(S1) and ResetToObserveTabsSet() (via Act on S2) must not
  // tear down S1's interceptor or drop its buffered dialogs.
  {
    ScopedHeadlessActorSurface surface1 =
        actor_service->CreateHeadlessActorSurface();
    ScopedHeadlessActorSurface surface2 =
        actor_service->CreateHeadlessActorSurface();
    const ActorSurfaceHandle handle1 = surface1.actor_surface_handle();
    const ActorSurfaceHandle handle2 = surface2.actor_surface_handle();
    content::WebContents* contents1 = handle1.Get()->GetWebContents();
    content::WebContents* contents2 = handle2.Get()->GetWebContents();
    ASSERT_TRUE(contents1);
    ASSERT_TRUE(contents2);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        contents2, GURL("https://example.com/surface2"));

    const TaskId task_id = actor_service->CreateTask(
        TaskSourceInfo(TaskSourceInfo::Client::kTest, "same_task"),
        GetNullEnterprisePolicyChecker());
    ActorTask* task = actor_service->GetTask(task_id);
    ASSERT_TRUE(task);

    task->ObserveActorSurfaceOnce(handle1);
    base::test::TestFuture<mojom::ActionResultPtr> add_future;
    task->AddActorSurface(handle1, /*stop_task_on_detach=*/false,
                          add_future.GetCallback());
    ASSERT_TRUE(add_future.Wait());
    task->SetState(ActorTask::State::kReflecting);

    ASSERT_NO_FATAL_FAILURE(buffer_alert(contents1, u"same task dialog"));

    std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
        base::BindOnce([](ToolCallback callback) {
          std::move(callback).Run(MakeOkResult());
        }),
        base::DoNothing(), handle2);
    ActResultFuture act_future;
    task->Act(ToRequestList(std::move(request)), act_future.GetCallback());
    ExpectOkResult(act_future);

    EXPECT_NE(JavaScriptDialogInterceptor::FromWebContents(contents1), nullptr);
    auto dialogs =
        JavaScriptDialogInterceptor::TakeInterceptedDialogs(*contents1);
    ASSERT_EQ(dialogs.size(), 1u);
    EXPECT_EQ(dialogs[0].dialog_message, "same task dialog");

    actor_service->StopTask(task_id, ActorTask::StoppedReason::kTaskComplete);
    EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(contents1), nullptr);
  }

  // Case 3: Another task navigating while interrupted with user control or
  // pausing on a shared surface must not drop an active task's interceptor.
  {
    ScopedHeadlessActorSurface headless =
        actor_service->CreateHeadlessActorSurface();
    const ActorSurfaceHandle handle = headless.actor_surface_handle();
    content::WebContents* contents = handle.Get()->GetWebContents();
    ASSERT_TRUE(contents);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        contents, GURL("https://example.com/initial"));

    const TaskId task_a_id = actor_service->CreateTask(
        TaskSourceInfo(TaskSourceInfo::Client::kTest, "active_task"),
        GetNullEnterprisePolicyChecker());
    const TaskId task_b_id = actor_service->CreateTask(
        TaskSourceInfo(TaskSourceInfo::Client::kTest, "other_task"),
        GetNullEnterprisePolicyChecker());
    ActorTask* task_a = actor_service->GetTask(task_a_id);
    ActorTask* task_b = actor_service->GetTask(task_b_id);
    ASSERT_TRUE(task_a);
    ASSERT_TRUE(task_b);

    base::test::TestFuture<mojom::ActionResultPtr> add_a;
    task_a->AddActorSurface(handle, /*stop_task_on_detach=*/false,
                            add_a.GetCallback());
    ASSERT_TRUE(add_a.Wait());
    task_a->SetState(ActorTask::State::kReflecting);

    base::test::TestFuture<mojom::ActionResultPtr> add_b;
    task_b->AddActorSurface(handle, /*stop_task_on_detach=*/false,
                            add_b.GetCallback());
    ASSERT_TRUE(add_b.Wait());
    task_b->SetState(ActorTask::State::kReflecting);

    task_b->Interrupt(/*retain_user_control=*/true,
                      ActorTask::InterruptReason::kWaitingUserTakeOver);

    ASSERT_NO_FATAL_FAILURE(buffer_alert(contents, u"active task dialog"));

    // Navigation triggers PrimaryPageChanged() on task_b while task_b is not
    // under actor control.
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        contents, GURL("https://example.com/navigated"));
    // Stopping task_b while interrupted triggers RemoveActorSurface() on
    // task_b.
    actor_service->StopTask(task_b_id, ActorTask::StoppedReason::kTaskComplete);

    EXPECT_NE(JavaScriptDialogInterceptor::FromWebContents(contents), nullptr);
    auto dialogs =
        JavaScriptDialogInterceptor::TakeInterceptedDialogs(*contents);
    ASSERT_EQ(dialogs.size(), 1u);
    EXPECT_EQ(dialogs[0].dialog_message, "active task dialog");

    actor_service->StopTask(task_a_id, ActorTask::StoppedReason::kTaskComplete);
    EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(contents), nullptr);
  }
}

TEST_F(JavaScriptDialogInterceptorTest, RecordsSameOriginSubframeDialogs) {
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("same_origin");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/subframe.html"), subframe);
  ASSERT_TRUE(subframe);
  ASSERT_FALSE(subframe->IsInPrimaryMainFrame());
  ASSERT_FALSE(subframe->GetLastCommittedOrigin().opaque());

  bool suppressed = true;
  bool callback_called = false;
  bool accepted = false;
  interceptor()->RunJavaScriptDialog(
      web_contents(), subframe, content::JAVASCRIPT_DIALOG_TYPE_ALERT,
      u"same-origin alert", u"",
      base::BindLambdaForTesting([&](bool accept, const std::u16string&) {
        callback_called = true;
        accepted = accept;
      }),
      &suppressed);

  EXPECT_FALSE(suppressed);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(accepted);

  std::vector<InterceptedJavaScriptDialog> dialogs =
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents());
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "same-origin alert");
}

TEST_F(JavaScriptDialogInterceptorTest,
       AutoDismissesWithoutRecordingOpaqueAndCrossOriginSubframes) {
  // Opaque-origin subframe (models `<iframe sandbox="allow-scripts
  // allow-modals">` or `data:` iframes).
  content::RenderFrameHost* opaque_subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("opaque");
  opaque_subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("data:text/html,sandboxed"), opaque_subframe);
  ASSERT_TRUE(opaque_subframe);
  ASSERT_TRUE(opaque_subframe->GetLastCommittedOrigin().opaque());

  {
    bool suppressed = true;
    bool callback_called = false;
    bool accepted = false;
    interceptor()->RunJavaScriptDialog(
        web_contents(), opaque_subframe, content::JAVASCRIPT_DIALOG_TYPE_ALERT,
        u"untrusted alert", u"",
        base::BindLambdaForTesting([&](bool accept, const std::u16string&) {
          callback_called = true;
          accepted = accept;
        }),
        &suppressed);
    EXPECT_FALSE(suppressed);
    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(accepted);
  }
  {
    bool suppressed = true;
    bool callback_called = false;
    bool accepted = true;
    interceptor()->RunJavaScriptDialog(
        web_contents(), opaque_subframe,
        content::JAVASCRIPT_DIALOG_TYPE_CONFIRM, u"untrusted confirm", u"",
        base::BindLambdaForTesting([&](bool accept, const std::u16string&) {
          callback_called = true;
          accepted = accept;
        }),
        &suppressed);
    EXPECT_FALSE(suppressed);
    EXPECT_TRUE(callback_called);
    EXPECT_FALSE(accepted);
  }

  // Cross-origin subframe.
  content::RenderFrameHost* cross_origin_subframe =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("cross_origin");
  cross_origin_subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://other.test/subframe.html"), cross_origin_subframe);
  ASSERT_TRUE(cross_origin_subframe);
  {
    bool suppressed = true;
    bool callback_called = false;
    bool accepted = false;
    interceptor()->RunJavaScriptDialog(
        web_contents(), cross_origin_subframe,
        content::JAVASCRIPT_DIALOG_TYPE_ALERT, u"cross-origin alert", u"",
        base::BindLambdaForTesting([&](bool accept, const std::u16string&) {
          callback_called = true;
          accepted = accept;
        }),
        &suppressed);
    EXPECT_FALSE(suppressed);
    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(accepted);
  }

  // Null RenderFrameHost.
  {
    bool suppressed = true;
    bool callback_called = false;
    interceptor()->RunJavaScriptDialog(
        web_contents(), /*render_frame_host=*/nullptr,
        content::JAVASCRIPT_DIALOG_TYPE_ALERT, u"null rfh", u"",
        base::BindLambdaForTesting(
            [&](bool, const std::u16string&) { callback_called = true; }),
        &suppressed);
    EXPECT_FALSE(suppressed);
    EXPECT_TRUE(callback_called);
  }

  EXPECT_TRUE(
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents())
          .empty());
}

TEST_F(JavaScriptDialogInterceptorTest, RecordsOpaquePrimaryMainFrameDialog) {
  NavigateAndCommit(GURL("data:text/html,main"));
  ASSERT_TRUE(main_rfh()->GetLastCommittedOrigin().opaque());

  bool suppressed = true;
  bool callback_called = false;
  interceptor()->RunJavaScriptDialog(
      web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
      u"opaque main alert", u"",
      base::BindLambdaForTesting(
          [&](bool, const std::u16string&) { callback_called = true; }),
      &suppressed);

  EXPECT_FALSE(suppressed);
  EXPECT_TRUE(callback_called);

  std::vector<InterceptedJavaScriptDialog> dialogs =
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents());
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "opaque main alert");
}

TEST_F(JavaScriptDialogInterceptorTest,
       HandlesGuestWebContentsWithoutRecordingAndForwardsToOwnerFallback) {
  // Models a non-MPArch guest view (e.g. MimeHandlerViewGuest for the non-OOPIF
  // PDF viewer) that borrows its owner tab's JavaScriptDialogManager and passes
  // its own guest WebContents* into the manager calls.
  std::unique_ptr<content::WebContents> guest_contents =
      CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      guest_contents.get(), GURL("https://example.com/guest.pdf"));

  auto run_guest_dialog = [&](content::JavaScriptDialogType type,
                              const std::u16string& message,
                              bool expected_accept) {
    bool suppressed = true;
    bool callback_called = false;
    bool accepted = !expected_accept;
    interceptor()->RunJavaScriptDialog(
        guest_contents.get(), guest_contents->GetPrimaryMainFrame(), type,
        message, u"",
        base::BindLambdaForTesting([&](bool accept, const std::u16string&) {
          callback_called = true;
          accepted = accept;
        }),
        &suppressed);
    EXPECT_FALSE(suppressed);
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(accepted, expected_accept);
  };

  run_guest_dialog(content::JAVASCRIPT_DIALOG_TYPE_ALERT, u"guest alert",
                   /*expected_accept=*/true);
  run_guest_dialog(content::JAVASCRIPT_DIALOG_TYPE_CONFIRM, u"guest confirm",
                   /*expected_accept=*/false);
  run_guest_dialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT, u"guest prompt",
                   /*expected_accept=*/false);

  // Guest dialogs are auto-dismissed without being recorded on the owner tab.
  EXPECT_TRUE(
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents())
          .empty());
  EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(guest_contents.get()),
            nullptr);

  // Without a fallback manager, beforeunload auto-proceeds without crashing.
  {
    bool beforeunload_called = false;
    bool beforeunload_proceed = false;
    interceptor()->RunBeforeUnloadDialog(
        guest_contents.get(), guest_contents->GetPrimaryMainFrame(),
        /*is_reload=*/false,
        base::BindLambdaForTesting([&](bool proceed, const std::u16string&) {
          beforeunload_called = true;
          beforeunload_proceed = proceed;
        }));
    EXPECT_TRUE(beforeunload_called);
    EXPECT_TRUE(beforeunload_proceed);
  }

  // With a real TabModalDialogManager attached only to the owner WebContents
  // (not the guest), RunJavaScriptDialog, HandleJavaScriptDialog, and
  // CancelDialogs called with `guest_contents` must resolve the owner's
  // TabModalDialogManager rather than looking up `guest_contents`.
  {
    javascript_dialogs::TabModalDialogManager::CreateForWebContents(
        web_contents(), std::make_unique<FakeTabModalDialogManagerDelegate>());
    auto* owner_tab_modal =
        javascript_dialogs::TabModalDialogManager::FromWebContents(
            web_contents());
    ASSERT_NE(owner_tab_modal, nullptr);
    ASSERT_EQ(javascript_dialogs::TabModalDialogManager::FromWebContents(
                  guest_contents.get()),
              nullptr);

    auto queue_pending_owner_alert = [&]() {
      bool suppressed = true;
      owner_tab_modal->RunJavaScriptDialog(
          web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
          u"pending owner alert", u"",
          base::BindLambdaForTesting([](bool, const std::u16string&) {}),
          &suppressed);
      ASSERT_TRUE(owner_tab_modal->HasDialogOrPendingDialog());
    };

    ASSERT_NO_FATAL_FAILURE(queue_pending_owner_alert());
    run_guest_dialog(content::JAVASCRIPT_DIALOG_TYPE_ALERT,
                     u"guest clears owner pending",
                     /*expected_accept=*/true);
    EXPECT_FALSE(owner_tab_modal->HasDialogOrPendingDialog());

    ASSERT_NO_FATAL_FAILURE(queue_pending_owner_alert());
    const std::u16string empty_override;
    EXPECT_TRUE(interceptor()->HandleJavaScriptDialog(
        guest_contents.get(), /*accept=*/true, &empty_override));
    EXPECT_FALSE(owner_tab_modal->HasDialogOrPendingDialog());

    ASSERT_NO_FATAL_FAILURE(queue_pending_owner_alert());
    interceptor()->CancelDialogs(guest_contents.get(), /*reset_state=*/true);
    EXPECT_FALSE(owner_tab_modal->HasDialogOrPendingDialog());
  }

  // With a fallback manager attached to the owner WebContents, beforeunload,
  // handle, and cancel resolve the owner's fallback while forwarding the
  // original guest WebContents*.
  FakeJavaScriptDialogManager fallback;
  fallback.beforeunload_result_ = false;
  fallback.handle_return_value_ = true;
  interceptor()->SetFallbackDialogManagerForTesting(&fallback);
  base::ScopedClosureRunner reset_fallback(base::BindOnce(
      &JavaScriptDialogInterceptor::SetFallbackDialogManagerForTesting,
      base::Unretained(interceptor()), nullptr));

  bool beforeunload_called = false;
  bool beforeunload_proceed = true;
  interceptor()->RunBeforeUnloadDialog(
      guest_contents.get(), guest_contents->GetPrimaryMainFrame(),
      /*is_reload=*/true,
      base::BindLambdaForTesting([&](bool proceed, const std::u16string&) {
        beforeunload_called = true;
        beforeunload_proceed = proceed;
      }));
  EXPECT_TRUE(beforeunload_called);
  EXPECT_FALSE(beforeunload_proceed);
  EXPECT_EQ(fallback.beforeunload_calls_, 1);
  EXPECT_EQ(fallback.last_beforeunload_web_contents_, guest_contents.get());
  EXPECT_TRUE(fallback.last_beforeunload_is_reload_);

  const std::u16string override_text = u"guest_prompt";
  EXPECT_TRUE(interceptor()->HandleJavaScriptDialog(
      guest_contents.get(), /*accept=*/true, &override_text));
  EXPECT_EQ(fallback.handle_calls_, 1);
  EXPECT_EQ(fallback.last_handle_web_contents_, guest_contents.get());
  EXPECT_TRUE(fallback.last_handle_accept_);
  EXPECT_EQ(fallback.last_handle_prompt_override_, override_text);

  interceptor()->CancelDialogs(guest_contents.get(), /*reset_state=*/true);
  EXPECT_EQ(fallback.cancel_calls_, 1);
  EXPECT_EQ(fallback.last_cancel_web_contents_, guest_contents.get());
  EXPECT_TRUE(fallback.last_cancel_reset_state_);
}

TEST_F(JavaScriptDialogInterceptorTest,
       DoesNotActivateFieldTrialForNonActorOrHeadlessWebContents) {
  base::test::ScopedFeatureList trial_features;
  trial_features.InitAndEnableFeatureWithParameters(
      kActorInterceptJavaScriptDialogs, {});
  base::FieldTrial* trial =
      base::FeatureList::GetFieldTrial(kActorInterceptJavaScriptDialogs);
  ASSERT_TRUE(trial);
  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));

  // Non-actor WebContents must not query `kActorInterceptJavaScriptDialogs`
  // (and thus must not activate its Finch field trial on regular Chrome JS
  // dialogs).
  EXPECT_EQ(JavaScriptDialogInterceptor::MaybeGetForWebContents(web_contents()),
            nullptr);
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));

  // DismissExistingTabModalDialog on a WebContents without a
  // TabModalDialogManager (such as a headless surface) must not activate the
  // trial either.
  JavaScriptDialogInterceptor::DismissExistingTabModalDialog(*web_contents());
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));

  // Entering actor control on a headless surface also avoids activating the
  // trial until a dialog is actually requested on the actor-controlled surface.
  ActorKeyedService* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);
  ScopedHeadlessActorSurface headless =
      actor_service->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  content::WebContents* headless_contents = handle.Get()->GetWebContents();
  ASSERT_TRUE(headless_contents);

  const TaskId task_id = actor_service->CreateTask(
      TaskSourceInfo(TaskSourceInfo::Client::kTest, "trial_test"),
      GetNullEnterprisePolicyChecker());
  ActorTask* task = actor_service->GetTask(task_id);
  ASSERT_TRUE(task);

  base::test::TestFuture<mojom::ActionResultPtr> add_future;
  task->AddActorSurface(handle, /*stop_task_on_detach=*/false,
                        add_future.GetCallback());
  ASSERT_TRUE(add_future.Wait());
  task->SetState(ActorTask::State::kReflecting);
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));

  // Requesting the dialog manager for an actively controlled surface queries
  // the feature flag and activates the trial.
  EXPECT_NE(
      JavaScriptDialogInterceptor::MaybeGetForWebContents(headless_contents),
      nullptr);
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(trial->trial_name()));

  actor_service->StopTask(task_id, ActorTask::StoppedReason::kTaskComplete);
}

}  // namespace
}  // namespace actor
