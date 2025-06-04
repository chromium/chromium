// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_DELEGATE_H_

#include "ui/views/window/dialog_delegate.h"

namespace views {
class BoxLayoutView;
}  // namespace views

namespace enterprise_connectors {

// Implementation of `views::DialogDelegate` used to show a user the state of
// content analysis triggered by one of their action.
class ContentAnalysisDialogDelegate : public views::DialogDelegate {
 public:
  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  ui::mojom::ModalType GetModalType() const override;

 protected:
  raw_ptr<views::BoxLayoutView> contents_view_ = nullptr;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_DELEGATE_H_
