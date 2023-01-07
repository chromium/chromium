// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_WARN_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_WARN_DIALOG_H_

#include <string>

#include "base/callback_forward.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

// The callback function that is invoked when the user addresses the
// DlpWarnDialog. When `should_proceed` is set to true, the action will continue
// as if there was no restricted content. Otherwise, the operation is aborted.
using OnDlpRestrictionCheckedCallback =
    base::OnceCallback<void(bool should_proceed)>;

// DlpWarnDialog is a system modal dialog shown when Data Leak Protection
// files and on screen restriction (Screen Capture, Printing, Screen Share)
// level is set to WARN.
class DlpWarnDialog : public views::DialogDelegateView {
 public:
  METADATA_HEADER(DlpWarnDialog);

  // Type of the restriction for which the dialog is created, used to determine
  // the text shown in the dialog.
  enum class Restriction {
    kScreenCapture,
    kVideoCapture,
    kPrinting,
    kScreenShare,
    kFiles
  };

  // A structure to keep track of optional and configurable parameters of a
  // DlpWarnDialog.
  struct DlpWarnDialogOptions {
    DlpWarnDialogOptions() = delete;
    explicit DlpWarnDialogOptions(Restriction restriction);
    DlpWarnDialogOptions(Restriction restriction,
                         DlpConfidentialContents confidential_contents);
    DlpWarnDialogOptions(Restriction restriction,
                         DlpConfidentialContents confidential_contents,
                         const std::u16string& application_title);
    DlpWarnDialogOptions(
        Restriction restriction,
        const std::vector<DlpConfidentialFile>& confidential_files,
        absl::optional<DlpRulesManager::Component> dst_component,
        const absl::optional<std::string>& destination_pattern,
        DlpFilesController::FileAction files_action);
    DlpWarnDialogOptions(const DlpWarnDialogOptions& other);
    DlpWarnDialogOptions& operator=(const DlpWarnDialogOptions& other);
    ~DlpWarnDialogOptions();

    // Returns whether all members are equal.
    // Uses EqualWithTitles to compare confidential_contents, which ensures that
    // not only URLs but also the titles are equal as well.
    friend bool operator==(const DlpWarnDialogOptions& a,
                           const DlpWarnDialogOptions& b) {
      return a.restriction == b.restriction &&
             a.application_title == b.application_title &&
             a.destination_component == b.destination_component &&
             a.destination_pattern == b.destination_pattern &&
             a.files_action == b.files_action &&
             EqualWithTitles(a.confidential_contents,
                             b.confidential_contents) &&
             a.confidential_files == b.confidential_files;
    }
    friend bool operator!=(const DlpWarnDialogOptions& a,
                           const DlpWarnDialogOptions& b) {
      return !(a == b);
    }

    Restriction restriction;
    // May have content only if the |restriction| is not kFiles.
    DlpConfidentialContents confidential_contents;
    // May have files only if the |restriction| is kFiles.
    std::vector<DlpConfidentialFile> confidential_files;
    absl::optional<std::u16string> application_title;

    // May have value only if the |restriction| is kFiles.
    absl::optional<DlpRulesManager::Component> destination_component;
    // Has value only if the |restriction| is kFiles.
    absl::optional<std::string> destination_pattern;
    // Has value only if the |restriction| is kFiles.
    absl::optional<DlpFilesController::FileAction> files_action;
  };

  DlpWarnDialog() = delete;
  DlpWarnDialog(OnDlpRestrictionCheckedCallback callback,
                DlpWarnDialogOptions options);
  DlpWarnDialog(const DlpWarnDialog& other) = delete;
  DlpWarnDialog& operator=(const DlpWarnDialog& other) = delete;
  ~DlpWarnDialog() override = default;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_WARN_DIALOG_H_
