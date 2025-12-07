// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/enterprise/data_controls/android_data_controls_dialog_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/android/modal_dialog_wrapper.h"

namespace data_controls {

class AndroidDataControlsDialogUiTest : public AndroidBrowserTest {
 public:
  AndroidDataControlsDialogUiTest() = default;
  ~AndroidDataControlsDialogUiTest() override = default;

  void SetUp() override { AndroidBrowserTest::SetUp(); }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  ui::WindowAndroid* window_android() {
    return web_contents()->GetTopLevelNativeWindow();
  }
};

IN_PROC_BROWSER_TEST_F(AndroidDataControlsDialogUiTest,
                       SmokeTest_ClipboardCopyWarn) {
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
  AndroidDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);
  EXPECT_NE(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidDataControlsDialogUiTest,
                       SmokeTest_ClipboardPasteWarn) {
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
  AndroidDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);
  EXPECT_NE(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidDataControlsDialogUiTest,
                       SmokeTest_ClipboardShareWarn) {
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
  AndroidDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardShareWarn);
  EXPECT_NE(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidDataControlsDialogUiTest,
                       SmokeTest_ClipboardActionWarn) {
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
  AndroidDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardActionWarn);
  EXPECT_NE(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidDataControlsDialogUiTest,
                       SmokeTest_ClipboardCopyBlock) {
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
  AndroidDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardCopyBlock);
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidDataControlsDialogUiTest,
                       SmokeTest_ClipboardPasteBlock) {
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
  AndroidDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidDataControlsDialogUiTest,
                       SmokeTest_ClipboardShareBlock) {
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
  AndroidDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardShareBlock);
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidDataControlsDialogUiTest,
                       SmokeTest_ClipboardActionBlock) {
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
  AndroidDataControlsDialogFactory::GetInstance()->ShowDialogIfNeeded(
      web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardActionBlock);
  EXPECT_EQ(nullptr, ui::ModalDialogWrapper::GetDialogForTesting());
}

}  // namespace data_controls
