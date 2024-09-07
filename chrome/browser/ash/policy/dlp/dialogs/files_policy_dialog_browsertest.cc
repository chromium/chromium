// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"

#include <tuple>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_string_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textarea/textarea.h"

namespace policy {
namespace {
// Generate a random string with the given `length`.
std::u16string GenerateText(size_t length) {
  std::u16string random_string;
  for (size_t index = 0; index < length; ++index) {
    random_string += static_cast<char16_t>(base::RandInt('A', 'Z'));
  }
  return random_string;
}
}  // namespace

class FilesPolicyDialogBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<dlp::FileAction> {
 public:
  FilesPolicyDialogBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kNewFilesPolicyUX);
  }
  FilesPolicyDialogBrowserTest(const FilesPolicyDialogBrowserTest&) = delete;
  FilesPolicyDialogBrowserTest& operator=(const FilesPolicyDialogBrowserTest&) =
      delete;
  ~FilesPolicyDialogBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Setup the Files app.
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(
        browser()->profile());
  }

 protected:
  Browser* FindFilesApp() {
    return FindSystemWebAppBrowser(browser()->profile(),
                                   ash::SystemWebAppType::FILE_MANAGER);
  }

  Browser* OpenFilesApp() {
    base::RunLoop run_loop;
    file_manager::util::ShowItemInFolder(
        browser()->profile(),
        file_manager::util::GetDownloadsFolderForProfile(browser()->profile()),
        base::BindLambdaForTesting(
            [&run_loop](platform_util::OpenOperationResult result) {
              EXPECT_EQ(platform_util::OpenOperationResult::OPEN_SUCCEEDED,
                        result);
              run_loop.Quit();
            }));
    run_loop.Run();
    return ui_test_utils::WaitForBrowserToOpen();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  const base::HistogramTester histogram_tester_;
};

class WarningDialogBrowserTest : public FilesPolicyDialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    FilesPolicyDialogBrowserTest::SetUpOnMainThread();

    warning_paths_.emplace_back("file1.txt");
    warning_paths_.emplace_back("file2.txt");
  }

 protected:
  std::vector<base::FilePath> warning_paths_;
  base::MockCallback<WarningWithJustificationCallback> cb_;
};

// Tests that the warning dialog is created as a system modal if no parent is
// passed, and that accepting the dialog runs the callback with true.
IN_PROC_BROWSER_TEST_P(WarningDialogBrowserTest, NoParent) {
  dlp::FileAction action = GetParam();

  auto* widget = FilesPolicyDialog::CreateWarnDialog(
      cb_.Get(), action,
      /*modal_parent=*/nullptr,
      FilesPolicyDialog::Info::Warn(FilesPolicyDialog::BlockReason::kDlp,
                                    warning_paths_));
  ASSERT_TRUE(widget);

  FilesPolicyWarnDialog* dialog = static_cast<FilesPolicyWarnDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  // There should be no justification text area since it's not set in the
  // FilesPolicyDialog::Info above.
  ASSERT_EQ(widget->GetRootView()->GetViewByID(
                PolicyDialogBase::kEnterpriseConnectorsJustificationTextareaId),
            nullptr);

  EXPECT_EQ(dialog->GetModalType(), ui::mojom::ModalType::kSystem);
  // Proceed.
  EXPECT_CALL(cb_, Run(/*user_justification=*/std::optional<std::u16string>(),
                       /*should_proceed=*/true))
      .Times(1);
  dialog->AcceptDialog();
  EXPECT_TRUE(widget->IsClosed());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionWarnReviewedUMA)),
              base::BucketsAre(base::Bucket(action, 1)));
}

// Tests that the warning dialog is created as a window modal if a Files app
// window is passed as the parent, and that cancelling the dialog runs the
// callback with false.
IN_PROC_BROWSER_TEST_P(WarningDialogBrowserTest, WithParent) {
  dlp::FileAction action = GetParam();

  ASSERT_FALSE(FindFilesApp());
  Browser* files_app = OpenFilesApp();
  ASSERT_TRUE(files_app);
  ASSERT_EQ(files_app, FindFilesApp());

  auto* widget = FilesPolicyDialog::CreateWarnDialog(
      cb_.Get(), action, files_app->window()->GetNativeWindow(),
      FilesPolicyDialog::Info::Warn(FilesPolicyDialog::BlockReason::kDlp,
                                    warning_paths_));
  ASSERT_TRUE(widget);

  FilesPolicyWarnDialog* dialog = static_cast<FilesPolicyWarnDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::mojom::ModalType::kWindow);
  EXPECT_EQ(widget->parent()->GetNativeWindow(),
            files_app->window()->GetNativeWindow());
  // Cancel.
  EXPECT_CALL(cb_, Run(/*user_justification=*/std::optional<std::u16string>(),
                       /*should_proceed=*/false))
      .Times(1);
  dialog->CancelDialog();
  EXPECT_TRUE(widget->IsClosed());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionWarnReviewedUMA)),
              base::BucketsAre(base::Bucket(action, 1)));
}

// Tests that the warning dialog contains a justification textarea and when the
// warning is proceeded the user justification is forwarded to the continue
// callback.
IN_PROC_BROWSER_TEST_P(WarningDialogBrowserTest, JustificationTextarea) {
  dlp::FileAction action = GetParam();

  auto dialog_info = FilesPolicyDialog::Info::Warn(
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
      warning_paths_);
  dialog_info.SetBypassRequiresJustification(true);

  auto* widget = FilesPolicyDialog::CreateWarnDialog(
      cb_.Get(), action, /*modal_parent=*/nullptr, std::move(dialog_info));
  ASSERT_TRUE(widget);

  FilesPolicyWarnDialog* dialog = static_cast<FilesPolicyWarnDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  views::Textarea* justification_area =
      static_cast<views::Textarea*>(widget->GetRootView()->GetViewByID(
          PolicyDialogBase::kEnterpriseConnectorsJustificationTextareaId));
  ASSERT_TRUE(justification_area);

  // The OK button should be disabled if there is no text in the justification
  // area.
  EXPECT_FALSE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  const size_t max_chars = dialog->GetMaxBypassJustificationLengthForTesting();
  const std::u16string valid_justification = GenerateText(max_chars);

  // The OK button should be enable if the max amount of char is inserted.
  justification_area->SetText(u"");
  justification_area->InsertText(
      valid_justification,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // The OK button should also be disabled if an extra char is added.
  justification_area->InsertText(
      GenerateText(1),
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_FALSE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Reset a valid justification by deleting the extra char to proceed the
  // warning.
  justification_area->DeleteRange(gfx::Range(max_chars, max_chars + 1));

  EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  EXPECT_EQ(dialog->GetModalType(), ui::mojom::ModalType::kSystem);
  // Proceed.
  EXPECT_CALL(cb_, Run({valid_justification}, /*should_proceed=*/true))
      .Times(1);
  dialog->AcceptDialog();
  EXPECT_TRUE(widget->IsClosed());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionWarnReviewedUMA)),
              base::BucketsAre(base::Bucket(action, 1)));
}

INSTANTIATE_TEST_SUITE_P(FilesPolicyDialog,
                         WarningDialogBrowserTest,
                         ::testing::Values(dlp::FileAction::kDownload,
                                           dlp::FileAction::kTransfer,
                                           dlp::FileAction::kUpload,
                                           dlp::FileAction::kCopy,
                                           dlp::FileAction::kMove,
                                           dlp::FileAction::kOpen,
                                           dlp::FileAction::kShare));

class ErrorDialogBrowserTest : public FilesPolicyDialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    FilesPolicyDialogBrowserTest::SetUpOnMainThread();

    const std::vector<base::FilePath> paths = {base::FilePath("file1.txt"),
                                               base::FilePath("file2.txt")};

    dialog_info_map_.insert({FilesPolicyDialog::BlockReason::kDlp,
                             FilesPolicyDialog::Info::Error(
                                 FilesPolicyDialog::BlockReason::kDlp, paths)});
  }

  // Checks that a error dialog with mixed errors only contains the sections for
  // the given `reasons`.
  void ContainMixedErrorSections(
      FilesPolicyErrorDialog* dialog,
      const std::vector<FilesPolicyDialog::BlockReason>& reasons) {
    std::set<FilesPolicyDialog::BlockReason> reasons_without_sections(
        std::begin(FilesPolicyDialog::available_reasons),
        std::end(FilesPolicyDialog::available_reasons));
    for (FilesPolicyDialog::BlockReason reason : reasons) {
      // The view ID is attached to the title label.
      views::View* title_label = dialog->GetViewByID(
          FilesPolicyDialog::MapBlockReasonToViewID(reason));
      EXPECT_TRUE(title_label);
      reasons_without_sections.erase(reason);
    }
    for (FilesPolicyDialog::BlockReason reason : reasons_without_sections) {
      EXPECT_FALSE(dialog->GetViewByID(
          FilesPolicyDialog::MapBlockReasonToViewID(reason)));
    }
  }

  std::u16string GetTitle(FilesPolicyErrorDialog* dialog,
                          FilesPolicyDialog::BlockReason reason) {
    views::View* title_label =
        dialog->GetViewByID(FilesPolicyDialog::MapBlockReasonToViewID(reason));
    if (!title_label) {
      return u"";
    }
    // The view ID is attached to the title label.
    return static_cast<views::Label*>(title_label)->GetText();
  }

 protected:
  std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info>
      dialog_info_map_;
};

// Tests that the error dialog is created as a system modal if no parent is
// passed, and that accepting the dialog dismisses it without any other action.
IN_PROC_BROWSER_TEST_P(ErrorDialogBrowserTest, NoParent) {
  dlp::FileAction action = GetParam();
  // Add another blocked file to test the mixed error case.
  const std::vector<base::FilePath> paths = {base::FilePath("file3.txt")};

  auto dialog_settings = FilesPolicyDialog::Info::Error(
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
      paths);

  // Override default dialog settings.
  dialog_settings.SetMessage(u"Custom message");
  dialog_settings.SetLearnMoreURL(GURL("https://learnmore.com"));

  dialog_info_map_.insert(
      {FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
       std::move(dialog_settings)});

  auto* widget = FilesPolicyDialog::CreateErrorDialog(dialog_info_map_, action,
                                                      /*modal_parent=*/nullptr);
  ASSERT_TRUE(widget);

  FilesPolicyErrorDialog* dialog = static_cast<FilesPolicyErrorDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::mojom::ModalType::kSystem);
  // Accept -> dismiss.
  dialog->AcceptDialog();
  EXPECT_TRUE(widget->IsClosed());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionBlockReviewedUMA)),
              base::BucketsAre(base::Bucket(action, 1)));
}

// Tests that the error dialog is created as a window modal if a Files app
// window is passed as the parent, and that accepting the dialog dismisses it
// without any other action.
IN_PROC_BROWSER_TEST_P(ErrorDialogBrowserTest, WithParent) {
  dlp::FileAction action = GetParam();

  ASSERT_FALSE(FindFilesApp());
  Browser* files_app = OpenFilesApp();
  ASSERT_TRUE(files_app);
  ASSERT_EQ(files_app, FindFilesApp());

  auto* widget = FilesPolicyDialog::CreateErrorDialog(
      dialog_info_map_, action, files_app->window()->GetNativeWindow());
  ASSERT_TRUE(widget);

  FilesPolicyErrorDialog* dialog = static_cast<FilesPolicyErrorDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::mojom::ModalType::kWindow);
  EXPECT_EQ(widget->parent()->GetNativeWindow(),
            files_app->window()->GetNativeWindow());
  // Accept -> dismiss.
  dialog->AcceptDialog();
  EXPECT_TRUE(widget->IsClosed());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionBlockReviewedUMA)),
              base::BucketsAre(base::Bucket(action, 1)));
}

// Tests that the error dialog is populated with one section for every available
// block reason.
IN_PROC_BROWSER_TEST_P(ErrorDialogBrowserTest, AllErrorSections) {
  dlp::FileAction action = GetParam();

  std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info> info_map;

  size_t count = 0;
  for (FilesPolicyDialog::BlockReason reason :
       FilesPolicyDialog::available_reasons) {
    const base::FilePath path1("file" + base::NumberToString(count++) + ".txt");
    const base::FilePath path2("file" + base::NumberToString(count++) + ".txt");

    auto dialog_settings =
        FilesPolicyDialog::Info::Error(reason, {path1, path2});
    info_map.insert({reason, std::move(dialog_settings)});
  }

  // Sensitive data and malware have their own section only when a custom
  // message is defined.
  const std::u16string sensitive_data_message =
      u"Sensitive data custom message";
  info_map
      .at(FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData)
      .SetMessage(sensitive_data_message);

  const std::u16string malware_message = u"Malware data custom message";
  info_map.at(FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware)
      .SetMessage(malware_message);

  auto* widget = FilesPolicyDialog::CreateErrorDialog(info_map, action,
                                                      /*modal_parent=*/nullptr);
  ASSERT_TRUE(widget);

  FilesPolicyErrorDialog* dialog = static_cast<FilesPolicyErrorDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  std::vector<FilesPolicyDialog::BlockReason> expected_sections = {
      FilesPolicyDialog::BlockReason::kDlp,
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware,
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsScanFailed,
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsEncryptedFile,
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsLargeFile,
      FilesPolicyDialog::BlockReason::kEnterpriseConnectors};

  ContainMixedErrorSections(dialog, expected_sections);

  for (FilesPolicyDialog::BlockReason reason : expected_sections) {
    if (reason == FilesPolicyDialog::BlockReason::kEnterpriseConnectors) {
      // In this case we also expect files with
      // FilesPolicyDialog::BlockReason::kEnterpriseConnectorsUnknownScanResult
      // block reason.
      size_t files_num = info_map.at(reason).GetFiles().size() +
                         info_map
                             .at(FilesPolicyDialog::BlockReason::
                                     kEnterpriseConnectorsUnknownScanResult)
                             .GetFiles()
                             .size();
      ASSERT_EQ(GetTitle(dialog, reason),
                files_string_util::GetBlockReasonMessage(reason, files_num));
      continue;
    }
    ASSERT_EQ(GetTitle(dialog, reason), info_map.at(reason).GetMessage());
  }
}

// Tests that when no custom message is specified for Enterprise Connectors
// malware and sensitive data, the error dialog is a single error dialog.
IN_PROC_BROWSER_TEST_P(ErrorDialogBrowserTest,
                       NoEnterpriseConnectorsCustomMessage) {
  dlp::FileAction action = GetParam();

  std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info> info_map;

  auto sensitive_data_file_dialog_settings = FilesPolicyDialog::Info::Error(
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
      {base::FilePath("file1.txt"), base::FilePath("file2.txt")});

  auto malware_file_dialog_settings = FilesPolicyDialog::Info::Error(
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware,
      {base::FilePath("file3.txt"), base::FilePath("file4.txt")});

  info_map.insert(
      {FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
       std::move(sensitive_data_file_dialog_settings)});
  info_map.insert({FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware,
                   std::move(malware_file_dialog_settings)});

  auto* widget = FilesPolicyDialog::CreateErrorDialog(info_map, action,
                                                      /*modal_parent=*/nullptr);
  ASSERT_TRUE(widget);

  FilesPolicyErrorDialog* dialog = static_cast<FilesPolicyErrorDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  // The dialog is a single error dialog, and thus it does not contain mixed
  // error sections.
  ContainMixedErrorSections(dialog, {});
}

// Tests that when files are blocked because of Enterprise Connectors malware
// and sensitive data reasons, but a custom message is only defined for one of
// them, e.g., malware, the error dialog should have two sections: one for
// malware with the custom message, and a generic one section with a default
// message for the other files.
IN_PROC_BROWSER_TEST_P(ErrorDialogBrowserTest,
                       EnterpriseConnectorsCustomMessage) {
  dlp::FileAction action = GetParam();

  std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info> info_map;

  std::vector<base::FilePath> sensitive_data_paths = {
      base::FilePath("file1.txt"), base::FilePath("file2.txt")};
  auto sensitive_data_file_dialog_settings = FilesPolicyDialog::Info::Error(
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
      sensitive_data_paths);

  auto malware_file_dialog_settings = FilesPolicyDialog::Info::Error(
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware,
      {base::FilePath("file3.txt"), base::FilePath("file4.txt")});
  const std::u16string malware_message = u"Malware data custom message";
  malware_file_dialog_settings.SetMessage(malware_message);

  info_map.insert(
      {FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
       std::move(sensitive_data_file_dialog_settings)});
  info_map.insert({FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware,
                   std::move(malware_file_dialog_settings)});

  auto* widget = FilesPolicyDialog::CreateErrorDialog(info_map, action,
                                                      /*modal_parent=*/nullptr);
  ASSERT_TRUE(widget);

  FilesPolicyErrorDialog* dialog = static_cast<FilesPolicyErrorDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  ContainMixedErrorSections(
      dialog, {FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware,
               FilesPolicyDialog::BlockReason::kEnterpriseConnectors});

  ASSERT_EQ(
      GetTitle(dialog,
               FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware),
      l10n_util::GetStringFUTF16(IDS_POLICY_DLP_FROM_YOUR_ADMIN_MESSAGE,
                                 malware_message));

  ASSERT_EQ(
      GetTitle(dialog, FilesPolicyDialog::BlockReason::kEnterpriseConnectors),
      files_string_util::GetBlockReasonMessage(
          FilesPolicyDialog::BlockReason::kEnterpriseConnectors,
          sensitive_data_paths.size()));
}

INSTANTIATE_TEST_SUITE_P(FilesPolicyDialog,
                         ErrorDialogBrowserTest,
                         ::testing::Values(dlp::FileAction::kDownload,
                                           dlp::FileAction::kTransfer,
                                           dlp::FileAction::kUpload,
                                           dlp::FileAction::kCopy,
                                           dlp::FileAction::kMove,
                                           dlp::FileAction::kOpen,
                                           dlp::FileAction::kShare));

// Class to test "old" DLP Files restriction warning dialogs.
class DlpWarningDialogDestinationBrowserTest : public InProcessBrowserTest {
 public:
  DlpWarningDialogDestinationBrowserTest() = default;
  DlpWarningDialogDestinationBrowserTest(
      const DlpWarningDialogDestinationBrowserTest&) = delete;
  DlpWarningDialogDestinationBrowserTest& operator=(
      const DlpWarningDialogDestinationBrowserTest&) = delete;
  ~DlpWarningDialogDestinationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    warning_paths_.emplace_back("file1.txt");
    warning_paths_.emplace_back("file2.txt");
  }

 protected:
  std::vector<base::FilePath> warning_paths_;
};

// (b/281495499): This is a test for the crash that happens upon showing a
// warning dialog for downloads.
IN_PROC_BROWSER_TEST_F(DlpWarningDialogDestinationBrowserTest, Download) {
  auto paths = std::vector<base::FilePath>({base::FilePath("file1.txt")});
  ASSERT_TRUE(FilesPolicyDialog::CreateWarnDialog(
      base::DoNothing(), dlp::FileAction::kDownload,
      /*modal_parent=*/nullptr,
      FilesPolicyDialog::Info::Warn(FilesPolicyDialog::BlockReason::kDlp,
                                    paths),
      DlpFileDestination(data_controls::Component::kDrive)));
}

class DestinationBrowserTest
    : public DlpWarningDialogDestinationBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<dlp::FileAction, DlpFileDestination>> {};

IN_PROC_BROWSER_TEST_P(DestinationBrowserTest, CreateDialog) {
  auto [action, destination] = GetParam();

  ASSERT_TRUE(FilesPolicyDialog::CreateWarnDialog(
      base::DoNothing(), action,
      /*modal_parent=*/nullptr,
      FilesPolicyDialog::Info::Warn(FilesPolicyDialog::BlockReason::kDlp,
                                    warning_paths_),
      destination));
}

INSTANTIATE_TEST_SUITE_P(
    FilesPolicyDialog,
    DestinationBrowserTest,
    ::testing::Values(
        // (b/277594200): This is a test for the crash that happens upon showing
        // a warning dialog when a file is dragged to a webpage.
        std::make_tuple(dlp::FileAction::kCopy,
                        DlpFileDestination(GURL("https://example.com"))),
        std::make_tuple(dlp::FileAction::kUpload,
                        DlpFileDestination(GURL("https://example.com"))),
        // (b/273269211): This is a test for the crash that happens upon showing
        // a warning dialog when a file is moved to Google Drive.
        std::make_tuple(dlp::FileAction::kMove,
                        DlpFileDestination(data_controls::Component::kDrive)),
        std::make_tuple(dlp::FileAction::kTransfer,
                        DlpFileDestination(data_controls::Component::kArc)),
        std::make_tuple(
            dlp::FileAction::kUnknown,
            DlpFileDestination(data_controls::Component::kCrostini)),
        std::make_tuple(dlp::FileAction::kOpen,
                        DlpFileDestination(data_controls::Component::kUsb)),
        std::make_tuple(
            dlp::FileAction::kMove,
            DlpFileDestination(data_controls::Component::kPluginVm)),
        std::make_tuple(
            dlp::FileAction::kShare,
            DlpFileDestination(data_controls::Component::kOneDrive))));

}  // namespace policy
