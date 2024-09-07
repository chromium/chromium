// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/dialog_button.mojom.h"

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
    DesktopDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
        browser()->tab_strip_model()->GetActiveWebContents(), type());
  }
};

class DesktopDataControlsDialogTest : public InProcessBrowserTest,
                               public DesktopDataControlsDialog::TestObserver {
 public:
  void OnConstructed(DesktopDataControlsDialog* dialog) override {
    ++constructor_called_count_;

    ASSERT_TRUE(dialog);
    ASSERT_EQ(dialog->GetDefaultDialogButton(),
              static_cast<int>(ui::mojom::DialogButton::kOk));
    ASSERT_FALSE(base::Contains(dialog_close_loops_, dialog));
    ASSERT_FALSE(base::Contains(dialog_close_callbacks_, dialog));

    dialog_close_loops_[dialog] = std::make_unique<base::RunLoop>();
    dialog_close_callbacks_[dialog] =
        dialog_close_loops_[dialog]->QuitClosure();
  }

  void OnDestructed(DesktopDataControlsDialog* dialog) override {
    ASSERT_TRUE(dialog);
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
          base::BindOnce(&DesktopDataControlsDialog::AcceptDialog,
                         base::Unretained(dialog_and_loop.first)));
    }

    for (auto& dialog_and_loop : dialog_close_loops_) {
      dialog_and_loop.second->Run();
    }
  }

 protected:
  size_t constructor_called_count_ = 0;

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

}  // namespace data_controls
