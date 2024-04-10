// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/data_controls/data_controls_dialog.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/data_controls/features.h"
#include "components/enterprise/data_controls/test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget_delegate.h"

namespace enterprise_data_protection {

namespace {

content::ClipboardPasteData MakeClipboardPasteData(
    std::string text,
    std::string image,
    std::vector<base::FilePath> file_paths) {
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = base::UTF8ToUTF16(text);
  clipboard_paste_data.png = std::vector<uint8_t>(image.begin(), image.end());
  clipboard_paste_data.file_paths = std::move(file_paths);
  return clipboard_paste_data;
}

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
    EXPECT_TRUE(expected_dialog_type_);
    EXPECT_EQ(dialog->type(), expected_dialog_type_);

    dialog_init_loop_ = std::make_unique<base::RunLoop>();
    dialog_close_loop_ = std::make_unique<base::RunLoop>();
    dialog_init_callback_ = dialog_init_loop_->QuitClosure();
    dialog_close_callback_ = dialog_close_loop_->QuitClosure();
  }

  void OnWidgetInitialized(data_controls::DataControlsDialog* dialog) override {
    ASSERT_TRUE(dialog);
    ASSERT_EQ(dialog, constructed_dialog_);

    std::move(dialog_init_callback_).Run();
  }

  void OnDestructed(data_controls::DataControlsDialog* dialog) override {
    ASSERT_TRUE(dialog);
    ASSERT_EQ(dialog, constructed_dialog_);
    constructed_dialog_ = nullptr;

    std::move(dialog_close_callback_).Run();
  }

  // Some platforms crash if the dialog has been accepted/cancelled before fully
  // launching modally, so to avoid that issue accepting/cancelling the dialog
  // is done asynchronously.
  void AcceptDialog() {
    EXPECT_TRUE(constructed_dialog_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&data_controls::DataControlsDialog::AcceptDialog,
                       base::Unretained(constructed_dialog_)));
  }
  void CancelDialog() {
    EXPECT_TRUE(constructed_dialog_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&data_controls::DataControlsDialog::CancelDialog,
                       base::Unretained(constructed_dialog_)));
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void WaitForDialogToInitialize() {
    ASSERT_TRUE(dialog_init_loop_);
    dialog_init_loop_->Run();
  }

  void WaitForDialogToClose() {
    ASSERT_TRUE(dialog_close_loop_);
    dialog_close_loop_->Run();
  }

  void set_expected_dialog_type(data_controls::DataControlsDialog::Type type) {
    expected_dialog_type_ = type;
  }

 protected:
  base::test::ScopedFeatureList scoped_features_;

  // Members used to track the dialog being initialized.
  std::unique_ptr<base::RunLoop> dialog_init_loop_;
  base::OnceClosure dialog_init_callback_;

  // Members used to track the dialog closing.
  std::unique_ptr<base::RunLoop> dialog_close_loop_;
  base::OnceClosure dialog_close_callback_;

  std::optional<data_controls::DataControlsDialog::Type> expected_dialog_type_;

  raw_ptr<data_controls::DataControlsDialog> constructed_dialog_ = nullptr;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       PasteAllowed_NoSource) {
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");

  EXPECT_FALSE(constructed_dialog_);
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       PasteAllowed_SameSource) {
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");

  EXPECT_FALSE(constructed_dialog_);
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       PasteBlockedByDataControls_DestinationRule) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);

  WaitForDialogToInitialize();
  CancelDialog();
  WaitForDialogToClose();
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_BypassedDestinationRule) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  AcceptDialog();
  WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_CanceledDestinationRule) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  CancelDialog();
  WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);
}

// Ash requires extra boilerplate to run this test, and since copy-pasting
// between profiles on Ash isn't a meaningful test it is simply omitted from
// running this.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       PasteBlockedByDataControls_SourceRule) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile.
  std::unique_ptr<Profile> destination_profile;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    destination_profile = Profile::CreateProfile(
        g_browser_process->profile_manager()->user_data_dir().Append(
            FILE_PATH_LITERAL("DC Test Profile")),
        /*delegate=*/nullptr, Profile::CreateMode::CREATE_MODE_SYNCHRONOUS);
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://foo.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [&destination_profile]() -> content::BrowserContext* {
                return destination_profile.get();
              }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);

  WaitForDialogToInitialize();
  CancelDialog();
  WaitForDialogToClose();
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_BypassedSourceRule) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile.
  std::unique_ptr<Profile> destination_profile;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    destination_profile = Profile::CreateProfile(
        g_browser_process->profile_manager()->user_data_dir().Append(
            FILE_PATH_LITERAL("DC Test Profile")),
        /*delegate=*/nullptr, Profile::CreateMode::CREATE_MODE_SYNCHRONOUS);
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://foo.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [&destination_profile]() -> content::BrowserContext* {
                return destination_profile.get();
              }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  AcceptDialog();
  WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_CanceledSourceRule) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "destinations": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile.
  std::unique_ptr<Profile> destination_profile;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    destination_profile = Profile::CreateProfile(
        g_browser_process->profile_manager()->user_data_dir().Append(
            FILE_PATH_LITERAL("DC Test Profile")),
        /*delegate=*/nullptr, Profile::CreateMode::CREATE_MODE_SYNCHRONOUS);
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://foo.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [&destination_profile]() -> content::BrowserContext* {
                return destination_profile.get();
              }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  CancelDialog();
  WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest, CopyAllowed) {
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("foo", "", {}),
      future.GetCallback());

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest, CopyBlocked) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardCopyBlock);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("foo", "", {}),
      future.GetCallback());

  WaitForDialogToInitialize();
  CancelDialog();
  WaitForDialogToClose();

  EXPECT_FALSE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenCanceled) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("foo", "", {}),
      future.GetCallback());

  WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  CancelDialog();
  WaitForDialogToClose();

  EXPECT_FALSE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenCanceled_OsClipboardDestination) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("foo", "", {}),
      future.GetCallback());

  WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  CancelDialog();
  WaitForDialogToClose();

  EXPECT_FALSE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenBypassed) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("foo", "", {}),
      future.GetCallback());

  WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  AcceptDialog();
  WaitForDialogToClose();

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
}

IN_PROC_BROWSER_TEST_F(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenBypassed_OsClipboardDestination) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  set_expected_dialog_type(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("foo", "", {}),
      future.GetCallback());

  WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  AcceptDialog();
  WaitForDialogToClose();

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
}

}  // namespace enterprise_data_protection
