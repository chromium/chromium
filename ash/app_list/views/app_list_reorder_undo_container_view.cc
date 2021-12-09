// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_reorder_undo_container_view.h"

#include <memory>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/style/system_toast_style.h"
#include "base/strings/strcat.h"

namespace ash {

namespace {

// TODO(https://crbug.com/1269386): Raw strings are used for now. It should be
// replaced by an i18n string after the ui design is finalized.

// The toast text's fixed part that is independent of the sorting order.
constexpr char16_t kToastTextFixedPart[] = u"Apps are now reordered ";

// The toast texts that depend on the sorting order.
constexpr char16_t kToastAlphabeticalOrderText[] = u"alphabetically";
constexpr char16_t kToastReverseAlphabeticalOrderText[] =
    u"reverse-alphabetically";
constexpr char16_t kToastColorOrderText[] = u"by color";

// The text shown on the toast dismiss button.
constexpr char16_t kToastDismissText[] = u"Redo";

}  // namespace

AppListReorderUndoContainerView::AppListReorderUndoContainerView() {
  SetUseDefaultFillLayout(true);
}

AppListReorderUndoContainerView::~AppListReorderUndoContainerView() {
  toast_view_ = nullptr;
}

void AppListReorderUndoContainerView::OnTemporarySortOrderChanged(
    const absl::optional<AppListSortOrder>& new_order) {
  // Remove `toast_view_` when the temporary sorting order is cleared.
  if (!new_order) {
    RemoveChildView(toast_view_);
    toast_view_ = nullptr;
    return;
  }

  const std::u16string toast_text = CalculateToastTextFromOrder(*new_order);
  if (toast_view_) {
    toast_view_->SetText(toast_text);
    return;
  }

  toast_view_ = AddChildView(std::make_unique<SystemToastStyle>(
      base::BindRepeating(
          &AppListReorderUndoContainerView::OnReorderUndoButtonClicked,
          base::Unretained(this)),
      toast_text, kToastDismissText,
      /*is_managed=*/false));
}

views::LabelButton*
AppListReorderUndoContainerView::GetToastDismissButtonForTest() {
  return toast_view_->button();
}

void AppListReorderUndoContainerView::OnReorderUndoButtonClicked() {
  AppListModelProvider::Get()->model()->delegate()->RequestAppListSortRevert();
}

std::u16string AppListReorderUndoContainerView::CalculateToastTextFromOrder(
    AppListSortOrder order) const {
  base::StringPiece16 toast_text_variable_part;

  switch (order) {
    case AppListSortOrder::kNameAlphabetical:
      toast_text_variable_part =
          base::StringPiece16(kToastAlphabeticalOrderText);
      break;
    case AppListSortOrder::kNameReverseAlphabetical:
      toast_text_variable_part =
          base::StringPiece16(kToastReverseAlphabeticalOrderText);
      break;
    case AppListSortOrder::kColor:
      toast_text_variable_part = base::StringPiece16(kToastColorOrderText);
      break;
    case AppListSortOrder::kCustom:
      NOTREACHED();
      break;
  }

  return base::StrCat(
      {base::StringPiece16(kToastTextFixedPart), toast_text_variable_part});
}

}  // namespace ash
