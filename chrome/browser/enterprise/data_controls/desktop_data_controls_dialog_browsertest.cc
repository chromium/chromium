// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_factory.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"

namespace data_controls {

namespace {

class DesktopDataControlsDialogUiTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<DataControlsDialog::Type> {
 public:
  DesktopDataControlsDialogUiTest() = default;
  ~DesktopDataControlsDialogUiTest() override = default;

  DataControlsDialog::Type type() const { return GetParam(); }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    helper_ = std::make_unique<DesktopDataControlsDialogTestHelper>(type());
    DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
        browser()->tab_strip_model()->GetActiveWebContents(), type());
  }

  void DismissUi() override {
    helper_->CloseDialogWithoutBypass();
    helper_->WaitForDialogToClose();
  }

 private:
  std::unique_ptr<DesktopDataControlsDialogTestHelper> helper_;
};

class DesktopDataControlsDialogTest : public InProcessBrowserTest,
                               public DesktopDataControlsDialog::TestObserver {
 public:
  void OnConstructed(DesktopDataControlsDialog* dialog,
                     views::DialogDelegate* delegate) override {
    ++constructor_called_count_;

    ASSERT_TRUE(dialog);
    ASSERT_TRUE(delegate);

    ASSERT_EQ(delegate->GetDefaultDialogButton(),
              static_cast<int>(ui::mojom::DialogButton::kOk));

    ASSERT_FALSE(base::Contains(delegates_, dialog));
    ASSERT_FALSE(base::Contains(dialog_close_loops_, dialog));
    ASSERT_FALSE(base::Contains(dialog_close_callbacks_, dialog));

    delegates_[dialog] = delegate;
    dialog_close_loops_[dialog] = std::make_unique<base::RunLoop>();
    dialog_close_callbacks_[dialog] =
        dialog_close_loops_[dialog]->QuitClosure();
  }

  void OnDestructed(DesktopDataControlsDialog* dialog) override {
    ASSERT_TRUE(dialog);
    ASSERT_TRUE(base::Contains(delegates_, dialog));
    ASSERT_TRUE(base::Contains(dialog_close_loops_, dialog));
    ASSERT_TRUE(base::Contains(dialog_close_callbacks_, dialog));

    std::move(dialog_close_callbacks_[dialog]).Run();
  }

  void CloseDialogsAndWait() {
    ASSERT_EQ(dialog_close_loops_.size(), constructor_called_count_);
    ASSERT_EQ(dialog_close_callbacks_.size(), constructor_called_count_);

    for (auto& dialog_and_loop : dialog_close_loops_) {
      // Some platforms crash if the dialog has been cancelled before fully
      // launching modally, so to avoid that issue cancelling the dialog is done
      // asynchronously.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&views::DialogDelegate::AcceptDialog,
                         base::Unretained(delegates_[dialog_and_loop.first])));
    }

    for (auto& dialog_and_loop : dialog_close_loops_) {
      dialog_and_loop.second->Run();
    }
  }

 protected:
  size_t constructor_called_count_ = 0;
  // ensure NoWebContentsModalDialogManager test's bool observer outlives its
  // objects destruction.
  bool test_dialog_destructor_called_unused_ = false;

  std::map<DesktopDataControlsDialog*, views::DialogDelegate*> delegates_;
  std::map<DesktopDataControlsDialog*, base::OnceClosure>
      dialog_close_callbacks_;
  std::map<DesktopDataControlsDialog*, std::unique_ptr<base::RunLoop>>
      dialog_close_loops_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(DesktopDataControlsDialogUiTest, DefaultUi) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DesktopDataControlsDialogUiTest,
    testing::Values(DataControlsDialog::Type::kClipboardPasteBlock,
                    DataControlsDialog::Type::kClipboardCopyBlock,
                    DataControlsDialog::Type::kClipboardPasteWarn,
                    DataControlsDialog::Type::kClipboardCopyWarn));

IN_PROC_BROWSER_TEST_F(DesktopDataControlsDialogTest, ShowDialogMultipleTimes) {
  // Only 1 dialog should be shown for the same WebContents-Type pair.
  for (int i = 0; i < 100; ++i) {
    DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
        browser()->tab_strip_model()->GetActiveWebContents(),
        DataControlsDialog::Type::kClipboardCopyBlock);
  }

  ASSERT_EQ(constructor_called_count_, 1u);
  CloseDialogsAndWait();
}

IN_PROC_BROWSER_TEST_F(DesktopDataControlsDialogTest,
                       ShowDialogMultipleTimes_DifferentTypes) {
  // Distinct dialogs should be created for different types.
  for (int i = 0; i < 100; ++i) {
    DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
        browser()->tab_strip_model()->GetActiveWebContents(),
        DataControlsDialog::Type::kClipboardCopyBlock);
    DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
        browser()->tab_strip_model()->GetActiveWebContents(),
        DataControlsDialog::Type::kClipboardPasteBlock);
    DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
        browser()->tab_strip_model()->GetActiveWebContents(),
        DataControlsDialog::Type::kClipboardPasteWarn);
    DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
        browser()->tab_strip_model()->GetActiveWebContents(),
        DataControlsDialog::Type::kClipboardCopyWarn);
  }

  ASSERT_EQ(constructor_called_count_, 4u);
  CloseDialogsAndWait();
}

IN_PROC_BROWSER_TEST_F(DesktopDataControlsDialogTest,
                       ShowDialogMultipleTimes_DifferentWebContents) {
  // Distinct dialogs should be created for different WebContents.
  DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      browser()->tab_strip_model()->GetActiveWebContents(),
      DataControlsDialog::Type::kClipboardCopyBlock);
  chrome::NewTab(browser());
  DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      browser()->tab_strip_model()->GetActiveWebContents(),
      DataControlsDialog::Type::kClipboardCopyBlock);

  ASSERT_EQ(constructor_called_count_, 2u);
  CloseDialogsAndWait();
}

IN_PROC_BROWSER_TEST_F(DesktopDataControlsDialogTest,
                       NoWebContentsModalDialogManager) {
  ui::test::TestWebDialogDelegate* delegate =
      new ui::test::TestWebDialogDelegate(GURL(url::kAboutBlankURL));
  delegate->SetDeleteOnClosedAndObserve(&test_dialog_destructor_called_unused_);

  auto view = std::make_unique<views::WebDialogView>(
      browser()->profile(), delegate,
      std::make_unique<ChromeWebContentsHandler>());
  auto view_ptr = view.get();
  gfx::NativeView parent_view =
      browser()->tab_strip_model()->GetActiveWebContents()->GetNativeView();
  auto* widget =
      views::Widget::CreateWindowWithParent(std::move(view), parent_view);
  widget->Show();
  ASSERT_TRUE(content::WaitForLoadStop(view_ptr->web_contents()));

  base::test::TestFuture<bool> was_bypassed;
  DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      view_ptr->web_contents(), DataControlsDialog::Type::kClipboardCopyBlock,
      was_bypassed.GetCallback());

  for (auto& dialog_and_loop : dialog_close_loops_) {
    dialog_and_loop.second->Run();
  }
  ASSERT_TRUE(was_bypassed.IsReady());
  ASSERT_FALSE(was_bypassed.Get());
}

}  // namespace data_controls
