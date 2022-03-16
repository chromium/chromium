// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_toast_container_view.h"

#include <memory>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
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

constexpr gfx::Insets kReorderUndoInteriorMargin(8, 16, 8, 8);

}  // namespace

AppListToastContainerView::AppListToastContainerView(
    AppListNudgeController* nudge_controller,
    AppListA11yAnnouncer* a11y_announcer,
    bool tablet_mode)
    : a11y_announcer_(a11y_announcer),
      tablet_mode_(tablet_mode),
      nudge_controller_(nudge_controller) {
  DCHECK(a11y_announcer_);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));

  context_menu_ = std::make_unique<AppsGridContextMenu>();
  set_context_menu_controller(context_menu_.get());
}

AppListToastContainerView::~AppListToastContainerView() {
  toast_view_ = nullptr;
}

void AppListToastContainerView::MaybeUpdateReorderNudgeView() {
  // If the expect state in `nudge_controller_` is different from the one in
  // toast container, update the actual reorder nudge view in the toast
  // container.
  if (nudge_controller_->ShouldShowReorderNudge() &&
      current_toast_ != ToastType::kReorderNudge) {
    CreateReorderNudgeView();
  } else if (!nudge_controller_->ShouldShowReorderNudge() &&
             current_toast_ == ToastType::kReorderNudge) {
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
          .SetThemingIcons(tablet_mode_ ? &kReorderNudgeDarkTabletIcon
                                        : &kReorderNudgeDarkClamshellIcon,
                           tablet_mode_ ? &kReorderNudgeLightTabletIcon
                                        : &kReorderNudgeLightClamshellIcon)
          .SetIconBackground(true)
          .Build());
  current_toast_ = ToastType::kReorderNudge;
}

void AppListToastContainerView::RemoveReorderNudgeView() {
  // If the nudge is requested to be removed, it is likely that it won't be
  // shown to the user again. Therefore, the nudge child view is directly
  // removed instead of made invisible.
  if (current_toast_ == ToastType::kReorderNudge)
    RemoveCurrentView();
}

void AppListToastContainerView::RemoveCurrentView() {
  if (toast_view_) {
    RemoveChildViewT(toast_view_);
  }
  toast_view_ = nullptr;
  current_toast_ = ToastType::kNone;
}

void AppListToastContainerView::UpdateVisibilityState(VisibilityState state) {
  visibility_state_ = state;

  // Return early if the reorder nudge is not showing when the app list is
  // hiding.
  if (nudge_controller_->is_visible() &&
      nudge_controller_->current_nudge() !=
          AppListNudgeController::NudgeType::kReorderNudge) {
    return;
  }

  // Return early if the privacy notice should be showing.
  if (nudge_controller_->current_nudge() ==
      AppListNudgeController::NudgeType::kPrivacyNotice) {
    return;
  }

  AppListNudgeController::NudgeType new_nudge =
      nudge_controller_->ShouldShowReorderNudge()
          ? AppListNudgeController::NudgeType::kReorderNudge
          : AppListNudgeController::NudgeType::kNone;

  // Update the visible and active state in `nudge_controller_`.
  switch (state) {
    case VisibilityState::kShown:
      nudge_controller_->SetNudgeVisible(true, new_nudge);
      break;
    case VisibilityState::kShownInBackground:
      // The nudge must be visible to change to inactive state.
      if (!nudge_controller_->is_visible())
        nudge_controller_->SetNudgeVisible(true, new_nudge);
      nudge_controller_->SetNudgeActive(false, new_nudge);
      break;
    case VisibilityState::kHidden:
      nudge_controller_->SetNudgeVisible(false, new_nudge);
      break;
  }
}

void AppListToastContainerView::OnTemporarySortOrderChanged(
    const absl::optional<AppListSortOrder>& new_order) {
  // Remove `toast_view_` when the temporary sorting order is cleared.
  if (!GetVisibilityForSortOrder(new_order)) {
    RemoveCurrentView();
    return;
  }

  // The nudge view should be removed when the user triggers apps reordering.
  RemoveReorderNudgeView();

  const std::u16string toast_text = CalculateToastTextFromOrder(*new_order);
  const gfx::VectorIcon* toast_icon = GetToastIconForOrder(*new_order);

  if (toast_view_) {
    // If the reorder undo toast is showing, updates the title and icon of the
    // toast.
    toast_view_->SetTitle(toast_text);
    toast_view_->SetIcon(toast_icon);
    return;
  }

  toast_view_ = AddChildView(
      AppListToastView::Builder(toast_text)
          .SetStyleForTabletMode(tablet_mode_)
          .SetIcon(toast_icon)
          .SetButton(l10n_util::GetStringUTF16(
                         IDS_ASH_LAUNCHER_UNDO_SORT_TOAST_ACTION_BUTTON),
                     base::BindRepeating(
                         &AppListToastContainerView::OnReorderUndoButtonClicked,
                         base::Unretained(this)))
          .Build());
  toast_view_->UpdateInteriorMargins(kReorderUndoInteriorMargin);
  current_toast_ = ToastType::kReorderUndo;
}

bool AppListToastContainerView::GetVisibilityForSortOrder(
    const absl::optional<AppListSortOrder>& new_order) const {
  return new_order && *new_order != AppListSortOrder::kCustom;
}

void AppListToastContainerView::AnnounceSortOrder(AppListSortOrder new_order) {
  a11y_announcer_->Announce(CalculateToastTextFromOrder(new_order));
}

views::LabelButton* AppListToastContainerView::GetToastDismissButtonForTest() {
  if (!toast_view_ || current_toast_ != ToastType::kReorderUndo)
    return nullptr;

  return toast_view_->toast_button();
}

void AppListToastContainerView::OnReorderUndoButtonClicked() {
  AppListModelProvider::Get()->model()->delegate()->RequestAppListSortRevert();
}

std::u16string AppListToastContainerView::CalculateToastTextFromOrder(
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
