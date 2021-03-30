// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/quick_answers_menu_observer.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/quick_answers/controller/quick_answers_controller.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

// A test class for Quick Answers. This test should be a browser test because it
// accesses resources.
class QuickAnswersMenuObserverTest : public InProcessBrowserTest {
 public:
  QuickAnswersMenuObserverTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kQuickAnswers);
  }

  QuickAnswersMenuObserverTest(const QuickAnswersMenuObserverTest&) = delete;
  QuickAnswersMenuObserverTest& operator=(const QuickAnswersMenuObserverTest&) =
      delete;

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override {
    Reset(false);
    observer_->OnEligibilityChanged(true);
  }

  void TearDownOnMainThread() override {
    menu_.reset();
    observer_.reset();
  }

  void Reset(bool incognito) {
    observer_.reset();
    menu_ = std::make_unique<MockRenderViewContextMenu>(incognito);
    observer_ = std::make_unique<QuickAnswersMenuObserver>(menu_.get());
    menu_->SetObserver(observer_.get());
  }

  void ShowMenu(const content::ContextMenuParams& params) {
    auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
    menu()->set_web_contents(web_contents);
    content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_TRUE(ExecuteScript(main_frame, "window.focus();"));

    observer_->OnContextMenuShown(params, gfx::Rect());
  }

  MockRenderViewContextMenu* menu() { return menu_.get(); }
  ash::QuickAnswersController* controller() {
    return ash::QuickAnswersController::Get();
  }
  QuickAnswersMenuObserver* observer() { return observer_.get(); }

 protected:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<QuickAnswersMenuObserver> observer_;
  std::unique_ptr<MockRenderViewContextMenu> menu_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, FeatureIneligible) {
  observer_->OnEligibilityChanged(false);

  content::ContextMenuParams params;
  params.selection_text = u"test";

  ShowMenu(params);

  // Quick Answers UI should stay hidden since the feature is not eligible.
  ASSERT_EQ(ash::QuickAnswersVisibility::kClosed,
            controller()->GetVisibilityForTesting());
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, PasswordField) {
  observer_->OnEligibilityChanged(true);

  content::ContextMenuParams params;
  params.input_field_type =
      blink::mojom::ContextMenuDataInputFieldType::kPassword;
  params.selection_text = u"test";

  ShowMenu(params);

  // Quick Answers UI should stay hidden since the input field is password
  // field.
  ASSERT_EQ(ash::QuickAnswersVisibility::kClosed,
            controller()->GetVisibilityForTesting());
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, NoSelectedText) {
  observer_->OnEligibilityChanged(true);

  content::ContextMenuParams params;
  ShowMenu(params);

  // Quick Answers UI should stay hidden since no text is selected.
  ASSERT_EQ(ash::QuickAnswersVisibility::kClosed,
            controller()->GetVisibilityForTesting());
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, QuickAnswersPending) {
  observer_->OnEligibilityChanged(true);

  content::ContextMenuParams params;
  params.selection_text = u"test";
  ShowMenu(params);

  // Quick Answers UI should be pending.
  ASSERT_EQ(ash::QuickAnswersVisibility::kPending,
            controller()->GetVisibilityForTesting());
}
