// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/drag_drop/scoped_drag_drop_observer.h"
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
#include "ash/style/ash_color_id.h"
#include "ash/system/holding_space/holding_space_animation_registry.h"
#include "ash/system/holding_space/holding_space_progress_indicator_util.h"
#include "ash/system/holding_space/holding_space_tray_bubble.h"
#include "ash/system/holding_space/holding_space_tray_icon.h"
#include "ash/system/holding_space/pinned_files_section.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

using ::ui::mojom::DragOperation;

// Animation.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(167);
constexpr base::TimeDelta kInProgressAnimationOpacityDuration =
    base::Milliseconds(100);
constexpr base::TimeDelta kInProgressAnimationScaleDelay =
    base::Milliseconds(50);
constexpr base::TimeDelta kInProgressAnimationScaleDuration =
    base::Milliseconds(166);
constexpr float kInProgressAnimationScaleFactor = 0.875f;

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
  if (!data.GetPickledData(ui::ClipboardFormatType::WebCustomDataType(),
                           &pickle)) {
    return {};
  }

  constexpr char16_t kFileSystemSourcesType[] = u"fs/sources";

  std::u16string file_system_sources;
  ui::ReadCustomDataForType(pickle.data(), pickle.size(),
                            kFileSystemSourcesType, &file_system_sources);
  if (file_system_sources.empty())
    return {};

  HoldingSpaceClient* const client = HoldingSpaceController::Get()->client();

  std::vector<base::FilePath> result;
  for (const base::StringPiece16& file_system_source :
       base::SplitStringPiece(file_system_sources, u"\n", base::TRIM_WHITESPACE,
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
  return prefs && holding_space_prefs::IsPreviewsEnabled(prefs);
}

// Returns whether a preview of `item` should be shown in the shelf. Beyond
// being initialized, what makes an `item` previewable is having been created by
// a user action.
bool IsPreviewable(const std::unique_ptr<HoldingSpaceItem>& item) {
  return item->IsInitialized() && !HoldingSpaceItem::IsSuggestion(item->type());
}

// Creates the default tray icon.
std::unique_ptr<views::ImageView> CreateDefaultTrayIcon() {
  auto icon = std::make_unique<views::ImageView>();
  icon->SetID(kHoldingSpaceTrayDefaultIconId);
  icon->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));
  icon->SetPaintToLayer();
  icon->layer()->SetFillsBoundsOpaquely(false);
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      features::IsHoldingSpaceRefreshEnabled() ? kHoldingSpaceRefreshIcon
                                               : kHoldingSpaceIcon,
      kColorAshIconColorPrimary, kHoldingSpaceTrayIconSize));
  return icon;
}

// Creates the icon to be parented by the drop target overlay to indicate that
// the parent view is a drop target and is capable of handling the current drag
// payload.
std::unique_ptr<views::ImageView> CreateDropTargetIcon() {
  auto icon = std::make_unique<views::ImageView>();
  icon->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  icon->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  icon->SetPreferredSize(
      gfx::Size(kHoldingSpaceIconSize, kHoldingSpaceIconSize));
  icon->SetPaintToLayer();
  icon->layer()->SetFillsBoundsOpaquely(false);
  icon->SetImage(gfx::CreateVectorIcon(
      views::kUnpinIcon, kColorAshIconColorPrimary, kHoldingSpaceIconSize));
  return icon;
}

// Creates the overlay to be drawn over all other child views to indicate that
// the parent view is a drop target and is capable of handling the current drag
// payload.
std::unique_ptr<views::View> CreateDropTargetOverlay() {
  auto drop_target_overlay = std::make_unique<views::View>();
  drop_target_overlay->SetID(kHoldingSpaceTrayDropTargetOverlayId);
  drop_target_overlay->SetLayoutManager(std::make_unique<views::FillLayout>());
  drop_target_overlay->SetPaintToLayer();
  drop_target_overlay->layer()->SetFillsBoundsOpaquely(false);
  return drop_target_overlay;
}

// Returns the `aura::client::DragDropClient` for the given `widget`. Note that
// this may return `nullptr` if the browser is performing its shutdown sequence.
aura::client::DragDropClient* GetDragDropClient(views::Widget* widget) {
  if (widget) {
    auto* native_window = widget->GetNativeWindow();
    if (native_window) {
      auto* root_window = native_window->GetRootWindow();
      if (root_window)
        return aura::client::GetDragDropClient(root_window);
    }
  }
  return nullptr;
}

}  // namespace

// HoldingSpaceTray ------------------------------------------------------------

HoldingSpaceTray::HoldingSpaceTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kHoldingSpace) {
  // Ensure the existence of the singleton animation registry.
  HoldingSpaceAnimationRegistry::GetInstance();

  controller_observer_.Observe(HoldingSpaceController::Get());
  session_observer_.Observe(Shell::Get()->session_controller());
  SetVisible(false);

  // Default icon.
  default_tray_icon_ = tray_container()->AddChildView(CreateDefaultTrayIcon());

  // Previews icon.
  previews_tray_icon_ = tray_container()->AddChildView(
      std::make_unique<HoldingSpaceTrayIcon>(shelf));
  previews_tray_icon_->SetVisible(false);

  // Drop target overlay.
  // NOTE: The `drop_target_overlay_` will only be visible when:
  //   * a drag is in progress,
  //   * the drag payload contains pinnable files, and
  //   * the cursor is sufficiently close to this view.
  drop_target_overlay_ = AddChildView(CreateDropTargetOverlay());
  drop_target_overlay_->layer()->SetOpacity(0.f);

  // Drop target icon.
  drop_target_icon_ =
      drop_target_overlay_->AddChildView(CreateDropTargetIcon());

  // Progress indicator.
  // NOTE: The `progress_indicator_` will only be visible when:
  //   * there is at least one in-progress item in the attached model, and
  //   * previews are hidden.
  progress_indicator_ =
      holding_space_util::CreateProgressIndicatorForController(
          HoldingSpaceController::Get());
  layer()->Add(progress_indicator_->CreateLayer());

  // Subscribe to receive notification of changes to the `progress_indicator_`'s
  // underlying progress. When progress changes, the `default_tray_icon_` may
  // need to be updated since it occupies the same space as the inner icon of
  // the `progress_indicator_`.
  progress_indicator_progress_changed_callback_list_subscription_ =
      progress_indicator_->AddProgressChangedCallback(base::BindRepeating(
          &HoldingSpaceTray::UpdateDefaultTrayIcon, base::Unretained(this)));
  UpdateDefaultTrayIcon();

  // Enable context menu, which supports an action to toggle item previews.
  SetContextMenuEnabled(true);
}

HoldingSpaceTray::~HoldingSpaceTray() = default;

void HoldingSpaceTray::Initialize() {
  TrayBackgroundView::Initialize();

  UpdatePreviewsVisibility();

  // The preview icon is displayed conditionally, depending on user prefs state.
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  if (prefs)
    ObservePrefService(prefs);

  // It's possible that this holding space tray was created after login, such as
  // would occur if the user connects an external display. In such situations
  // the holding space model will already have been attached.
  if (HoldingSpaceController::Get()->model())
    OnHoldingSpaceModelAttached(HoldingSpaceController::Get()->model());
}

void HoldingSpaceTray::ClickedOutsideBubble() {
  CloseBubble();
}

std::u16string HoldingSpaceTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_HOLDING_SPACE_A11Y_NAME,
      features::IsHoldingSpaceRefreshEnabled()
          ? l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE_REFRESH)
          : l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE));
}

views::View* HoldingSpaceTray::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tooltip events should be handled top level, not by descendents.
  return HitTestPoint(point) ? this : nullptr;
}

std::u16string HoldingSpaceTray::GetTooltipText(const gfx::Point& point) const {
  return features::IsHoldingSpaceRefreshEnabled()
             ? l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE_REFRESH)
             : l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE);
}

void HoldingSpaceTray::HandleLocaleChange() {
  TooltipTextChanged();
}

void HoldingSpaceTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->GetBubbleView() == bubble_view)
    CloseBubble();
}

void HoldingSpaceTray::AnchorUpdated() {
  if (bubble_)
    bubble_->AnchorUpdated();
}

void HoldingSpaceTray::UpdateAfterLoginStatusChange() {
  UpdateVisibility();
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

void HoldingSpaceTray::ShowBubble() {
  if (bubble_)
    return;

  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kShowBubble);

  DCHECK(tray_container());

  bubble_ = std::make_unique<HoldingSpaceTrayBubble>(this);

  // Observe the bubble widget so that we can close the bubble when a holding
  // space item is being dragged.
  widget_observer_.Observe(bubble_->GetBubbleWidget());

  SetIsActive(true);
}

TrayBubbleView* HoldingSpaceTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

views::Widget* HoldingSpaceTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
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
  format_types->insert(ui::ClipboardFormatType::WebCustomDataType());
  return true;
}

bool HoldingSpaceTray::AreDropTypesRequired() {
  return true;
}

bool HoldingSpaceTray::CanDrop(const ui::OSExchangeData& data) {
  return !ExtractUnpinnedFilePaths(data).empty();
}

int HoldingSpaceTray::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ExtractUnpinnedFilePaths(event.data()).empty()
             ? ui::DragDropTypes::DRAG_NONE
             : ui::DragDropTypes::DRAG_COPY;
}

views::View::DropCallback HoldingSpaceTray::GetDropCallback(
    const ui::DropTargetEvent& event) {
  std::vector<base::FilePath> unpinned_file_paths(
      ExtractUnpinnedFilePaths(event.data()));
  if (unpinned_file_paths.empty())
    return base::NullCallback();

  return base::BindOnce(&HoldingSpaceTray::PerformDrop,
                        weak_factory_.GetWeakPtr(),
                        std::move(unpinned_file_paths));
}

void HoldingSpaceTray::PerformDrop(
    std::vector<base::FilePath> unpinned_file_paths,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op) {
  DCHECK(!unpinned_file_paths.empty());

  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kDragAndDropToPin);

  HoldingSpaceController::Get()->client()->PinFiles(unpinned_file_paths);
  did_drop_to_pin_ = true;

  output_drag_op = DragOperation::kCopy;
}

void HoldingSpaceTray::Layout() {
  TrayBackgroundView::Layout();

  // The `drop_target_overlay_` should always fill this view's bounds as they
  // are perceived by the user. Note that the user perceives the bounds of this
  // view to be its background bounds, not its local bounds.
  const gfx::Rect background_bounds(GetBackgroundBounds());
  drop_target_overlay_->SetBoundsRect(GetMirroredRect(background_bounds));

  // The `progress_indicator_` should also fill this view's bounds as they are
  // perceived by the user, but these bounds do not need to be mirrored since
  // they are in layer coordinates.
  progress_indicator_->layer()->SetBounds(background_bounds);
}

void HoldingSpaceTray::VisibilityChanged(views::View* starting_from,
                                         bool is_visible) {
  TrayBackgroundView::VisibilityChanged(starting_from, is_visible);

  // `drag_drop_observer_` is constructed only when `HoldingSpaceTray` is
  // drawn. NOTE: `is_visible` indicates the visibility of `starting_from`.
  // Therefore, `IsDrawn()` should not be replaced by `is_visible`.
  if (!IsDrawn()) {
    drag_drop_observer_.reset();
    return;
  }

  // It's possible that the `drag_drop_client` might be `nullptr` if the browser
  // is performing its shutdown sequence.
  auto* drag_drop_client = GetDragDropClient(GetWidget());
  if (!drag_drop_client)
    return;

  // Observe drag/drop events only when visible. Since the observer is owned by
  // `this` view, it's safe to bind to a raw pointer.
  drag_drop_observer_ = std::make_unique<ScopedDragDropObserver>(
      drag_drop_client,
      /*event_callback=*/base::BindRepeating(
          &HoldingSpaceTray::UpdateDropTargetState, base::Unretained(this)));
}

void HoldingSpaceTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();

  // Progress indicator.
  progress_indicator_->InvalidateLayer();
}

void HoldingSpaceTray::UpdateVisibility() {
  // The holding space tray should not be visible if the `model` is not attached
  // or if the user session is blocked.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  if (!model || Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    SetVisiblePreferred(false);
    return;
  }

  // If the predictability flag is enabled, always show the holding space tray.
  if (features::IsHoldingSpacePredictabilityEnabled()) {
    SetVisiblePreferred(true);
    return;
  }

  // The holding space tray should always be shown if the `model` contains items
  // that are previewable, or if the predictability feature flag is enabled.
  // Otherwise, it should only be visible if the time of first add has been
  // marked, but a file has never been pinned, and the Files app chip has never
  // been pressed.
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  SetVisiblePreferred(
      base::ranges::any_of(model->items(), IsPreviewable) ||
      (prefs && holding_space_prefs::GetTimeOfFirstAdd(prefs) &&
       !holding_space_prefs::GetTimeOfFirstPin(prefs) &&
       !holding_space_prefs::GetTimeOfFirstFilesAppChipPress(prefs)));
}

void HoldingSpaceTray::FirePreviewsUpdateTimerIfRunningForTesting() {
  if (previews_update_.IsRunning())
    previews_update_.FireNow();
}

std::u16string HoldingSpaceTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool HoldingSpaceTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void HoldingSpaceTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void HoldingSpaceTray::OnShouldShowAnimationChanged(bool should_animate) {
  previews_tray_icon_->set_should_animate_updates(should_animate);
}

std::unique_ptr<ui::SimpleMenuModel>
HoldingSpaceTray::CreateContextMenuModel() {
  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kShowContextMenu);

  bool previews_enabled = holding_space_prefs::IsPreviewsEnabled(
      Shell::Get()->session_controller()->GetActivePrefService());
  auto context_menu_model = std::make_unique<ui::SimpleMenuModel>(this);

  if (previews_enabled) {
    context_menu_model->AddItemWithIcon(
        static_cast<int>(HoldingSpaceCommandId::kHidePreviews),
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_HIDE_PREVIEWS),
        ui::ImageModel::FromVectorIcon(kVisibilityOffIcon,
                                       ui::kColorAshSystemUIMenuIcon,
                                       kHoldingSpaceIconSize));
  } else {
    context_menu_model->AddItemWithIcon(
        static_cast<int>(HoldingSpaceCommandId::kShowPreviews),
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_SHOW_PREVIEWS),
        ui::ImageModel::FromVectorIcon(kVisibilityIcon,
                                       ui::kColorAshSystemUIMenuIcon,
                                       kHoldingSpaceIconSize));
  }

  return context_menu_model;
}

void HoldingSpaceTray::OnHoldingSpaceModelAttached(HoldingSpaceModel* model) {
  // When the `model` is attached the session is either being started/unlocked
  // or the active profile is being changed. It's also possible that the status
  // area is being initialized (such as is the case when a display is added at
  // runtime). The holding space tray should not bounce or animate previews
  // during this phase.
  SetShouldAnimate(false);

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
  // If an initialized holding space item is added to the model mid-session, the
  // holding space tray should bounce in (if it isn't already visible) and
  // previews should be animated.
  if (!Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    const bool has_initialized_item = base::ranges::any_of(
        items,
        [](const HoldingSpaceItem* item) { return item->IsInitialized(); });
    if (has_initialized_item)
      SetShouldAnimate(true);
  }

  UpdateVisibility();
  UpdatePreviewsState();
}

void HoldingSpaceTray::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  // If an initialized holding space item is removed from the model mid-session,
  // the holding space tray should animate updates.
  if (!Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    const bool has_initialized_item = base::ranges::any_of(
        items,
        [](const HoldingSpaceItem* item) { return item->IsInitialized(); });
    if (has_initialized_item)
      SetShouldAnimate(true);
  }

  UpdateVisibility();
  UpdatePreviewsState();
}

void HoldingSpaceTray::OnHoldingSpaceItemInitialized(
    const HoldingSpaceItem* item) {
  UpdateVisibility();
  UpdatePreviewsState();
}

void HoldingSpaceTray::ExecuteCommand(int command_id, int event_flags) {
  switch (static_cast<HoldingSpaceCommandId>(command_id)) {
    case HoldingSpaceCommandId::kHidePreviews:
      holding_space_metrics::RecordPodAction(
          holding_space_metrics::PodAction::kHidePreviews);

      holding_space_prefs::SetPreviewsEnabled(
          Shell::Get()->session_controller()->GetActivePrefService(), false);
      break;
    case HoldingSpaceCommandId::kShowPreviews:
      SetShouldAnimate(true);

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

void HoldingSpaceTray::OnWidgetDragWillStart(views::Widget* widget) {
  // The holding space bubble should be closed while dragging holding space
  // items so as not to obstruct drop targets. Post the task to close the bubble
  // so that we don't attempt to destroy the bubble widget before the associated
  // drag event has been fully initialized.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&HoldingSpaceTray::CloseBubble,
                                weak_factory_.GetWeakPtr()));
}

void HoldingSpaceTray::OnActiveUserPrefServiceChanged(PrefService* prefs) {
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

  // NOTE: The binding of these callbacks is scoped to `pref_change_registrar_`
  // which is owned by `this` so it is safe to bind with an unretained raw
  // pointer.
  holding_space_prefs::AddPreviewsEnabledChangedCallback(
      pref_change_registrar_.get(),
      base::BindRepeating(&HoldingSpaceTray::UpdatePreviewsState,
                          base::Unretained(this)));
  holding_space_prefs::AddTimeOfFirstAddChangedCallback(
      pref_change_registrar_.get(), base::BindRepeating(
                                        [](HoldingSpaceTray* tray) {
                                          tray->SetShouldAnimate(true);
                                          tray->UpdateVisibility();
                                        },
                                        base::Unretained(this)));
}

void HoldingSpaceTray::UpdatePreviewsState() {
  UpdatePreviewsVisibility();
  SchedulePreviewsIconUpdate();

  if (PreviewsShown())
    return;

  // When previews are shown, progress icon animations are started on completion
  // of preview animations. When previews are *not* shown, there is nothing to
  // wait for so progress icon animations should be started immediately.
  if (auto* model = HoldingSpaceController::Get()->model(); model) {
    auto* registry = HoldingSpaceAnimationRegistry::GetInstance();
    for (const auto& item : model->items()) {
      auto* animation = registry->GetProgressIconAnimationForKey(item.get());
      if (animation && !animation->HasAnimated())
        animation->Start();
    }
  }
}

void HoldingSpaceTray::UpdatePreviewsVisibility() {
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  const bool show_previews =
      IsPreviewsEnabled() && model &&
      base::ranges::any_of(model->items(), IsPreviewable);

  if (PreviewsShown() == show_previews)
    return;

  default_tray_icon_->SetVisible(!show_previews);
  previews_tray_icon_->SetVisible(show_previews);
  progress_indicator_->layer()->SetVisible(!show_previews);

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
                              : base::Milliseconds(50);
  previews_update_.Start(FROM_HERE, delay,
                         base::BindOnce(&HoldingSpaceTray::UpdatePreviewsIcon,
                                        base::Unretained(this)));
}

void HoldingSpaceTray::UpdatePreviewsIcon() {
  if (!PreviewsShown()) {
    previews_tray_icon_->Clear();
    return;
  }

  std::vector<const HoldingSpaceItem*> items_with_previews;
  std::set<base::FilePath> paths_with_previews;
  for (const auto& item :
       base::Reversed(HoldingSpaceController::Get()->model()->items())) {
    if (!IsPreviewable(item))
      continue;
    if (base::Contains(paths_with_previews, item->file_path()))
      continue;
    items_with_previews.push_back(item.get());
    paths_with_previews.insert(item->file_path());
  }
  previews_tray_icon_->UpdatePreviews(items_with_previews);
}

bool HoldingSpaceTray::PreviewsShown() const {
  return previews_tray_icon_->GetVisible();
}

void HoldingSpaceTray::UpdateDefaultTrayIcon() {
  const absl::optional<float>& progress = progress_indicator_->progress();

  // If `progress` is not `complete`, there is potential for overlap between the
  // `default_tray_icon_` and the `progress_indicator_`'s inner icon. To address
  // this, hide the `default_tray_icon_` when `progress` is being indicated.
  bool complete = progress == ProgressIndicator::kProgressComplete;
  float target_opacity = complete ? 1.f : 0.f;

  // If `target_opacity` is already set there's nothing to do.
  ui::Layer* const layer = default_tray_icon_->layer();
  if (layer->GetTargetOpacity() == target_opacity)
    return;

  // If `target_opacity` is zero, it should be set immediately w/o animation.
  if (target_opacity == 0.f) {
    layer->GetAnimator()->StopAnimating();
    layer->SetOpacity(0.f);
    return;
  }

  // If `bounds` are empty, the `default_tray_icon_` has not yet been laid out
  // and therefore any changes to its visual state need not be animated.
  const gfx::Rect& bounds = default_tray_icon_->bounds();
  if (bounds.IsEmpty()) {
    layer->SetOpacity(1.f);
    layer->SetTransform(gfx::Transform());
    return;
  }

  const auto preemption_strategy =
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET;
  const auto transform = gfx::GetScaleTransform(
      bounds.CenterPoint(), kInProgressAnimationScaleFactor);
  const auto tween_type = gfx::Tween::Type::FAST_OUT_SLOW_IN_3;

  views::AnimationBuilder()
      .SetPreemptionStrategy(preemption_strategy)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(layer, 0.f)
      .SetTransform(layer, transform)
      .Then()
      .SetDuration(kInProgressAnimationOpacityDuration)
      .SetOpacity(layer, 1.f)
      .Offset(kInProgressAnimationScaleDelay)
      .SetDuration(kInProgressAnimationScaleDuration)
      .SetTransform(layer, gfx::Transform(), tween_type);
}

void HoldingSpaceTray::UpdateDropTargetState(const ui::DropTargetEvent* event) {
  bool is_drop_target = false;

  if (event && !ExtractUnpinnedFilePaths(event->data()).empty()) {
    // If the `event` contains pinnable files and is within range of this view,
    // indicate this view is a drop target to increase discoverability.
    constexpr int kProximityThreshold = 20;
    gfx::Rect drop_target_bounds_in_screen(GetBoundsInScreen());
    drop_target_bounds_in_screen.Inset(gfx::Insets(-kProximityThreshold));

    gfx::Point event_location_in_screen(event->root_location());
    ::wm::ConvertPointToScreen(
        static_cast<aura::Window*>(event->target())->GetRootWindow(),
        &event_location_in_screen);

    is_drop_target =
        drop_target_bounds_in_screen.Contains(event_location_in_screen);
  }

  AnimateToTargetOpacity(drop_target_overlay_, is_drop_target ? 1.f : 0.f);
  AnimateToTargetOpacity(default_tray_icon_, is_drop_target ? 0.f : 1.f);
  AnimateToTargetOpacity(previews_tray_icon_, is_drop_target ? 0.f : 1.f);

  previews_tray_icon_->UpdateDropTargetState(is_drop_target, did_drop_to_pin_);
  did_drop_to_pin_ = false;

  const views::InkDropState target_ink_drop_state =
      is_drop_target ? views::InkDropState::ACTION_PENDING
                     : views::InkDropState::HIDDEN;
  if (views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState() ==
      target_ink_drop_state)
    return;

  // Do *not* pass in an event as the origin for the ink drop. Since the user is
  // not directly over this view, it would look strange to give the ink drop an
  // out-of-bounds origin.
  views::InkDrop::Get(this)->AnimateToState(target_ink_drop_state,
                                            /*event=*/nullptr);
}

void HoldingSpaceTray::SetShouldAnimate(bool should_animate) {
  if (!should_animate) {
    if (!animation_disabler_) {
      animation_disabler_ =
          std::make_unique<base::ScopedClosureRunner>(DisableShowAnimation());
    }
  } else if (animation_disabler_) {
    animation_disabler_.reset();
  }
}

BEGIN_METADATA(HoldingSpaceTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
