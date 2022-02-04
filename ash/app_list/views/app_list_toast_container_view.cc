// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_toast_container_view.h"

#include <memory>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/strcat.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

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
constexpr char16_t kToastDismissText[] = u"Undo";

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

AppListToastContainerView::AppListToastContainerView(
    AppListNudgeController* nudge_controller,
    bool tablet_mode)
    : tablet_mode_(tablet_mode), nudge_controller_(nudge_controller) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::MinimumFlexSizeRule::kPreferred,
                      views::MaximumFlexSizeRule::kScaleToMaximum));
}

AppListToastContainerView::~AppListToastContainerView() {
  toast_view_ = nullptr;
}

void AppListToastContainerView::MaybeUpdateReorderNudgeView() {
  // If the expect state in `nudge_controller_` is different from the one in
  // toast container, update the actual reorder nudge view in the toast
  // container.
  if (nudge_controller_->ShouldShowReorderNudge() &&
      current_toast_ != AppListToastContainerView::kReorderNudge) {
    CreateReorderNudgeView();
  } else if (!nudge_controller_->ShouldShowReorderNudge() &&
             current_toast_ == AppListToastContainerView::kReorderNudge) {
    RemoveReorderNudgeView();
  }
}

void AppListToastContainerView::CreateReorderNudgeView() {
  if (toast_view_)
    return;

  int subtitle_message_id =
      tablet_mode_
          ? IDS_ASH_LAUNCHER_APP_LIST_REORDER_NUDGE_TABLET_MODE_SUBTITLE
          : IDS_ASH_LAUNCHER_APP_LIST_REORDER_NUDGE_CLAMSHELL_MODE_SUBTITLE;

  toast_view_ = AddChildView(
      AppListToastView::Builder(
          l10n_util::GetStringUTF16(
              IDS_ASH_LAUNCHER_APP_LIST_REORDER_NUDGE_TITLE))
          .SetStyleForTabletMode(tablet_mode_)
          .SetSubtitle(l10n_util::GetStringUTF16(subtitle_message_id))
          .SetThemingIcons(&kReorderNudgeDarkIcon, &kReorderNudgeLightIcon)
          .Build());
  current_toast_ = kReorderNudge;
}

void AppListToastContainerView::RemoveReorderNudgeView() {
  // If the nudge is requested to be removed, it is likely that it won't be
  // shown to the user again. Therefore, the nudge child view is directly
  // removed instead of made invisible.
  if (current_toast_ == kReorderNudge)
    RemoveCurrentView();
}

void AppListToastContainerView::RemoveCurrentView() {
  if (toast_view_)
    RemoveChildView(toast_view_);
  toast_view_ = nullptr;
  current_toast_ = kNone;
}

void AppListToastContainerView::UpdateVisibilityState(VisibilityState state) {
  visibility_state_ = state;

  AppListNudgeController::NudgeType new_nudge =
      nudge_controller_->ShouldShowReorderNudge()
          ? AppListNudgeController::NudgeType::kReorderNudge
          : AppListNudgeController::NudgeType::kNone;

  // Update the visible and active state in `nudge_controller_`.
  switch (state) {
    case kShown:
      nudge_controller_->SetNudgeVisible(true, new_nudge);
      break;
    case kShownInBackground:
      // The nudge must be visible to change to inactive state.
      DCHECK(nudge_controller_->is_visible());
      nudge_controller_->SetNudgeActive(false, new_nudge);
      break;
    case kHidden:
      nudge_controller_->SetNudgeVisible(false, new_nudge);
      break;
  }
}

void AppListToastContainerView::OnTemporarySortOrderChanged(
    const absl::optional<AppListSortOrder>& new_order) {
  // Remove `toast_view_` when the temporary sorting order is cleared.
  if (!new_order) {
    RemoveCurrentView();
    return;
  }

  // The nudge view should be removed when the user triggers apps reordering.
  RemoveReorderNudgeView();

  if (toast_view_)
    return;

  const std::u16string toast_text = CalculateToastTextFromOrder(*new_order);
  const gfx::VectorIcon* toast_icon = GetToastIconForOrder(*new_order);

  // TODO(crbug.com/1277001): Add icon to the toast.
  toast_view_ = AddChildView(
      AppListToastView::Builder(toast_text)
          .SetStyleForTabletMode(tablet_mode_)
          .SetIcon(toast_icon)
          .SetButton(kToastDismissText,
                     base::BindRepeating(
                         &AppListToastContainerView::OnReorderUndoButtonClicked,
                         base::Unretained(this)))
          .Build());
  current_toast_ = kReorderUndo;
}

views::LabelButton* AppListToastContainerView::GetToastDismissButtonForTest() {
  if (!toast_view_ || current_toast_ != kReorderUndo)
    return nullptr;

  return toast_view_->toast_button();
}

void AppListToastContainerView::OnReorderUndoButtonClicked() {
  AppListModelProvider::Get()->model()->delegate()->RequestAppListSortRevert();
}

std::u16string AppListToastContainerView::CalculateToastTextFromOrder(
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
