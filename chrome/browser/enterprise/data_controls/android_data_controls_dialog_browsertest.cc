// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/enterprise/data_controls/android_data_controls_dialog_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/android/modal_dialog_wrapper.h"

namespace data_controls {

class AndroidDataControlsDialogUiTest
    : public AndroidBrowserTest,
      public testing::WithParamInterface<DataControlsDialog::Type> {
 public:
  AndroidDataControlsDialogUiTest() = default;
  ~AndroidDataControlsDialogUiTest() override = default;

  void SetUp() override { AndroidBrowserTest::SetUp(); }

  DataControlsDialog::Type type() const { return GetParam(); }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  ui::WindowAndroid* window_android() {
    return web_contents()->GetTopLevelNativeWindow();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AndroidDataControlsDialogUiTest,
    testing::Values(
        data_controls::DataControlsDialog::Type::kClipboardCopyWarn,
        data_controls::DataControlsDialog::Type::kClipboardCopyBlock,
        data_controls::DataControlsDialog::Type::kClipboardPasteWarn,
        data_controls::DataControlsDialog::Type::kClipboardPasteBlock));

IN_PROC_BROWSER_TEST_P(AndroidDataControlsDialogUiTest, SmokeTest) {
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());

  AndroidDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      web_contents(), type());
  EXPECT_NE(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
}

}  // namespace data_controls
