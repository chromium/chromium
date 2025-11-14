// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace actor::ui {
namespace {

using actor::mojom::ActionResultPtr;
using base::test::TestFuture;

class ActorUiHandoffButtonControllerPixelTest : public DialogBrowserTest {
 public:
  ActorUiHandoffButtonControllerPixelTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicActor, {}},
                              {features::kGlicActorUi,
                               {{features::kGlicActorUiHandoffButtonName,
                                 "true"}}}},
        /*disabled_features=*/{});
  }
  ~ActorUiHandoffButtonControllerPixelTest() override = default;

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    GetActorKeyedService()->GetPolicyChecker().SetActOnWebForTesting(true);
  }

  ActorKeyedService* GetActorKeyedService() {
    return ActorKeyedService::Get(browser()->profile());
  }

  std::string GetNonDialogName() override { return "HandoffButtonWidget"; }

  void ShowUi(const std::string& name) override {
    task_id_ = GetActorKeyedService()->CreateTask();
    TestFuture<actor::mojom::ActionResultPtr> future;
    GetActorKeyedService()->GetTask(task_id_)->AddTab(
        browser()->GetActiveTabInterface()->GetHandle(), future.GetCallback());
    ExpectOkResult(future);
    actor::PerformActionsFuture result_future;
    std::vector<std::unique_ptr<actor::ToolRequest>> actions;
    actions.push_back(actor::MakeWaitRequest());
    GetActorKeyedService()->PerformActions(task_id_, std::move(actions),
                                           actor::ActorTaskMetadata(),
                                           result_future.GetCallback());
    ExpectOkResult(result_future);

    auto* tab_controller = ActorUiTabController::From(
        browser()->tab_strip_model()->GetActiveTab());
    ASSERT_TRUE(tab_controller);

    UiTabState ui_tab_state;
    ui_tab_state.handoff_button = {.is_active = true, .controller = ownership_};
    base::test::TestFuture<bool> tab_state_change_future;
    tab_controller->OnUiTabStateChange(ui_tab_state,
                                       tab_state_change_future.GetCallback());
    EXPECT_TRUE(tab_state_change_future.Get());
  }

 protected:
  HandoffButtonState::ControlOwnership ownership_ =
      HandoffButtonState::ControlOwnership::kActor;

 private:
  base::test::ScopedFeatureList feature_list_;
  actor::TaskId task_id_;
};

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerPixelTest,
                       InvokeUi_TakeOverTask) {
  ShowAndVerifyUi();
}

class ActorUiHandoffButtonHiddenPixelTest
    : public ActorUiHandoffButtonControllerPixelTest {
 public:
  ActorUiHandoffButtonHiddenPixelTest() {
    override_feature_list_.InitAndDisableFeature(
        features::kGlicHandoffButtonHiddenClientControl);
  }

 private:
  base::test::ScopedFeatureList override_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonHiddenPixelTest,
                       InvokeUi_TakeOverTask) {
  ownership_ = HandoffButtonState::ControlOwnership::kClient;
  ShowAndVerifyUi();
}
}  // namespace
}  // namespace actor::ui
