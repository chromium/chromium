// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_DLP_WARN_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_DLP_WARN_DIALOG_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace policy {

// DlpWarnDialog is a system modal dialog shown when Data Leak Protection on
// screen restriction (Screen Capture, Printing, Screen Share) level is set to
// WARN.
class DlpWarnDialog : public PolicyDialogBase {
  METADATA_HEADER(DlpWarnDialog, PolicyDialogBase)

 public:
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
             EqualWithTitles(a.confidential_contents, b.confidential_contents);
    }
    friend bool operator!=(const DlpWarnDialogOptions& a,
                           const DlpWarnDialogOptions& b) {
      return !(a == b);
    }

    Restriction restriction;
    std::optional<std::u16string> application_title;

    // Non-empty only if the |restriction| is one of kScreenCapture,
    // kVideoCapture, or kScreenshare.
    DlpConfidentialContents confidential_contents;
  };

  DlpWarnDialog() = delete;
  DlpWarnDialog(WarningCallback callback, DlpWarnDialogOptions options);
  DlpWarnDialog(const DlpWarnDialog& other) = delete;
  DlpWarnDialog& operator=(const DlpWarnDialog& other) = delete;
  ~DlpWarnDialog() override;

 private:
  // Splits `callback` and assigns to accept and cancel callbacks.
  void SetWarningCallback(WarningCallback callback);

  // PolicyDialogBase overrides:
  views::Label* AddTitle(const std::u16string& title) override;
  views::Label* AddMessage(const std::u16string& message) override;
  void MaybeAddConfidentialRows() override;
  std::u16string GetOkButton() override;
  std::u16string GetTitle() override;
  std::u16string GetMessage() override;
  void AddConfidentialRow(const gfx::ImageSkia& icon,
                          const std::u16string& title) override;

  // Returns the Cancel button label.
  std::u16string GetCancelButton();

  Restriction restriction_;
  std::optional<std::u16string> application_title_;
  DlpConfidentialContents contents_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_DLP_WARN_DIALOG_H_
