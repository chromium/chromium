// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_reorder_undo_container_view.h"

#include <memory>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

const gfx::VectorIcon* GetToastIconForOrder(AppListSortOrder order) {
  switch (order) {
    case AppListSortOrder::kNameAlphabetical:
    case AppListSortOrder::kNameReverseAlphabetical:
      return &kSortAlphabeticalIcon;
    case AppListSortOrder::kColor:
      return &kSortColorIcon;
    case AppListSortOrder::kCustom:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

AppListReorderUndoContainerView::AppListReorderUndoContainerView(
    bool tablet_mode)
    : tablet_mode_(tablet_mode) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::MinimumFlexSizeRule::kPreferred,
                      views::MaximumFlexSizeRule::kScaleToMaximum));
}

AppListReorderUndoContainerView::~AppListReorderUndoContainerView() {
  toast_view_ = nullptr;
}

void AppListReorderUndoContainerView::OnTemporarySortOrderChanged(
    const absl::optional<AppListSortOrder>& new_order) {
  // Remove `toast_view_` when the temporary sorting order is cleared.
  if (!new_order || *new_order == AppListSortOrder::kCustom) {
    RemoveChildView(toast_view_);
    delete toast_view_;
    toast_view_ = nullptr;
    return;
  }

  const std::u16string toast_text = CalculateToastTextFromOrder(*new_order);
  const gfx::VectorIcon* toast_icon = GetToastIconForOrder(*new_order);
  if (toast_view_) {
    toast_view_->SetTitle(toast_text);
    toast_view_->SetIcon(toast_icon);
    return;
  }

  toast_view_ = AddChildView(
      AppListToastView::Builder(toast_text)
          .SetStyleForTabletMode(tablet_mode_)
          .SetIcon(toast_icon)
          .SetButton(
              l10n_util::GetStringUTF16(
                  IDS_ASH_LAUNCHER_UNDO_SORT_TOAST_ACTION_BUTTON),
              base::BindRepeating(
                  &AppListReorderUndoContainerView::OnReorderUndoButtonClicked,
                  base::Unretained(this)))
          .Build());
}

views::LabelButton*
AppListReorderUndoContainerView::GetToastDismissButtonForTest() {
  return toast_view_->toast_button();
}

void AppListReorderUndoContainerView::OnReorderUndoButtonClicked() {
  AppListModelProvider::Get()->model()->delegate()->RequestAppListSortRevert();
}

std::u16string AppListReorderUndoContainerView::CalculateToastTextFromOrder(
    AppListSortOrder order) const {
  switch (order) {
    case AppListSortOrder::kNameAlphabetical:
    case AppListSortOrder::kNameReverseAlphabetical:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_UNDO_SORT_TOAST_FOR_NAME_SORT);
    case AppListSortOrder::kColor:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_UNDO_SORT_TOAST_FOR_COLOR_SORT);
    case AppListSortOrder::kCustom:
      NOTREACHED();
      return u"";
  }
}

}  // namespace ash
