// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
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
#include "base/pickle.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/vector_icons.h"

namespace ash {
namespace {

using ::ui::mojom::DragOperation;

// Animation.
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(167);

// Helpers ---------------------------------------------------------------------

// Animates the specified `view` to the given `target_opacity`.
void AnimateToTargetOpacity(views::View* view, float target_opacity) {
  DCHECK(view->layer());
  if (view->layer()->GetTargetOpacity() == target_opacity)
    return;

  ui::ScopedLayerAnimationSettings settings(view->layer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(kAnimationDuration);

  view->layer()->SetOpacity(target_opacity);
}

// Returns the file paths extracted from the specified `data` at the filenames
// storage location. The Files app stores file paths but *not* directory paths
// at this location.
std::vector<base::FilePath> ExtractFilePathsFromFilenames(
    const ui::OSExchangeData& data) {
  if (!data.HasFile())
    return {};

  std::vector<ui::FileInfo> filenames;
  if (!data.GetFilenames(&filenames))
    return {};

  std::vector<base::FilePath> result;
  for (const ui::FileInfo& filename : filenames)
    result.push_back(base::FilePath(filename.path));

  return result;
}

// Returns the file paths extracted from the specified `data` at the file system
// sources storage location. The Files app stores both file paths *and*
// directory paths at this location.
std::vector<base::FilePath> ExtractFilePathsFromFileSystemSources(
    const ui::OSExchangeData& data) {
  base::Pickle pickle;
  if (!data.GetPickledData(ui::ClipboardFormatType::GetWebCustomDataType(),
                           &pickle)) {
    return {};
  }

  constexpr char kFileSystemSourcesType[] = "fs/sources";

  std::u16string file_system_sources;
  ui::ReadCustomDataForType(pickle.data(), pickle.size(),
                            base::UTF8ToUTF16(kFileSystemSourcesType),
                            &file_system_sources);
  if (file_system_sources.empty())
    return {};

  HoldingSpaceClient* const client = HoldingSpaceController::Get()->client();

  std::vector<base::FilePath> result;
  for (const base::StringPiece16& file_system_source : base::SplitStringPiece(
           file_system_sources, base::UTF8ToUTF16("\n"), base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath file_path =
        client->CrackFileSystemUrl(GURL(file_system_source));
    if (!file_path.empty())
      result.push_back(file_path);
  }

  return result;
}

// Returns the file paths extracted from the specified `data` which are *not*
// already pinned to the attached holding space model.
std::vector<base::FilePath> ExtractUnpinnedFilePaths(
    const ui::OSExchangeData& data) {
  // Prefer extracting file paths from file system sources when possible. The
  // Files app populates both file system sources and filenames storage
  // locations, but only the former contains directory paths.
  std::vector<base::FilePath> unpinned_file_paths =
      ExtractFilePathsFromFileSystemSources(data);
  if (unpinned_file_paths.empty())
    unpinned_file_paths = ExtractFilePathsFromFilenames(data);

  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  base::EraseIf(unpinned_file_paths, [model](const base::FilePath& file_path) {
    return model->ContainsItem(HoldingSpaceItem::Type::kPinnedFile, file_path);
  });

  return unpinned_file_paths;
}

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

// Creates the default tray icon.
std::unique_ptr<views::ImageView> CreateDefaultTrayIcon() {
  auto icon = std::make_unique<views::ImageView>();
  icon->SetID(kHoldingSpaceTrayDefaultIconId);
  icon->SetImage(gfx::CreateVectorIcon(
      kHoldingSpaceIcon, kHoldingSpaceTrayIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  icon->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));
  icon->SetPaintToLayer();
  icon->layer()->SetFillsBoundsOpaquely(false);
  return icon;
}

// Creates the overlay to be drawn over all other child views to indicate that
// the parent view is a drop target and is capable of handling the current drag
// payload.
std::unique_ptr<views::View> CreateDropTargetOverlay() {
  auto drop_target_overlay = std::make_unique<views::View>();
  drop_target_overlay->SetID(kHoldingSpaceTrayDropTargetOverlayId);
  drop_target_overlay->SetLayoutManager(std::make_unique<views::FillLayout>());

  // Layer.
  drop_target_overlay->SetPaintToLayer();
  drop_target_overlay->layer()->SetFillsBoundsOpaquely(false);

  // Icon.
  auto* icon =
      drop_target_overlay->AddChildView(std::make_unique<views::ImageView>());
  icon->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  icon->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  icon->SetImage(gfx::CreateVectorIcon(
      views::kUnpinIcon, kHoldingSpaceIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  icon->SetPaintToLayer();
  icon->layer()->SetFillsBoundsOpaquely(false);

  return drop_target_overlay;
}

}  // namespace

// HoldingSpaceTray ------------------------------------------------------------

HoldingSpaceTray::HoldingSpaceTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  controller_observer_.Observe(HoldingSpaceController::Get());
  session_observer_.Observe(Shell::Get()->session_controller());
  SetVisible(false);

  // Icon.
  default_tray_icon_ = tray_container()->AddChildView(CreateDefaultTrayIcon());

  if (features::IsTemporaryHoldingSpacePreviewsEnabled()) {
    previews_tray_icon_ = tray_container()->AddChildView(
        std::make_unique<HoldingSpaceTrayIcon>(shelf));
    previews_tray_icon_->SetVisible(false);

    // Enable context menu, which supports an action to toggle item previews.
    set_context_menu_controller(this);
  }

  // Drop target overlay.
  // NOTE: The `drop_target_overlay_` will only be visible when:
  //   * a drag is in progress,
  //   * the drag payload contains pinnable files, and
  //   * the cursor is sufficiently close to this view.
  drop_target_overlay_ = AddChildView(CreateDropTargetOverlay());
  drop_target_overlay_->layer()->SetOpacity(0.f);

  // Animation.
  set_use_bounce_in_animation(true);
}

HoldingSpaceTray::~HoldingSpaceTray() = default;

void HoldingSpaceTray::Initialize() {
  TrayBackgroundView::Initialize();

  if (features::IsTemporaryHoldingSpacePreviewsEnabled()) {
    DCHECK(previews_tray_icon_);
    UpdatePreviewsVisibility();

    // If previews feature is enabled, the preview icon is displayed
    // conditionally, depending on user prefs state.
    auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
    if (prefs)
      ObservePrefService(prefs);
  }

  // It's possible that this holding space tray was created after login, such as
  // would occur if the user connects an external display. In such situations
  // the holding space model will already have been attached.
  if (HoldingSpaceController::Get()->model())
    OnHoldingSpaceModelAttached(HoldingSpaceController::Get()->model());
}

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

void HoldingSpaceTray::SetVisiblePreferred(bool visible_preferred) {
  if (this->visible_preferred() == visible_preferred)
    return;

  holding_space_metrics::RecordPodAction(
      visible_preferred ? holding_space_metrics::PodAction::kShowPod
                        : holding_space_metrics::PodAction::kHidePod);

  TrayBackgroundView::SetVisiblePreferred(visible_preferred);

  if (!visible_preferred)
    CloseBubble();
}

bool HoldingSpaceTray::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::FILE_NAME;

  // Support custom web data so that file system sources can be retrieved from
  // pickled data. That is the storage location at which the Files app stores
  // both file paths *and* directory paths.
  format_types->insert(ui::ClipboardFormatType::GetWebCustomDataType());
  return true;
}

bool HoldingSpaceTray::AreDropTypesRequired() {
  return true;
}

bool HoldingSpaceTray::CanDrop(const ui::OSExchangeData& data) {
  return !ExtractUnpinnedFilePaths(data).empty();
}

// TODO(crbug.com/1171059): Instead of handling `OnDragEntered()`, show the
// `drop_target_overlay_` if the cursor is within range of this view.
void HoldingSpaceTray::OnDragEntered(const ui::DropTargetEvent& event) {
  if (ExtractUnpinnedFilePaths(event.data()).empty())
    UpdateDropTargetState(/*is_drop_target=*/false, &event);
  else
    UpdateDropTargetState(/*is_drop_target=*/true, &event);
}

int HoldingSpaceTray::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (ExtractUnpinnedFilePaths(event.data()).empty()) {
    UpdateDropTargetState(/*is_drop_target=*/false, &event);
    return ui::DragDropTypes::DRAG_NONE;
  }
  UpdateDropTargetState(/*is_drop_target=*/true, &event);
  return ui::DragDropTypes::DRAG_COPY;
}

// TODO(crbug.com/1171059): Instead of handling `OnDragExited()`, hide the
// `drop_target_overlay_` if the cursor is outside range of this view.
void HoldingSpaceTray::OnDragExited() {
  UpdateDropTargetState(/*is_drop_target=*/false, /*event=*/nullptr);
}

DragOperation HoldingSpaceTray::OnPerformDrop(
    const ui::DropTargetEvent& event) {
  UpdateDropTargetState(/*is_drop_target=*/false, /*event=*/nullptr);

  std::vector<base::FilePath> unpinned_file_paths(
      ExtractUnpinnedFilePaths(event.data()));
  if (unpinned_file_paths.empty())
    return DragOperation::kNone;

  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kDragAndDropToPin);

  HoldingSpaceController::Get()->client()->PinFiles(unpinned_file_paths);
  return DragOperation::kCopy;
}

void HoldingSpaceTray::Layout() {
  TrayBackgroundView::Layout();

  // The `drop_target_overlay_` should always fill this view's bounds as they
  // are perceived by the user. Note that the user perceives the bounds of this
  // view to be its background bounds, not its local bounds.
  drop_target_overlay_->SetBoundsRect(GetBackgroundBounds());
}

void HoldingSpaceTray::FirePreviewsUpdateTimerIfRunningForTesting() {
  if (previews_update_.IsRunning())
    previews_update_.FireNow();
}

void HoldingSpaceTray::UpdateVisibility() {
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  if (!model || Shell::Get()->session_controller()->IsUserSessionBlocked()) {
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
  switch (static_cast<HoldingSpaceCommandId>(command_id)) {
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
        static_cast<int>(HoldingSpaceCommandId::kHidePreviews),
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_HIDE_PREVIEWS),
        ui::ImageModel::FromVectorIcon(kVisibilityOffIcon, /*color_id=*/-1,
                                       kHoldingSpaceIconSize));
  } else {
    context_menu_model_->AddItemWithIcon(
        static_cast<int>(HoldingSpaceCommandId::kShowPreviews),
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_SHOW_PREVIEWS),
        ui::ImageModel::FromVectorIcon(kVisibilityIcon, /*color_id=*/-1,
                                       kHoldingSpaceIconSize));
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
  if (!features::IsTemporaryHoldingSpacePreviewsEnabled())
    return;

  UpdatePreviewsState();
  ObservePrefService(prefs);
}

void HoldingSpaceTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateVisibility();
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

  if (!show_previews) {
    previews_tray_icon_->Clear();
    previews_update_.Stop();
  }
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

// TODO(crbug.com/1171059): Animate translation of tray icons.
void HoldingSpaceTray::UpdateDropTargetState(bool is_drop_target,
                                             const ui::LocatedEvent* event) {
  AnimateToTargetOpacity(drop_target_overlay_, is_drop_target ? 1.f : 0.f);
  AnimateToTargetOpacity(default_tray_icon_, is_drop_target ? 0.f : 1.f);
  AnimateToTargetOpacity(previews_tray_icon_, is_drop_target ? 0.f : 1.f);

  const views::InkDropState target_ink_drop_state =
      is_drop_target ? views::InkDropState::ACTION_PENDING
                     : views::InkDropState::HIDDEN;
  if (GetInkDrop()->GetTargetInkDropState() == target_ink_drop_state)
    return;

  // Even though `event` is a `ui::LocatedEvent`, it may *not* return `true` for
  // `IsLocatedEvent()` which is checked downstream when animating the ink drop.
  // Since the only data that needs to propagate is `event` location, create a
  // synthetic event that *will* return `true` for `IsLocatedEvent()` if needed.
  std::unique_ptr<ui::MouseEvent> mouse_moved_event;
  if (event && !event->IsLocatedEvent()) {
    mouse_moved_event = std::make_unique<ui::MouseEvent>(
        ui::ET_MOUSE_MOVED, event->location_f(), event->root_location_f(),
        event->time_stamp(), /*flags=*/ui::EF_NONE,
        /*changed_button_flags=*/ui::EF_NONE);
    event = mouse_moved_event.get();
  }

  AnimateInkDrop(target_ink_drop_state, event);
}

BEGIN_METADATA(HoldingSpaceTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
