// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_ERROR_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_ERROR_DIALOG_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/native_widget_types.h"

namespace policy {

// FilesPolicyErrorDialog is a window modal dialog used to show detailed
// overview of files blocked by data protection policies.
class FilesPolicyErrorDialog : public FilesPolicyDialog {
  METADATA_HEADER(FilesPolicyErrorDialog, FilesPolicyDialog)

 public:
  FilesPolicyErrorDialog() = delete;
  FilesPolicyErrorDialog(const std::map<BlockReason, Info>& dialog_info_map,
                         dlp::FileAction action,
                         gfx::NativeWindow modal_parent);
  FilesPolicyErrorDialog(const FilesPolicyErrorDialog&) = delete;
  FilesPolicyErrorDialog(FilesPolicyErrorDialog&&) = delete;
  FilesPolicyErrorDialog& operator=(const FilesPolicyErrorDialog&) = delete;
  FilesPolicyErrorDialog& operator=(FilesPolicyErrorDialog&&) = delete;
  ~FilesPolicyErrorDialog() override;

 private:
  // Holds all the information of a section of the dialog.
  struct BlockedFilesSection {
    BlockedFilesSection(
        int view_id,
        const std::u16string& message,
        const std::vector<DlpConfidentialFile>& files,
        const std::vector<std::pair<GURL, std::u16string>>& learn_more_urls);
    ~BlockedFilesSection();

    BlockedFilesSection(const BlockedFilesSection& other);
    BlockedFilesSection& operator=(BlockedFilesSection&& other);

    // A unique ID attached to the view element holding `message`. This ID is
    // only set for mixed error dialogs and allows to figure out in tests
    // whether certain sections have been added to the dialog.
    int view_id;

    // The message shown to the user describing why `files` have been blocked.
    std::u16string message;

    // The blocked files.
    std::vector<DlpConfidentialFile> files;

    // Learn more URLs displayed to the user and their accessible name read out
    // by ChromeVox. A section may hold files blocked for different reasons,
    // each of which defining its own learn more URL.
    std::vector<std::pair<GURL, std::u16string>> learn_more_urls;
  };

  // PolicyDialogBase overrides:
  void MaybeAddConfidentialRows() override;
  std::u16string GetOkButton() override;
  std::u16string GetTitle() override;
  std::u16string GetMessage() override;

  // Initialize the `sections_` vector by possibly aggregating data taken from
  // the `dialog_info_map`.
  void SetupBlockedFilesSections(
      const std::map<BlockReason, Info>& dialog_info_map);

  // Appends a section to `sections_`. Details such as the displayed message and
  // list of blocked files are retrieved from `dialog_info_map`. It is no-op if
  // `reason` is not in `dialog_info_map` or there are no files blocked for the
  // given `reason`.
  void AppendBlockedFilesSection(
      BlockReason reason,
      const std::map<BlockReason, Info>& dialog_info_map);

  // Adds the given `section` to the dialog.
  // Should only be called after `SetupUpperPanel()`.
  void AddBlockedFilesSection(const BlockedFilesSection& section);

  // Called from the dialog's "OK" button.
  // Dismisses the dialog.
  void Dismiss();

  // The dialog is composed of one section in case of single error dialog or
  // more sections for mixed errors. Every section holds the information that
  // allows to populate the dialog UI such as the list of blocked files and the
  // message that should be shown to the user.
  std::vector<BlockedFilesSection> sections_;

  // Total number of blocked files for all policy reasons.
  size_t file_count_;

  base::WeakPtrFactory<FilesPolicyErrorDialog> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_ERROR_DIALOG_H_
