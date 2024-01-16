// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "chrome/browser/enterprise/data_controls/test_utils.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/data_controls/data_controls_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/data_controls/features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget_delegate.h"

namespace data_protection {

class DataControlsClipboardUtilsBrowserTest
    : public InProcessBrowserTest,
      public data_controls::DataControlsDialog::TestObserver {
 public:
  DataControlsClipboardUtilsBrowserTest() {
    scoped_features_.InitAndEnableFeature(
        data_controls::kEnableDesktopDataControls);
  }
  ~DataControlsClipboardUtilsBrowserTest() override = default;

  void OnConstructed(data_controls::DataControlsDialog* dialog) override {
    constructed_dialog_ = dialog;

    dialog_close_loop_ = std::make_unique<base::RunLoop>();
    dialog_close_callback_ = dialog_close_loop_->QuitClosure();
  }

  void OnWidgetInitialized(data_controls::DataControlsDialog* dialog) override {
    ASSERT_TRUE(dialog);
    ASSERT_EQ(dialog, constructed_dialog_);

    // Some platforms crash if the dialog has been cancelled before fully        
    // launching modally, so to avoid that issue cancelling the dialog is done   
    // asynchronously.                                                           
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(                 
        FROM_HERE,                                                               
        base::BindOnce(&data_controls::DataControlsDialog::CancelDialog,         
                       base::Unretained(dialog)));
  }

  void OnDestructed(data_controls::DataControlsDialog* dialog) override {
    ASSERT_TRUE(dialog);
    ASSERT_EQ(dialog, constructed_dialog_);
    constructed_dialog_ = nullptr;

    std::move(dialog_close_callback_).Run();
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void WaitForDialogToClose() {
    ASSERT_TRUE(dialog_close_loop_);
    dialog_close_loop_->Run();
  }

 protected:
  base::test::ScopedFeatureList scoped_features_;
  std::unique_ptr<base::RunLoop> dialog_close_loop_;
  base::OnceClosure dialog_close_callback_;

  raw_ptr<data_controls::DataControlsDialog> constructed_dialog_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest, PasteAllowed) {
  base::test::TestFuture<absl::optional<content::ClipboardPasteData>> future;
  enterprise_data_protection::PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(absl::nullopt),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234},
      content::ClipboardPasteData("text", "image", {}), future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, "text");
  EXPECT_EQ(paste_data->image, "image");

  EXPECT_FALSE(constructed_dialog_);
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       PasteBlockedByDataControls) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  base::test::TestFuture<absl::optional<content::ClipboardPasteData>> future;
  enterprise_data_protection::PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(absl::nullopt),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234},
      content::ClipboardPasteData("text", "image", {}), future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);

  WaitForDialogToClose();
}

}  // namespace data_protection
