// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_path_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/frame_view.h"
#include "ui/views/window/non_client_view.h"

namespace {

constexpr gfx::Size kRequestedContentsSize(620, 420);

const base::FilePath::CharType kDocumentPipPage[] = FILE_PATH_LITERAL(
    "media/picture-in-picture/documen_picture_in_picture_pixel_test.html");

// Verifies that the standalone and Browser-backed Document PiP windows render
// JavaScript dialogs identically. Both backends use the same Skia Gold
// baseline, so a pixel difference between them fails the test.
class DocumentPipDialogPixelTest : public UiBrowserTest,
                                   public testing::WithParamInterface<bool> {
 public:
  DocumentPipDialogPixelTest() {
    feature_list_.InitWithFeatureStates(
        {{blink::features::kDocumentPictureInPictureAPI, true},
         {features::kDocumentPipStandaloneWindow, standalone_enabled()},
         // TODO(crbug.com/452061489): Fix tests that fail when the WebUI
         // Omnibox is enabled and then remove the two omnibox features.
         {omnibox::internal::kWebUIOmniboxPopup, false},
         {omnibox::internal::kWebUIOmniboxAimPopup, false}});
    SetPixelMatchAlgorithm(
        std::make_unique<ui::test::ExactSkiaGoldMatchingAlgorithm>());
  }

  void TearDownOnMainThread() override {
    dialog_widget_ = nullptr;
    pip_widget_ = nullptr;
    pip_web_contents_ = nullptr;
    UiBrowserTest::TearDownOnMainThread();
  }

  bool standalone_enabled() const { return GetParam(); }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    OpenPictureInPicture();
    ShowDialog(GetDialogScript(name));
  }

  bool VerifyUi() override {
    if (!pip_widget_ || !dialog_widget_) {
      return false;
    }
    ScreenshotOptions options;
    // Child-modal dialogs are siblings of the PiP widget in the root window.
    options.capture_root_window = true;
    // Both backends share one baseline, so the name omits the backend.
    return VerifyPixelUi(pip_widget_->GetRootView(), options,
                         "DocumentPipDialogPixelTest",
                         NameFromTestCase()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    if (dialog_widget_) {
      views::test::WidgetDestroyedWaiter(dialog_widget_).Wait();
    }
  }

  void DismissUi() override {
    if (!dialog_widget_) {
      return;
    }
    views::DialogDelegate* delegate =
        dialog_widget_->widget_delegate()->AsDialogDelegate();
    views::test::WidgetDestroyedWaiter destroyed(dialog_widget_);
    dialog_widget_ = nullptr;
    delegate->AcceptDialog();
    destroyed.Wait();
  }

 private:
  static const char* GetDialogScript(const std::string& name) {
    if (name == "alert") {
      return "alert('Hello from Document PiP');";
    }
    if (name == "confirm") {
      return "confirm('Hello from Document PiP');";
    }
    CHECK_EQ(name, "prompt");
    return "prompt('Hello from Document PiP', 'input');";
  }

  void OpenPictureInPicture() {
    // A file:// page keeps the dialog's origin text independent of the port.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), chrome_test_utils::GetTestUrl(
                       base::FilePath(base::FilePath::kCurrentDirectory),
                       base::FilePath(kDocumentPipPage))));
    content::WebContents* const opener =
        browser()->GetTabStripModel()->GetActiveWebContents();
    ASSERT_EQ(true, content::EvalJs(
                        opener,
                        content::JsReplace(
                            "createDocumentPipWindow({width: $1, height: $2})",
                            kRequestedContentsSize.width(),
                            kRequestedContentsSize.height())));
    auto* controller = content::PictureInPictureWindowController::
        GetOrCreateDocumentPictureInPictureController(opener);
    pip_web_contents_ = controller->GetChildWebContents();
    ASSERT_TRUE(pip_web_contents_);
    pip_widget_ = views::Widget::GetWidgetForNativeWindow(
        pip_web_contents_->GetTopLevelNativeWindow());
    ASSERT_TRUE(pip_widget_);

    // Make both backends' root views the same size, and place the windows at
    // the same position, so that the captured surfaces are comparable.
    const gfx::Size expected_root_size =
        pip_widget_->non_client_view()
            ->frame_view()
            ->GetWindowBoundsForClientBounds(gfx::Rect(kRequestedContentsSize))
            .size();
    const gfx::Rect work_area =
        display::Screen::Get()->GetPrimaryDisplay().work_area();
    gfx::Rect bounds = pip_widget_->GetWindowBoundsInScreen();
    const gfx::Size native_margins =
        bounds.size() - pip_widget_->GetRootView()->size();
    bounds.set_size(expected_root_size + native_margins);
    bounds.set_origin(work_area.CenterPoint() -
                      gfx::Vector2d(bounds.width() / 2, bounds.height() / 2));
    pip_widget_->SetBounds(bounds);
    pip_widget_->Activate();
    views::test::WaitForWidgetActive(pip_widget_, true);
    pip_web_contents_->Focus();
    content::WaitForCopyableViewInWebContents(pip_web_contents_);
    ASSERT_EQ(expected_root_size, pip_widget_->GetRootView()->size());
  }

  void ShowDialog(const char* script) {
    auto* manager = javascript_dialogs::TabModalDialogManager::FromWebContents(
        pip_web_contents_);
    ASSERT_TRUE(manager);
    base::test::TestFuture<void> shown;
    manager->SetDialogShownCallbackForTesting(shown.GetCallback());
    content::ExecuteScriptAsync(pip_web_contents_, script);
    ASSERT_TRUE(shown.Wait(base::RunLoop::Type::kNestableTasksAllowed));
    ASSERT_TRUE(manager->IsShowingDialogForTesting());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      for (views::Widget* owned :
           views::Widget::GetAllOwnedWidgets(pip_widget_->GetNativeView())) {
        if (owned->IsVisible() && owned->widget_delegate() &&
            owned->widget_delegate()->AsDialogDelegate()) {
          dialog_widget_ = owned;
          return true;
        }
      }
      return false;
    }));
  }

  test::ScopedPrewarmFeatureList prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  base::test::ScopedFeatureList feature_list_;
  gfx::ScopedAnimationDurationScaleMode animation_duration_{
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION};
  raw_ptr<content::WebContents> pip_web_contents_ = nullptr;
  raw_ptr<views::Widget> pip_widget_ = nullptr;
  raw_ptr<views::Widget> dialog_widget_ = nullptr;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         DocumentPipDialogPixelTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Standalone" : "BrowserBacked";
                         });

IN_PROC_BROWSER_TEST_P(DocumentPipDialogPixelTest, InvokeUi_alert) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(DocumentPipDialogPixelTest, InvokeUi_confirm) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(DocumentPipDialogPixelTest, InvokeUi_prompt) {
  ShowAndVerifyUi();
}
