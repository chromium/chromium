// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_toast_container_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_keyboard_controller.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/feature_discovery_duration_reporter.h"
#include "ash/public/cpp/feature_discovery_metric_util.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/button/label_button.h"
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
    case AppListSortOrder::kAlphabeticalEphemeralAppFirst:
      NOTREACHED();
  }
}

constexpr auto kReorderUndoInteriorMargin = gfx::Insets::TLBR(8, 16, 8, 8);

}  // namespace

AppsGridContextMenu::GridType
AppListToastContainerView::Delegate::GetGridTypeForContextMenu() {
  return AppsGridContextMenu::GridType::kAppsGrid;
}

AppListToastContainerView::AppListToastContainerView(
    AppListNudgeController* nudge_controller,
    AppListKeyboardController* keyboard_controller,
    AppListA11yAnnouncer* a11y_announcer,
    AppListViewDelegate* view_delegate,
    Delegate* delegate,
    bool tablet_mode)
    : a11y_announcer_(a11y_announcer),
      tablet_mode_(tablet_mode),
      view_delegate_(view_delegate),
      delegate_(delegate),
      nudge_controller_(nudge_controller),
      keyboard_controller_(keyboard_controller),
      current_toast_(AppListToastType::kNone) {
  DCHECK(a11y_announcer_);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));

  if (!tablet_mode_) {
    // `context_menu_` is only set in clamshell mode. The sort options in tablet
    // mode are handled in RootWindowController with ShelfContextMenuModel.
    context_menu_ = std::make_unique<AppsGridContextMenu>(
        delegate->GetGridTypeForContextMenu());
    set_context_menu_controller(context_menu_.get());
  }
}

AppListToastContainerView::~AppListToastContainerView() {
  set_context_menu_controller(nullptr);
  toast_view_ = nullptr;
}

bool AppListToastContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  if (!delegate_ || !keyboard_controller_) {
    return false;
  }

  if (event.key_code() == ui::VKEY_UP)
    return keyboard_controller_->MoveFocusUpFromToast(focused_app_column_);

  if (event.key_code() == ui::VKEY_DOWN)
    return keyboard_controller_->MoveFocusDownFromToast(focused_app_column_);

  return false;
}

bool AppListToastContainerView::HandleFocus(int column) {
  // Only handle the focus if a button on the toast exists.
  views::LabelButton* toast_button = GetToastButton();
  if (toast_button) {
    focused_app_column_ = column;
    toast_button->RequestFocus();
    return true;
  }

  views::Button* close_button = GetCloseButton();
  if (close_button) {
    focused_app_column_ = column;
    close_button->RequestFocus();
    return true;
  }

  return false;
}

void AppListToastContainerView::DisableFocusForShowingActiveFolder(
    bool disabled) {
  if (auto* toast_button = GetToastButton())
    toast_button->SetEnabled(!disabled);
  if (auto* close_button = GetCloseButton())
    close_button->SetEnabled(!disabled);

  // Prevent items from being accessed by ChromeVox.
  SetViewIgnoredForAccessibility(this, disabled);
}

void AppListToastContainerView::MaybeUpdateReorderNudgeView() {
  // If the expect state in `nudge_controller_` is different from the one in
  // toast container, update the actual reorder nudge view in the toast
  // container.
  if (nudge_controller_->ShouldShowReorderNudge() &&
      current_toast_ != AppListToastType::kReorderNudge) {
    CreateReorderNudgeView();
  } else if (!nudge_controller_->ShouldShowReorderNudge() &&
             current_toast_ == AppListToastType::kReorderNudge) {
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

  AppListToastView::Builder toast_view_builder(
      l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_APP_LIST_REORDER_NUDGE_TITLE));

  toast_view_builder.SetButton(
      l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APP_LIST_REORDER_NUDGE_DISMISS_BUTTON),
      base::BindRepeating(&AppListToastContainerView::FadeOutToastView,
                          base::Unretained(this)));

  FeatureDiscoveryDurationReporter* reporter =
      FeatureDiscoveryDurationReporter::GetInstance();
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kAppListReorderAfterEducationNudge);
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::
          kAppListReorderAfterEducationNudgePerTabletMode);
  toast_view_ = AddChildView(
      toast_view_builder.SetStyleForTabletMode(tablet_mode_)
          .SetSubtitle(l10n_util::GetStringUTF16(subtitle_message_id))
          .SetIcon(
              ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
                  IDR_APP_LIST_SORT_NUDGE_IMAGE))
          .SetIconBackground(true)
          .Build());
  if (available_width_) {
    toast_view_->SetAvailableWidth(*available_width_);
  }
  current_toast_ = AppListToastType::kReorderNudge;
}

void AppListToastContainerView::CreateTutorialNudgeView() {
  if (toast_view_) {
    return;
  }

  AppListToastView::Builder toast_view_builder(
      l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_APPS_COLLECTIONS_NUDGE_TITLE));

  toast_view_builder
      .SetButton(
          l10n_util::GetStringUTF16(
              IDS_ASH_LAUNCHER_APPS_COLLECTIONS_NUDGE_DISMISS_BUTTON),
          base::BindRepeating(&AppListToastContainerView::FadeOutToastView,
                              base::Unretained(this)))
      .SetStyleForTabletMode(tablet_mode_)
      .SetSubtitle(l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_NUDGE_SUBTITLE))
      .SetIconBackground(true);

  toast_view_ = AddChildView(toast_view_builder.Build());
  toast_view_->GetViewAccessibility().SetRole(ax::mojom::Role::kRegion);
  toast_view_->toast_button()->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_NUDGE_DISMISS_BUTTON_SPOKEN_TEXT));
  if (available_width_) {
    toast_view_->SetAvailableWidth(*available_width_);
  }
  current_toast_ = AppListToastType::kTutorialViewNudge;
}

void AppListToastContainerView::RemoveReorderNudgeView() {
  // If the nudge is requested to be removed, it is likely that it won't be
  // shown to the user again. Therefore, the nudge child view is directly
  // removed instead of made invisible.
  if (current_toast_ == AppListToastType::kReorderNudge)
    RemoveCurrentView();
}

void AppListToastContainerView::RemoveCurrentView() {
  if (toast_view_)
    RemoveChildViewT(toast_view_.get());

  toast_view_ = nullptr;
  current_toast_ = AppListToastType::kNone;
}

void AppListToastContainerView::UpdateVisibilityState(VisibilityState state) {
  visibility_state_ = state;

  // Return early if the reorder nudge is not showing when the app list is
  // hiding.
  if (nudge_controller_->is_visible() &&
      (nudge_controller_->current_nudge() !=
       AppListNudgeController::NudgeType::kReorderNudge)) {
    return;
  }

  // Return early if the privacy notice should be showing.
  if (nudge_controller_->current_nudge() ==
      AppListNudgeController::NudgeType::kPrivacyNotice) {
    return;
  }

  AppListNudgeController::NudgeType new_nudge =
      AppListNudgeController::NudgeType::kNone;

  if (nudge_controller_->current_nudge() ==
      AppListNudgeController::NudgeType::kTutorialNudge) {
    new_nudge = AppListNudgeController::NudgeType::kTutorialNudge;
  } else if (nudge_controller_->ShouldShowReorderNudge()) {
    new_nudge = AppListNudgeController::NudgeType::kReorderNudge;
  }

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
    const std::optional<AppListSortOrder>& new_order) {
  // Remove `toast_view_` when the temporary sorting order is cleared.
  if (!GetVisibilityForSortOrder(new_order)) {
    if (committing_sort_order_) {
      // When the toast view is closed due to committing the sort order via the
      // close button , the toast view should be faded out with animation.
      FadeOutToastView();
    } else {
      RemoveCurrentView();
    }
    return;
  }

  // The nudge view should be removed when the user triggers apps reordering.
  RemoveReorderNudgeView();

  std::u16string toast_text = CalculateToastTextFromOrder(*new_order);
  const gfx::VectorIcon* toast_icon = GetToastIconForOrder(*new_order);
  const std::u16string a11y_text_on_undo_button =
      GetA11yTextOnUndoButtonFromOrder(*new_order);
  const ui::ColorId toast_icon_color_id = cros_tokens::kCrosSysOnSurface;

  if (toast_view_) {
    // If the reorder undo toast is showing, updates the title and icon of the
    // toast.
    toast_view_->SetTitle(toast_text);
    toast_view_->SetIcon(
        ui::ImageModel::FromVectorIcon(*toast_icon, toast_icon_color_id));
    toast_view_->toast_button()->GetViewAccessibility().SetName(
        a11y_text_on_undo_button, ax::mojom::NameFrom::kAttribute);
    return;
  }

  AppListToastView::Builder toast_view_builder(std::move(toast_text));

  toast_view_builder.SetCloseButton(base::BindRepeating(
      &AppListToastContainerView::OnReorderCloseButtonClicked,
      base::Unretained(this)));

  toast_view_ = AddChildView(
      toast_view_builder.SetStyleForTabletMode(tablet_mode_)
          .SetIcon(
              ui::ImageModel::FromVectorIcon(*toast_icon, toast_icon_color_id))
          .SetButton(l10n_util::GetStringUTF16(
                         IDS_ASH_LAUNCHER_UNDO_SORT_TOAST_ACTION_BUTTON),
                     base::BindRepeating(
                         &AppListToastContainerView::OnReorderUndoButtonClicked,
                         base::Unretained(this)))
          .SetViewDelegate(view_delegate_)
          .Build());
  toast_view_->toast_button()->GetViewAccessibility().SetName(
      a11y_text_on_undo_button, ax::mojom::NameFrom::kAttribute);

  toast_view_->UpdateInteriorMargins(kReorderUndoInteriorMargin);
  if (available_width_) {
    toast_view_->SetAvailableWidth(*available_width_);
  }
  current_toast_ = AppListToastType::kReorderUndo;
}

bool AppListToastContainerView::GetVisibilityForSortOrder(
    const std::optional<AppListSortOrder>& new_order) const {
  return new_order && *new_order != AppListSortOrder::kCustom &&
         *new_order != AppListSortOrder::kAlphabeticalEphemeralAppFirst;
}

void AppListToastContainerView::AnnounceSortOrder(AppListSortOrder new_order) {
  a11y_announcer_->Announce(CalculateToastTextFromOrder(new_order));
}

void AppListToastContainerView::AnnounceUndoSort() {
  a11y_announcer_->Announce(
      l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_UNDO_SORT_DONE_SPOKEN_TEXT));
}

void AppListToastContainerView::ConfigureLayoutForAvailableWidth(
    int available_width) {
  available_width_ = available_width;
  if (toast_view_) {
    toast_view_->SetAvailableWidth(available_width);
  }
}

views::LabelButton* AppListToastContainerView::GetToastButton() {
  if (!toast_view_)
    return nullptr;

  return toast_view_->toast_button();
}

views::Button* AppListToastContainerView::GetCloseButton() {
  if (!toast_view_)
    return nullptr;

  return toast_view_->close_button();
}

void AppListToastContainerView::OnReorderUndoButtonClicked() {
  toast_view_->toast_button()->SetEnabled(false);
  AppListModelProvider::Get()->model()->delegate()->RequestAppListSortRevert();
}

void AppListToastContainerView::OnReorderCloseButtonClicked() {
  // Prevent the close button from being clicked again during the fade out
  // animation.
  toast_view_->close_button()->SetEnabled(false);

  base::AutoReset auto_reset(&committing_sort_order_, true);
  AppListModelProvider::Get()
      ->model()
      ->delegate()
      ->RequestCommitTemporarySortOrder();
}

bool AppListToastContainerView::IsToastVisible() const {
  return toast_view_ && !(toast_view_->layer() &&
                          toast_view_->layer()->GetTargetOpacity() == 0.0f);
}

void AppListToastContainerView::FadeOutToastView() {
  views::AnimationBuilder builder;
  toast_view_fade_out_animation_abort_handle_ = builder.GetAbortHandle();
  if (!toast_view_) {
    // Aborting an existing fade out animation deletes the `toast_view_`, so
    // avoid creating new animations.
    return;
  }

  if (!toast_view_->layer()) {
    toast_view_->SetPaintToLayer();
    toast_view_->layer()->SetFillsBoundsOpaquely(false);
  }
  builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(
          base::BindOnce(&AppListToastContainerView::OnFadeOutToastViewComplete,
                         weak_factory_.GetWeakPtr()))
      .OnAborted(
          base::BindOnce(&AppListToastContainerView::OnFadeOutToastViewComplete,
                         weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(base::Milliseconds(200))
      .SetOpacity(toast_view_->layer(), 0.0f, gfx::Tween::LINEAR);
}

void AppListToastContainerView::OnFadeOutToastViewComplete() {
  if (current_toast_ == AppListToastType::kReorderNudge)
    nudge_controller_->OnReorderNudgeConfirmed();
  RemoveCurrentView();
  delegate_->OnNudgeRemoved();
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
    case AppListSortOrder::kAlphabeticalEphemeralAppFirst:
      NOTREACHED();
  }
}

std::u16string AppListToastContainerView::GetA11yTextOnUndoButtonFromOrder(
    AppListSortOrder order) const {
  switch (order) {
    case AppListSortOrder::kNameAlphabetical:
    case AppListSortOrder::kNameReverseAlphabetical:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_UNDO_NAME_SORT_TOAST_SPOKEN_TEXT);
    case AppListSortOrder::kColor:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_UNDO_COLOR_SORT_TOAST_SPOKEN_TEXT);
    case AppListSortOrder::kCustom:
    case AppListSortOrder::kAlphabeticalEphemeralAppFirst:
      NOTREACHED();
  }
}

BEGIN_METADATA(AppListToastContainerView)
END_METADATA

}  // namespace ash
