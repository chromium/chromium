// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_tray_bubble.h"
#include "ash/system/holding_space/holding_space_tray_icon.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "base/containers/adapters.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns whether previews are enabled.
bool IsPreviewsEnabled() {
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  return features::IsTemporaryHoldingSpacePreviewsEnabled() && prefs &&
         holding_space_prefs::IsPreviewsEnabled(prefs);
}

// Returns whether the holding space model contains any finalized items.
bool ModelContainsFinalizedItems(HoldingSpaceModel* model) {
  for (const auto& item : model->items()) {
    if (item->IsFinalized())
      return true;
  }
  return false;
}

std::unique_ptr<views::ImageView> CreateDefaultTrayIcon() {
  auto icon = std::make_unique<views::ImageView>();
  icon->SetID(kHoldingSpaceTrayDefaultIconId);
  icon->SetImage(gfx::CreateVectorIcon(
      kHoldingSpaceIcon, kHoldingSpaceTrayIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  icon->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));
  return icon;
}

}  // namespace

// HoldingSpaceTray ------------------------------------------------------------

HoldingSpaceTray::HoldingSpaceTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  controller_observer_.Observe(HoldingSpaceController::Get());
  SetVisible(false);

  // Icon.
  default_tray_icon_ = tray_container()->AddChildView(CreateDefaultTrayIcon());

  if (features::IsTemporaryHoldingSpacePreviewsEnabled()) {
    previews_tray_icon_ = tray_container()->AddChildView(
        std::make_unique<HoldingSpaceTrayIcon>(shelf));
    previews_tray_icon_->SetVisible(false);
    UpdatePreviewsVisibility();

    // If previews feature is enabled, the preview icon is displayed
    // conditionally, depending on user prefs state.
    session_observer_.Observe(Shell::Get()->session_controller());
    auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
    if (prefs)
      ObservePrefService(prefs);

    // Enable context menu, which supports an action to toggle item previews.
    set_context_menu_controller(this);
  }

  // It's possible that this holding space tray was created after login, such as
  // would occur if the user connects an external display. In such situations
  // the holding space model will already have been attached.
  if (HoldingSpaceController::Get()->model())
    OnHoldingSpaceModelAttached(HoldingSpaceController::Get()->model());

  set_use_bounce_in_animation(true);
}

HoldingSpaceTray::~HoldingSpaceTray() = default;

void HoldingSpaceTray::ClickedOutsideBubble() {
  CloseBubble();
}

base::string16 HoldingSpaceTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_A11Y_NAME);
}

views::View* HoldingSpaceTray::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tooltip events should be handled top level, not by descendents.
  return HitTestPoint(point) ? this : nullptr;
}

base::string16 HoldingSpaceTray::GetTooltipText(const gfx::Point& point) const {
  return l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE);
}

void HoldingSpaceTray::HandleLocaleChange() {
  TooltipTextChanged();
}

void HoldingSpaceTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {}

void HoldingSpaceTray::AnchorUpdated() {
  if (bubble_)
    bubble_->AnchorUpdated();
}

void HoldingSpaceTray::UpdateAfterLoginStatusChange() {
  UpdateVisibility();
}

bool HoldingSpaceTray::PerformAction(const ui::Event& event) {
  if (bubble_) {
    CloseBubble();
    return true;
  }

  ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());

  // Activate the bubble for a11y or if it was shown via keypress. Otherwise
  // focus will remain on the tray when it should enter the bubble.
  if (event.IsKeyEvent() ||
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    DCHECK(bubble_ && bubble_->GetBubbleWidget());
    bubble_->GetBubbleWidget()->widget_delegate()->SetCanActivate(true);
    bubble_->GetBubbleWidget()->Activate();
  }

  return true;
}

void HoldingSpaceTray::CloseBubble() {
  if (!bubble_)
    return;

  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kCloseBubble);

  widget_observer_.Reset();

  bubble_.reset();
  SetIsActive(false);
}

void HoldingSpaceTray::ShowBubble(bool show_by_click) {
  if (bubble_)
    return;

  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kShowBubble);

  DCHECK(tray_container());

  bubble_ = std::make_unique<HoldingSpaceTrayBubble>(this, show_by_click);

  // Observe the bubble widget so that we can do proper clean up when it is
  // being destroyed. If destruction is due to a call to `CloseBubble()` we will
  // have already cleaned up state but there are cases where the bubble widget
  // is destroyed independent of a call to `CloseBubble()`, e.g. ESC key press.
  widget_observer_.Observe(bubble_->GetBubbleWidget());

  SetIsActive(true);
}

TrayBubbleView* HoldingSpaceTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

void HoldingSpaceTray::SetVisiblePreferred(bool preferred_visibility) {
  if (visible_preferred() != preferred_visibility) {
    holding_space_metrics::RecordPodAction(
        preferred_visibility ? holding_space_metrics::PodAction::kShowPod
                             : holding_space_metrics::PodAction::kHidePod);
    TrayBackgroundView::SetVisiblePreferred(preferred_visibility);
  }
}

void HoldingSpaceTray::FirePreviewsUpdateTimerIfRunningForTesting() {
  if (previews_update_.IsRunning())
    previews_update_.FireNow();
}

void HoldingSpaceTray::UpdateVisibility() {
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  LoginStatus login_status = shelf()->GetStatusAreaWidget()->login_status();
  const bool in_active_session = login_status != LoginStatus::NOT_LOGGED_IN &&
                                 login_status != LoginStatus::LOCKED;
  if (!model || !in_active_session) {
    SetVisiblePreferred(false);
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  const bool has_ever_added_item =
      prefs ? holding_space_prefs::GetTimeOfFirstAdd(prefs).has_value() : false;
  const bool has_ever_pinned_item =
      prefs ? holding_space_prefs::GetTimeOfFirstPin(prefs).has_value() : false;

  // The holding space tray should not be visible in the shelf until the user
  // has added their first item to holding space. Once an item has been added,
  // the holding space tray will continue to be visible until the user has
  // pinned their first file. After the user has pinned their first file, the
  // holding space tray will only be visible in the shelf if their holding space
  // contains finalized items.
  SetVisiblePreferred((has_ever_added_item && !has_ever_pinned_item) ||
                      ModelContainsFinalizedItems(model));
}

base::string16 HoldingSpaceTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool HoldingSpaceTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void HoldingSpaceTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void HoldingSpaceTray::OnHoldingSpaceModelAttached(HoldingSpaceModel* model) {
  model_observer_.Observe(model);
  UpdateVisibility();
  UpdatePreviewsState();
}

void HoldingSpaceTray::OnHoldingSpaceModelDetached(HoldingSpaceModel* model) {
  model_observer_.Reset();
  UpdateVisibility();
  UpdatePreviewsState();
}

void HoldingSpaceTray::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  UpdateVisibility();
  UpdatePreviewsState();
}

void HoldingSpaceTray::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  UpdateVisibility();
  UpdatePreviewsState();
}

void HoldingSpaceTray::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  UpdateVisibility();
  UpdatePreviewsState();
}

void HoldingSpaceTray::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());
  switch (command_id) {
    case HoldingSpaceCommandId::kHidePreviews:
      holding_space_metrics::RecordPodAction(
          holding_space_metrics::PodAction::kHidePreviews);

      holding_space_prefs::SetPreviewsEnabled(
          Shell::Get()->session_controller()->GetActivePrefService(), false);
      break;
    case HoldingSpaceCommandId::kShowPreviews:
      holding_space_metrics::RecordPodAction(
          holding_space_metrics::PodAction::kShowPreviews);

      holding_space_prefs::SetPreviewsEnabled(
          Shell::Get()->session_controller()->GetActivePrefService(), true);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void HoldingSpaceTray::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kShowContextMenu);

  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  const bool previews_enabled = holding_space_prefs::IsPreviewsEnabled(
      Shell::Get()->session_controller()->GetActivePrefService());

  if (previews_enabled) {
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kHidePreviews,
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_HIDE_PREVIEWS),
        ui::ImageModel::FromVectorIcon(kVisibilityOffIcon));
  } else {
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kShowPreviews,
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_SHOW_PREVIEWS),
        ui::ImageModel::FromVectorIcon(kVisibilityIcon));
  }

  const int run_types = views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR;

  context_menu_runner_ =
      std::make_unique<views::MenuRunner>(context_menu_model_.get(), run_types);

  gfx::Rect anchor = source->GetBoundsInScreen();
  anchor.Inset(gfx::Insets(-kHoldingSpaceContextMenuMargin, 0));

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), /*button_controller=*/nullptr, anchor,
      views::MenuAnchorPosition::kTopLeft, source_type);
}

void HoldingSpaceTray::OnWidgetDragWillStart(views::Widget* widget) {
  // The holding space bubble should be closed while dragging holding space
  // items so as not to obstruct drop targets. Post the task to close the bubble
  // so that we don't attempt to destroy the bubble widget before the associated
  // drag event has been fully initialized.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&HoldingSpaceTray::CloseBubble,
                                weak_factory_.GetWeakPtr()));
}

void HoldingSpaceTray::OnWidgetDestroying(views::Widget* widget) {
  CloseBubble();
}

void HoldingSpaceTray::OnActiveUserPrefServiceChanged(PrefService* prefs) {
  UpdatePreviewsState();
  ObservePrefService(prefs);
}

void HoldingSpaceTray::ObservePrefService(PrefService* prefs) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  // NOTE: The callback being bound is scoped to `pref_change_registrar_` which
  // is owned by `this` so it is safe to bind with an unretained raw pointer.
  holding_space_prefs::AddPreviewsEnabledChangedCallback(
      pref_change_registrar_.get(),
      base::BindRepeating(&HoldingSpaceTray::UpdatePreviewsState,
                          base::Unretained(this)));
}

void HoldingSpaceTray::UpdatePreviewsState() {
  UpdatePreviewsVisibility();
  SchedulePreviewsIconUpdate();
}

void HoldingSpaceTray::UpdatePreviewsVisibility() {
  const bool show_previews =
      IsPreviewsEnabled() && HoldingSpaceController::Get()->model() &&
      ModelContainsFinalizedItems(HoldingSpaceController::Get()->model());

  if (PreviewsShown() == show_previews)
    return;
  default_tray_icon_->SetVisible(!show_previews);

  DCHECK(previews_tray_icon_);
  previews_tray_icon_->SetVisible(show_previews);

  if (!show_previews)
    previews_update_.Stop();
}

void HoldingSpaceTray::SchedulePreviewsIconUpdate() {
  if (previews_update_.IsRunning())
    return;

  // Schedule async task with a short (somewhat arbitrary) delay to update
  // previews so items added in quick succession are handled together.
  base::TimeDelta delay = use_zero_previews_update_delay_
                              ? base::TimeDelta()
                              : base::TimeDelta::FromMilliseconds(50);
  previews_update_.Start(FROM_HERE, delay,
                         base::BindOnce(&HoldingSpaceTray::UpdatePreviewsIcon,
                                        base::Unretained(this)));
}

void HoldingSpaceTray::UpdatePreviewsIcon() {
  if (!PreviewsShown()) {
    if (previews_tray_icon_)
      previews_tray_icon_->Clear();
    return;
  }

  std::vector<const HoldingSpaceItem*> items_with_previews;
  std::set<base::FilePath> paths_with_previews;
  for (const auto& item :
       base::Reversed(HoldingSpaceController::Get()->model()->items())) {
    if (!item->IsFinalized())
      continue;
    if (base::Contains(paths_with_previews, item->file_path()))
      continue;
    items_with_previews.push_back(item.get());
    paths_with_previews.insert(item->file_path());
  }
  previews_tray_icon_->UpdatePreviews(items_with_previews);
}

bool HoldingSpaceTray::PreviewsShown() const {
  return previews_tray_icon_ && previews_tray_icon_->GetVisible();
}

BEGIN_METADATA(HoldingSpaceTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
