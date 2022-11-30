// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_page_dialog_controller.h"

#include <utility>

#include "ash/app_list/views/search_result_page_anchored_dialog.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

SearchResultPageDialogController::SearchResultPageDialogController(
    views::View* host_view)
    : host_view_(host_view) {}

SearchResultPageDialogController::~SearchResultPageDialogController() = default;

void SearchResultPageDialogController::Show(
    std::unique_ptr<views::WidgetDelegate> dialog) {
  if (!enabled_)
    return;

  dialog_ = std::make_unique<SearchResultPageAnchoredDialog>(
      std::move(dialog), host_view_,
      base::BindOnce(&SearchResultPageDialogController::OnAnchoredDialogClosed,
                     base::Unretained(this)));
  dialog_->UpdateBounds();
  dialog_->widget()->Show();
}

void SearchResultPageDialogController::Reset(bool enabled) {
  enabled_ = enabled;
  dialog_.reset();
}

void SearchResultPageDialogController::OnAnchoredDialogClosed() {
  dialog_.reset();
}

}  // namespace ash
