// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/data_controls_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

namespace data_controls {

class DataControlsDialogUiTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<DataControlsDialog::Type> {
 public:
  DataControlsDialogUiTest() = default;
  ~DataControlsDialogUiTest() override = default;

  DataControlsDialog::Type type() const { return GetParam(); }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    DataControlsDialog::Show(
        browser()->tab_strip_model()->GetActiveWebContents(), type());
  }
};

IN_PROC_BROWSER_TEST_P(DataControlsDialogUiTest, DefaultUi) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DataControlsDialogUiTest,
    testing::Values(DataControlsDialog::Type::kClipboardPasteBlock,
                    DataControlsDialog::Type::kClipboardCopyBlock));

}  // namespace data_controls
