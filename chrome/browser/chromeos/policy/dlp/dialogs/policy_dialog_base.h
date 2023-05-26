// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_POLICY_DIALOG_BASE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_POLICY_DIALOG_BASE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

// The callback to be executed when the user addresses the dialog. When
// `should_proceed` is set to true, the action continues and is aborted
// otherwise.
using OnDlpRestrictionCheckedCallback =
    base::OnceCallback<void(bool should_proceed)>;

// PolicyDialogBase is the base class for showing Data Protection warnings or
// detailed error dialogs.
class PolicyDialogBase : public views::DialogDelegateView {
 public:
  METADATA_HEADER(PolicyDialogBase);

  // Type of the restriction for which the dialog is created.
  enum class Restriction {
    kScreenCapture,
    kVideoCapture,
    kPrinting,
    kScreenShare,
    kFiles
  };

  PolicyDialogBase();
  PolicyDialogBase(const PolicyDialogBase& other) = delete;
  PolicyDialogBase& operator=(const PolicyDialogBase& other) = delete;
  ~PolicyDialogBase() override = default;

 protected:
  // Splits `callback` and assigns to accept and cancel callbacks.
  void SetOnDlpRestrictionCheckedCallback(
      OnDlpRestrictionCheckedCallback callback);

  // Sets up the dialog's upper panel with |title| and |message|.
  void SetupUpperPanel(const std::u16string& title,
                       const std::u16string& message);

  // Sets up the scroll view container.
  void SetupScrollView();

  // Sets up and populates the upper section of the dialog.
  virtual void AddGeneralInformation() = 0;

  // Sets up and populates the scroll view.
  virtual void MaybeAddConfidentialRows() = 0;

  // Returns the Cancel button label.
  virtual std::u16string GetCancelButton() = 0;

  // Returns the Ok button label.
  virtual std::u16string GetOkButton() = 0;

  // Returns the title.
  virtual std::u16string GetTitle() = 0;

  // Returns the message text.
  virtual std::u16string GetMessage() = 0;

  // Adds one row with |icon| and |title|. Should only be called after
  // SetupScrollView().
  void AddConfidentialRow(const gfx::ImageSkia& icon,
                          const std::u16string& title);

  // The upper section of the dialog.
  raw_ptr<views::View, ExperimentalAsh> upper_panel_;
  // The scrollable container used for listing contents or files.
  raw_ptr<views::View, ExperimentalAsh> scroll_view_container_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_POLICY_DIALOG_BASE_H_
