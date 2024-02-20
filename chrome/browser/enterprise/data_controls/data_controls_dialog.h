// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_DIALOG_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_DIALOG_H_

#include <vector>

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
    kClipboardPasteWarn,
    kClipboardCopyBlock,
    kClipboardCopyWarn,
  };

  // Test observer to validate the dialog was shown/closed at appropriate
  // timings, which buttons were pressed, etc. Only one `TestObserver` should be
  // instantiated per test.
  class TestObserver {
   public:
    TestObserver();
    ~TestObserver();

    // Called as the last statement in the DataControlsDialog constructor.
    virtual void OnConstructed(DataControlsDialog* dialog) {}

    // Called when OnWidgetInitialized is called. This is used to give the test
    // a proper hook to close the dialog after it's first shown.
    virtual void OnWidgetInitialized(DataControlsDialog* dialog) {}

    // Called as the last statement in the DataControlsDialog destructor. As
    // such, do not keep `dialog` after this function returns, only use it
    // locally to validate test assertions.
    virtual void OnDestructed(DataControlsDialog* dialog) {}
  };
  static void SetObserverForTesting(TestObserver* observer);

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
  void OnWidgetInitialized() override;

  Type type() const;

 private:
  DataControlsDialog(Type type,
                     content::WebContents* web_contents,
                     base::OnceCallback<void(bool bypassed)> callback);

  // Calls `callbacks_` with the value in `bypasses`. "true" represents the user
  // ignoring a warning and proceeding with the action, "false" corresponds to
  // the user cancelling their action after seeing a warning. This should not be
  // called for blocking dialogs.
  void OnDialogButtonClicked(bool bypassed);

  // Helpers to create sub-views of the dialog.
  std::unique_ptr<views::View> CreateEnterpriseIcon() const;
  std::unique_ptr<views::Label> CreateMessage() const;

  Type type_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<views::BoxLayoutView> contents_view_ = nullptr;

  // Called when the dialog closes, with `true` in the case of a bypassed
  // warning.
  std::vector<base::OnceCallback<void(bool bypassed)>> callbacks_;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_DIALOG_H_
