// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_DIALOG_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/enterprise/data_controls/rule.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Label;
class BoxLayoutView;
}  // namespace views

namespace data_controls {

// Tab-modal dialog used to warn the user their action is interrupted by a Data
// Controls rule. The dialog looks like this (exact strings vary depending on
// what action is blocked):
//
// +---------------------------------------------------+
// |                                                    |
// |  Pasting this content to this site is not allowed  |
// |  +---+                                             |
// |  | E | Your administrator has blocked this action  |
// |  +---+                                             |
// |                                                    |
// |                           +--------+  +---------+  |
// |                           | Cancel |  | Proceed |  |
// |                           +--------+  +---------+  |
// +----------------------------------------------------+
//
// * The "E" box represents the enterprise logo that looks like a building.
// * The "Cancel"/"Proceed" choice is only available for warnings, blocked
//   actions only have a "Close" button.
class DataControlsDialog : public views::DialogDelegate {
 public:
  // Represents the type of dialog, based on the action that triggered it and
  // severity of the triggered rule. This will change the strings in the dialog,
  // available buttons, etc.
  enum class Type {
    kClipboardPasteBlock,
    // kClipboardPasteWarn,
    kClipboardCopyBlock,
    // kClipboardCopyWarn,
  };

  static void Show(content::WebContents* web_contents,
                   Type type,
                   base::OnceCallback<void(bool bypassed)> callback =
                       base::OnceCallback<void(bool bypassed)>());

  ~DataControlsDialog() override;

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;

 private:
  DataControlsDialog(Type type,
                     base::OnceCallback<void(bool bypassed)> callback);

  // Helpers to create sub-views of the dialog.
  std::unique_ptr<views::View> CreateEnterpriseIcon() const;
  std::unique_ptr<views::Label> CreateMessage() const;

  Type type_;
  raw_ptr<views::BoxLayoutView> contents_view_ = nullptr;

  // Called when the dialog closes, with `true` in the case of a bypassed
  // warning.
  base::OnceCallback<void(bool bypassed)> callback_;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_DIALOG_H_
