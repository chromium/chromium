// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_POLICY_DIALOG_BASE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_POLICY_DIALOG_BASE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/image/image_skia.h"
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

  // ViewIds to query different Views of this dialog using View::GetViewByID().
  // Used for testing the dialog.
  enum ViewIds {
    kScrollViewId = 1,
    kConfidentialRowTitleViewId,
  };

  PolicyDialogBase();
  PolicyDialogBase(const PolicyDialogBase& other) = delete;
  PolicyDialogBase& operator=(const PolicyDialogBase& other) = delete;
  ~PolicyDialogBase() override = default;

 protected:
  // Splits `callback` and assigns to accept and cancel callbacks.
  void SetOnDlpRestrictionCheckedCallback(
      OnDlpRestrictionCheckedCallback callback);

  // Sets up the dialog's upper panel and adds the managed icon and container
  // for the title and message. To add the text, use `AddTitle()` and
  // `AddMessage()` after this method.
  void SetupUpperPanel();

  // Adds and returns label with `title`. Should only be called after
  // `SetupUpperPanel()`.
  virtual views::Label* AddTitle(const std::u16string& title);

  // Adds and returns label with `message`. Should only be called after
  // `SetupUpperPanel()`.
  virtual views::Label* AddMessage(const std::u16string& message);

  // Sets up the scroll view container.
  virtual void SetupScrollView();

  // Sets up and populates the upper section of the dialog.
  void AddGeneralInformation();

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

  // Adds the `icon` to `row`.
  void AddRowIcon(const gfx::ImageSkia& icon, views::View* row);

  // Adds the `title` to `row` and returns the created label for further
  // styling.
  views::Label* AddRowTitle(const std::u16string& title, views::View* row);

  // Adds one row with |icon| and |title|. Should only be called after
  // SetupScrollView().
  virtual void AddConfidentialRow(const gfx::ImageSkia& icon,
                                  const std::u16string& title) = 0;

  // The upper section of the dialog.
  raw_ptr<views::View, ExperimentalAsh> upper_panel_;
  // The scrollable container used for listing contents or files.
  raw_ptr<views::View, ExperimentalAsh> scroll_view_container_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_POLICY_DIALOG_BASE_H_
