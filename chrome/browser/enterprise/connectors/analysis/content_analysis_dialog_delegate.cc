// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_delegate.h"

#include "ui/views/layout/box_layout_view.h"

namespace enterprise_connectors {

std::u16string ContentAnalysisDialogDelegate::GetWindowTitle() const {
  return std::u16string();
}

bool ContentAnalysisDialogDelegate::ShouldShowCloseButton() const {
  return false;
}

views::Widget* ContentAnalysisDialogDelegate::GetWidget() {
  return contents_view_->GetWidget();
}

const views::Widget* ContentAnalysisDialogDelegate::GetWidget() const {
  return contents_view_->GetWidget();
}

ui::mojom::ModalType ContentAnalysisDialogDelegate::GetModalType() const {
  return ui::mojom::ModalType::kChild;
}

}  // namespace enterprise_connectors
