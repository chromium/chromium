// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_page_action_controller.h"

#include <string>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/webui/settings/on_device_ai_settings_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace {

using optimization_guide::OnDeviceBaseModel;
using optimization_guide::OnDeviceModelControl;

class AIPageActionControllerBrowserTest : public InProcessBrowserTest {
 public:
  AIPageActionControllerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {kOnDeviceAiPageAction, ::features::kOnDeviceModelAdvancedSettings},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    WaitForInitialWebUIToolbar(browser());
  }

  AIPageActionController& controller() {
    return *AIPageActionController::From(browser());
  }

  page_actions::PageActionTestAccessor chip() {
    return page_actions::PageActionTestAccessor(browser(), kActionOnDeviceAi);
  }

  views::DialogDelegate* GetBubbleDialog() {
    views::View* const view = BrowserElementsViews::From(browser())->GetView(
        kOnDeviceAiBubbleElementId);
    return view ? view->GetWidget()->widget_delegate()->AsDialogDelegate()
                : nullptr;
  }

  bool IsExpandedChipShowing() {
    chip().FinishAnimation();
    if (!chip().ShouldShowSuggestionChip()) {
      return false;
    }
    return base::test::RunUntil([&]() { return chip().IsChipShowing(); });
  }

  views::DialogDelegate* ClickChip() {
    if (!base::test::RunUntil(
            [&]() { return chip().GetElement() != nullptr; })) {
      ADD_FAILURE() << "Timed out waiting for page action element.";
      return nullptr;
    }
    views::DialogDelegate* dialog = nullptr;
    base::RunLoop run_loop;
    const ui::ElementTracker::Subscription subscription =
        ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
            kOnDeviceAiBubbleElementId,
            BrowserElementsViews::From(browser())->GetContext(),
            base::BindLambdaForTesting([&](ui::TrackedElement* element) {
              if (!element) {
                return;
              }
              dialog = GetBubbleDialog();
              CHECK(dialog);
              // Clicking in WebUI mode opens the bubble asynchronously; disable
              // close-on-deactivate during `Widget::Show()` so concurrent
              // browser tests on Xvfb cannot steal focus and close it early.
              auto* const bubble = dialog->AsBubbleDialogDelegate();
              CHECK(bubble);
              bubble->set_close_on_deactivate(false);
              run_loop.Quit();
            }));
    chip().Click(page_actions::PageActionTrigger::kKeyboard);
    run_loop.Run();
    return dialog;
  }

  void CloseBubble(bool accept = true) {
    views::DialogDelegate* const dialog = GetBubbleDialog();
    ASSERT_TRUE(dialog);
    views::test::WidgetDestroyedWaiter waiter(dialog->GetWidget());
    if (accept) {
      dialog->AcceptDialog();
    } else {
      dialog->CancelDialog();
    }
    waiter.Wait();
  }

  bool WaitForChipCollapse() {
    chip().FinishAnimation();
    return base::test::RunUntil([&]() {
      if (chip().IsAnimating()) {
        chip().FinishAnimation();
      }
      return !chip().ShouldShowSuggestionChip() && !chip().IsChipShowing() &&
             chip().GetVisible() && !chip().IsLabelVisible();
    });
  }

  void SeedNanoV3State(OnDeviceModelControl::ModelInstallState state) {
    ScopedDictPrefUpdate(g_browser_process->local_state(),
                         optimization_guide::model_execution::prefs::
                             localstate::kManifestAssetLedger)
        ->Set("nano_key", base::DictValue()
                              .Set("asset_id", "nano_v3_gpu_component")
                              .Set("install_state", static_cast<int>(state)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AIPageActionControllerBrowserTest, AddedChipAndBubble) {
  controller().OnModelInstalled(OnDeviceBaseModel::kGemma4);
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4,
                                     browser()->GetActiveTabInterface());

  const std::u16string ready_title =
      l10n_util::GetStringUTF16(IDS_ON_DEVICE_AI_READY_TITLE);
  EXPECT_EQ(ready_title, chip().GetText());
  EXPECT_TRUE(IsExpandedChipShowing());
  EXPECT_FALSE(GetBubbleDialog());

  views::DialogDelegate* const dialog = ClickChip();
  ASSERT_TRUE(dialog);
  EXPECT_TRUE(chip().ShouldShowSuggestionChip());
  EXPECT_TRUE(chip().IsChipShowing());
  EXPECT_EQ(ready_title, dialog->GetWindowTitle());
  EXPECT_EQ(2u, dialog->GetContentsView()->children().size());

  CloseBubble();
  EXPECT_TRUE(WaitForChipCollapse());

  // Clicking the collapsed icon reopens the bubble.
  ASSERT_TRUE(ClickChip());
  CloseBubble();
  EXPECT_TRUE(WaitForChipCollapse());
}

IN_PROC_BROWSER_TEST_F(AIPageActionControllerBrowserTest,
                       UpdatedChipAndBubble) {
  SeedNanoV3State(
      OnDeviceModelControl::ModelInstallState::kUninstalledObsolete);
  controller().OnModelInstalled(OnDeviceBaseModel::kGemma4);
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4,
                                     browser()->GetActiveTabInterface());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ON_DEVICE_AI_UPDATED_CHIP_LABEL),
            chip().GetText());
  EXPECT_TRUE(IsExpandedChipShowing());
  EXPECT_FALSE(GetBubbleDialog());

  views::DialogDelegate* const dialog = ClickChip();
  ASSERT_TRUE(dialog);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ON_DEVICE_AI_UPDATED_BUBBLE_TITLE),
            dialog->GetWindowTitle());
  EXPECT_EQ(1u, dialog->GetContentsView()->children().size());

  // Canceling the dialog clicks Settings and opens the on-device AI subpage.
  CloseBubble(/*accept=*/false);
  EXPECT_TRUE(WaitForChipCollapse());
  EXPECT_EQ(
      GURL("chrome://settings/ai/onDeviceAi"),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(AIPageActionControllerBrowserTest,
                       NonObsoleteUninstallShowsAdded) {
  SeedNanoV3State(OnDeviceModelControl::ModelInstallState::kUninstalled);
  controller().OnModelInstalled(OnDeviceBaseModel::kGemma4);
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4,
                                     browser()->GetActiveTabInterface());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ON_DEVICE_AI_READY_TITLE),
            chip().GetText());
  EXPECT_TRUE(IsExpandedChipShowing());
}

IN_PROC_BROWSER_TEST_F(AIPageActionControllerBrowserTest,
                       SuppressedForIneligibleCases) {
  tabs::TabInterface* const tab = browser()->GetActiveTabInterface();

  // Non-Gemma 4 models, pre-existing installs, and uninstalled pending models
  // do not show the chip.
  controller().OnModelInstalled(OnDeviceBaseModel::kProofreaderSmallExpert);
  controller().OnModelSessionCreated(OnDeviceBaseModel::kProofreaderSmallExpert,
                                     tab);
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4, tab);
  controller().OnModelInstalled(OnDeviceBaseModel::kGemma4);
  controller().OnModelUninstalled(OnDeviceBaseModel::kGemma4);
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4, tab);
  EXPECT_FALSE(chip().GetVisible());

  BrowserWindowInterface* const popup = CreateBrowserForPopup(GetProfile());
  EXPECT_FALSE(AIPageActionController::From(popup));
  CloseBrowserSynchronously(popup);

  // Null-tab sessions neither show the chip nor consume pending state.
  controller().OnModelInstalled(OnDeviceBaseModel::kGemma4);
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4, nullptr);
  EXPECT_FALSE(chip().GetVisible());
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4, tab);
  EXPECT_TRUE(IsExpandedChipShowing());
}

IN_PROC_BROWSER_TEST_F(AIPageActionControllerBrowserTest,
                       BackgroundTabAndShownOnce) {
  tabs::TabInterface* const tab0 = browser()->GetActiveTabInterface();
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));

  controller().OnModelInstalled(OnDeviceBaseModel::kGemma4);
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4, tab0);
  EXPECT_FALSE(chip().GetVisible());

  browser()->GetTabStripModel()->ActivateTabAt(0);
  EXPECT_TRUE(IsExpandedChipShowing());
  ASSERT_TRUE(ClickChip());
  CloseBubble();
  EXPECT_TRUE(WaitForChipCollapse());

  // Subsequent sessions and reinstalls do not re-expand the chip once shown.
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4, tab0);
  controller().OnModelUninstalled(OnDeviceBaseModel::kGemma4);
  controller().OnModelInstalled(OnDeviceBaseModel::kGemma4);
  controller().OnModelSessionCreated(OnDeviceBaseModel::kGemma4, tab0);
  chip().FinishAnimation();
  EXPECT_TRUE(chip().IsIconVisible());
  EXPECT_FALSE(chip().ShouldShowSuggestionChip());
  EXPECT_FALSE(chip().IsChipShowing());
  EXPECT_FALSE(chip().IsLabelVisible());
}

}  // namespace
