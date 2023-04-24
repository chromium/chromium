// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_DLP_WARN_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_DLP_WARN_DIALOG_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace policy {

// DlpWarnDialog is a system modal dialog shown when Data Leak Protection on
// screen restriction (Screen Capture, Printing, Screen Share) level is set to
// WARN.
class DlpWarnDialog : public PolicyDialogBase {
 public:
  METADATA_HEADER(DlpWarnDialog);

  // A structure to keep track of optional and configurable parameters of a
  // DlpWarnDialog.
  // TODO(b/278046656): Clean this up.
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
        absl::optional<DlpFileDestination> files_destination,
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
             a.files_destination == b.files_destination &&
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
    absl::optional<std::u16string> application_title;

    // Non-empty only if the |restriction| is one of kScreenCapture,
    // kVideoCapture, or kScreenshare.
    DlpConfidentialContents confidential_contents;

    // Have value only if the |restriction| is kFiles:
    std::vector<DlpConfidentialFile> confidential_files;
    absl::optional<DlpFileDestination> files_destination;
    absl::optional<DlpFilesController::FileAction> files_action;
  };

  DlpWarnDialog() = delete;
  DlpWarnDialog(OnDlpRestrictionCheckedCallback callback,
                DlpWarnDialogOptions options);
  DlpWarnDialog(const DlpWarnDialog& other) = delete;
  DlpWarnDialog& operator=(const DlpWarnDialog& other) = delete;
  ~DlpWarnDialog() override;

 private:
  // PolicyDialogBase overrides:
  void AddGeneralInformation() override;
  void MaybeAddConfidentialRows() override;
  std::u16string GetOkButton() override;
  std::u16string GetCancelButton() override;
  std::u16string GetTitle() override;
  std::u16string GetMessage() override;

  Restriction restriction_;
  absl::optional<std::u16string> application_title_;
  DlpConfidentialContents contents_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_DLP_WARN_DIALOG_H_
