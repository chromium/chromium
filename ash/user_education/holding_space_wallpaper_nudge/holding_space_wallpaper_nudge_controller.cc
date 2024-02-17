// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/scoped_drag_drop_observer.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_metrics.h"
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"
#include "ash/user_education/user_education_controller.h"
#include "ash/user_education/user_education_help_bubble_controller.h"
#include "ash/user_education/user_education_ping_controller.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_drag_drop_delegate.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
HoldingSpaceWallpaperNudgeController* g_instance = nullptr;

// Helpers ---------------------------------------------------------------------

std::vector<base::FilePath> ExtractUnpinnedFilePaths(
    const ui::OSExchangeData& data) {
  const HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  if (!model) {
    return std::vector<base::FilePath>();
  }

  // We are only interested in file paths if they originated from the Files app,
  // so don't fall back to the filenames storage location if none are found.
  std::vector<base::FilePath> unpinned_file_paths =
      holding_space_util::ExtractFilePaths(data,
                                           /*fallback_to_filenames=*/false);

  base::EraseIf(unpinned_file_paths, [&](const base::FilePath& file_path) {
    return model->ContainsItem(HoldingSpaceItem::Type::kPinnedFile, file_path);
  });

  return unpinned_file_paths;
}

const ui::ClipboardFormatType& FilesAppFormatType() {
  // NOTE: The Files app stores file system sources as custom web data.
  return ui::ClipboardFormatType::WebCustomDataType();
}

std::u16string GetBubbleBodyText() {
  auto string_id =
      features::IsHoldingSpaceWallpaperNudgeDropToPinEnabled()
          ? IDS_ASH_HOLDING_SPACE_WALLPAPER_NUDGE_DROP_ENABLED_TEXT
          : IDS_ASH_HOLDING_SPACE_WALLPAPER_NUDGE_DROP_DISABLED_TEXT;
  return l10n_util::GetStringFUTF16(
      string_id,
      features::IsHoldingSpaceRefreshEnabled()
          ? l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE_REFRESH)
          : l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE));
}

aura::Window* GetRootWindowForDisplayId(int64_t display_id) {
  return Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
      display_id);
}

display::Display GetDisplayNearestPoint(const gfx::Point& location_in_screen) {
  return display::Screen::GetScreen()->GetDisplayNearestPoint(
      location_in_screen);
}

aura::client::DragDropClient* GetDragDropClientNearestPoint(
    const gfx::Point& location_in_screen) {
  return aura::client::GetDragDropClient(GetRootWindowForDisplayId(
      GetDisplayNearestPoint(location_in_screen).id()));
}

gfx::Point GetLocationInScreen(const ui::DropTargetEvent* event) {
  gfx::Point location_in_screen = event->root_location();
  wm::ConvertPointToScreen(
      static_cast<aura::Window*>(event->target())->GetRootWindow(),
      &location_in_screen);
  return location_in_screen;
}

Shelf* GetShelfNearestPoint(const gfx::Point& location_in_screen) {
  return Shelf::ForWindow(GetRootWindowForDisplayId(
      GetDisplayNearestPoint(location_in_screen).id()));
}

HoldingSpaceTray* GetHoldingSpaceTrayNearestPoint(
    const gfx::Point& location_in_screen) {
  return GetShelfNearestPoint(location_in_screen)
      ->status_area_widget()
      ->holding_space_tray();
}

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

WallpaperView* GetWallpaperViewNearestPoint(
    const gfx::Point& location_in_screen) {
  return RootWindowController::ForWindow(
             GetRootWindowForDisplayId(
                 GetDisplayNearestPoint(location_in_screen).id()))
      ->wallpaper_widget_controller()
      ->wallpaper_view();
}

bool IsWallpaperViewTarget(const ui::DropTargetEvent* event) {
  return static_cast<aura::Window*>(event->target())
      ->Contains(GetWallpaperViewNearestPoint(GetLocationInScreen(event))
                     ->GetWidget()
                     ->GetNativeWindow());
}

// TODO(http://b/311411775): Relocate recording wallpaper nudge histograms
// into the production metrics code path when cleaning up the experiment.
void RecordPinInteraction(const std::vector<const HoldingSpaceItem*>& items,
                          holding_space_metrics::EventSource event_source) {
  using holding_space_metrics::EventSource;
  using holding_space_wallpaper_nudge_metrics::Interaction;

  std::optional<Interaction> interaction;

  switch (event_source) {
    case EventSource::kHoldingSpaceBubble:
      NOTREACHED_NORETURN();
    case EventSource::kHoldingSpaceItem:
      interaction = Interaction::kPinnedFileFromPinButton;
      break;
    case EventSource::kHoldingSpaceItemContextMenu:
      interaction = Interaction::kPinnedFileFromContextMenu;
      break;
    case EventSource::kHoldingSpaceTray:
      interaction = Interaction::kPinnedFileFromHoldingSpaceDrop;
      break;
    case EventSource::kFilesApp:
      interaction = Interaction::kPinnedFileFromFilesApp;
      break;
    case EventSource::kTest:
      CHECK_IS_TEST();
      break;
    case EventSource::kWallpaper:
      interaction = Interaction::kPinnedFileFromWallpaperDrop;
      break;
  }

  for (const HoldingSpaceItem* _ : items) {
    holding_space_wallpaper_nudge_metrics::RecordInteraction(
        Interaction::kPinnedFileFromAnySource);
    if (interaction.has_value()) {
      holding_space_wallpaper_nudge_metrics::RecordInteraction(
          interaction.value());
    }
  }
}

// TODO(http://b/311411775): Relocate recording wallpaper nudge histograms
// into the production metrics code path when cleaning up the experiment.
void RecordUsedInteraction(const std::vector<const HoldingSpaceItem*>& items) {
  using holding_space_wallpaper_nudge_metrics::Interaction;

  for (const HoldingSpaceItem* item : items) {
    holding_space_wallpaper_nudge_metrics::RecordInteraction(
        item->type() == HoldingSpaceItem::Type::kPinnedFile
            ? Interaction::kUsedPinnedItem
            : Interaction::kUsedOtherItem);
  }
}

// Highlight -------------------------------------------------------------------

// A class which adds a highlight layer to the region above the associated
// `view`. On destruction, the highlight layer is automatically removed from
// the associated `view`. It is not required for the associated `view` to
// outlive its highlight.
class Highlight : public ui::LayerOwner, public views::ViewObserver {
 public:
  explicit Highlight(views::View* view)
      : ui::LayerOwner(std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR)) {
    // The associated `view` must have a layer to support layer regions.
    CHECK(view->layer());

    // Name the highlight layer so it is easy to identify in debugging/testing.
    layer()->SetName(HoldingSpaceWallpaperNudgeController::kHighlightLayerName);

    // Initialize highlight layer properties.
    layer()->SetFillsBoundsOpaquely(false);
    OnViewThemeChanged(view);
    OnViewBoundsChanged(view);

    // Add the highlight layer to the region above `view` layers so that it is
    // always shown on top of the `view`.
    view->AddLayerToRegion(layer(), views::LayerRegion::kAbove);

    // Observe the `view` to keep the highlight layer in sync.
    view_observation_.Observe(view);
  }

  Highlight(const Highlight&) = delete;
  Highlight& operator=(const Highlight&) = delete;
  ~Highlight() override = default;

 private:
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override {
    // Match the highlight layer bounds to the associated `view`. Note that
    // because the highlight layer was added to the region above `view` layers,
    // the highlight layer and `view` layer are siblings and so share the same
    // coordinate system.
    layer()->SetBounds(view->layer()->bounds());
  }

  void OnViewIsDeleting(views::View* view) override {
    view_observation_.Reset();
  }

  void OnViewThemeChanged(views::View* view) override {
    layer()->SetColor(SkColorSetA(
        view->GetColorProvider()->GetColor(cros_tokens::kCrosSysPrimaryLight),
        0.4f * SK_AlphaOPAQUE));
  }

  // Observe the associated view in order to keep the highlight layer in sync.
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
};

// DragDropSequenceTracker -----------------------------------------------------

// A class which facilitates tracking drag-and-drop sequences.
class DragDropSequenceTracker {
 public:
  DragDropSequenceTracker() = default;
  DragDropSequenceTracker(const DragDropSequenceTracker&) = delete;
  DragDropSequenceTracker& operator=(const DragDropSequenceTracker&) = delete;
  ~DragDropSequenceTracker() = default;

  // Returns whether a new sequence is actively being tracked. A sequence is new
  // only until it receives its first drag update.
  bool IsTrackingNewSequence() const { return is_tracking_new_sequence_; }

  // Returns whether a sequence is actively being tracked.
  bool IsTrackingSequence() const { return !!sequence_observer_; }

  // Initiates tracking a new sequence associated with the specified `client`.
  // If the tracked sequence terminates in drag completion, the specified
  // `drag_completed_callback` is invoked with the final `ui::DropTargetEvent`.
  // NOTE: This method may only be called while a sequence is in progress.
  void TrackNewSequence(
      aura::client::DragDropClient* client,
      base::OnceCallback<void(const ui::DropTargetEvent* event)>
          drag_completed_callback) {
    CHECK(client->IsDragDropInProgress());
    drag_completed_callback_ = std::move(drag_completed_callback);
    is_tracking_new_sequence_ = true;
    sequence_observer_ = std::make_unique<ScopedDragDropObserver>(
        client, base::BindRepeating(&DragDropSequenceTracker::OnDropTargetEvent,
                                    base::Unretained(this)));
  }

 private:
  void OnDropTargetEvent(ScopedDragDropObserver::EventType event_type,
                         const ui::DropTargetEvent* event) {
    is_tracking_new_sequence_ = false;
    switch (event_type) {
      case ScopedDragDropObserver::EventType::kDragCompleted:
        std::move(drag_completed_callback_).Run(event);
        [[fallthrough]];
      case ScopedDragDropObserver::EventType::kDragCancelled:
        drag_completed_callback_.Reset();
        sequence_observer_.reset();
        break;
      case ScopedDragDropObserver::EventType::kDragUpdated:
        break;
    }
  }

  // Exists only while a sequence is actively being tracked and invoked only if
  // the tracked sequence terminates in drag completion.
  base::OnceCallback<void(const ui::DropTargetEvent* event)>
      drag_completed_callback_;

  // Whether a new sequence is actively being tracked. A sequence is new only
  // until it receives its first drag update.
  bool is_tracking_new_sequence_ = false;

  // Exists only while a sequence is actively being tracked.
  std::unique_ptr<ScopedDragDropObserver> sequence_observer_;
};

// DragDropDelegate ------------------------------------------------------------

// An implementation of the singleton drag-and-drop delegate, owned by the
// `WallpaperControllerImpl`, which observes a drag-and-drop sequence once the
// user has dragged a file from the Files app over the wallpaper. It then
// ensures that:
//
// (a) the shelf is visible on the active display, and that
// (b) holding space is visible in the shelf on all displays
//
// While the observed drag-and-drop sequence is in progress.
class DragDropDelegate : public WallpaperDragDropDelegate,
                         public HoldingSpaceControllerObserver,
                         public holding_space_metrics::Observer,
                         public SessionObserver {
 public:
  explicit DragDropDelegate(
      UserEducationPrivateApiKey user_education_private_api_key)
      : user_education_private_api_key_(user_education_private_api_key) {
    session_observer_.Observe(Shell::Get()->session_controller());
  }

 private:
  // WallpaperDragDropDelegate:
  void GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* types) override {
    types->insert(FilesAppFormatType());
  }

  bool CanDrop(const ui::OSExchangeData& data) override {
    // Ineligible users should not see any behavioral changes introduced by
    // the holding space wallpaper nudge experiment. Returning `false` here
    // prevents any downstream events from being received.
    if (!features::IsHoldingSpaceWallpaperNudgeForceEligibilityEnabled() &&
        holding_space_wallpaper_nudge_prefs::GetUserEligibility(
            Shell::Get()
                ->session_controller()
                ->GetLastActiveUserPrefService()) != true) {
      return false;
    }

    // If this `data` can be pinned to holding space, return true to make sure
    // we can track the drag to show the nudge appropriately, even if
    // drop-to-pin is not enabled.
    return !ExtractUnpinnedFilePaths(data).empty();
  }

  void OnDragEntered(const ui::OSExchangeData& data,
                     const gfx::Point& location_in_screen) override {
    // Record metrics.
    holding_space_wallpaper_nudge_metrics::RecordInteraction(
        holding_space_wallpaper_nudge_metrics::Interaction::
            kDraggedFileOverWallpaper);

    // Start tracking the drag-and-drop sequence if necessary.
    if (!drag_drop_sequence_tracker_.IsTrackingSequence()) {
      drag_drop_sequence_tracker_.TrackNewSequence(
          GetDragDropClientNearestPoint(location_in_screen),
          /*drag_completed_callback=*/base::BindOnce(
              [](const ui::DropTargetEvent* event) {
                if (IsWallpaperViewTarget(event)) {
                  holding_space_wallpaper_nudge_metrics::RecordInteraction(
                      holding_space_wallpaper_nudge_metrics::Interaction::
                          kDroppedFileOnWallpaper);
                }
              }));
    }

    if (features::IsHoldingSpaceWallpaperNudgeEnabledCounterfactually()) {
      if (const auto reason = NudgeShouldBeSuppressed()) {
        if (drag_drop_sequence_tracker_.IsTrackingNewSequence()) {
          // Record that the nudge was suppressed for the specified `reason`.
          // NOTE: Suppression should be recorded at most once per sequence.
          holding_space_wallpaper_nudge_metrics::RecordNudgeSuppressed(*reason);
        }
      } else {
        // Mark the nudge as "shown" and record the corresponding metrics for
        // the counterfactual experiment arm given that the nudge won't actually
        // be shown to the user.
        holding_space_wallpaper_nudge_prefs::MarkNudgeShown(
            Shell::Get()->session_controller()->GetLastActiveUserPrefService());
        holding_space_wallpaper_nudge_metrics::RecordNudgeShown();
        holding_space_wallpaper_nudge_metrics::RecordNudgeDuration(
            base::TimeDelta());
      }
      return;
    }

    if (features::IsHoldingSpaceWallpaperNudgeDropToPinEnabled()) {
      // Highlight the wallpaper when `data` is dragged over it so that the user
      // better understands the wallpaper is a drop target.
      CHECK(!wallpaper_highlight_);
      wallpaper_highlight_ = std::make_unique<Highlight>(
          GetWallpaperViewNearestPoint(location_in_screen));
    }

    // If the `drag_drop_observer_` already exists, we are already observing the
    // current drag-and-drop sequence and can no-op here.
    if (drag_drop_observer_) {
      return;
    }

    // Begin observing the `HoldingSpaceController` in case holding space is
    // opened/closed. This observation will continue until destruction.
    if (!holding_space_controller_observer_.IsObserving()) {
      holding_space_controller_observer_.Observe(HoldingSpaceController::Get());
    }

    // Once the user has dragged a file from the Files app over the wallpaper,
    // observe the drag-and-drop sequence to ensure that (a) the shelf is
    // visible on the active display and that (b) holding space is visible in
    // the shelf on all displays while the observed drag-and-drop sequence is in
    // progress.
    drag_drop_observer_ = std::make_unique<ScopedDragDropObserver>(
        GetDragDropClientNearestPoint(location_in_screen),
        base::BindRepeating(&DragDropDelegate::OnDropTargetEvent,
                            base::Unretained(this)));

    // Explicitly update state as `OnDropTargetEvent()` will not be invoked
    // until the next drag event.
    OnDragOrDropEvent(location_in_screen);
  }

  ui::DragDropTypes::DragOperation OnDragUpdated(
      const ui::OSExchangeData& data,
      const gfx::Point& location_in_screen) override {
#if EXPENSIVE_DCHECKS_ARE_ON()
    // NOTE: Data is assumed to be constant during a drag-and-drop sequence.
    DCHECK(CanDrop(data));
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
    return (!features::IsHoldingSpaceWallpaperNudgeEnabledCounterfactually() &&
            features::IsHoldingSpaceWallpaperNudgeDropToPinEnabled())
               ? ui::DragDropTypes::DragOperation::DRAG_COPY
               : ui::DragDropTypes::DragOperation::DRAG_NONE;
  }

  void OnDragExited() override {
    if (!features::IsHoldingSpaceWallpaperNudgeEnabledCounterfactually() &&
        features::IsHoldingSpaceWallpaperNudgeDropToPinEnabled()) {
      // When `data` is dragged out of the wallpaper, remove the highlight which
      // was used to indicate the wallpaper was a drop target.
      CHECK(wallpaper_highlight_);
      wallpaper_highlight_.reset();
    }
  }

  ui::mojom::DragOperation OnDrop(
      const ui::OSExchangeData& data,
      const gfx::Point& location_in_screen) override {
    if (!features::IsHoldingSpaceWallpaperNudgeDropToPinEnabled() ||
        features::IsHoldingSpaceWallpaperNudgeEnabledCounterfactually()) {
      return ui::mojom::DragOperation::kNone;
    }

    // When `data` is dropped on the wallpaper, remove the highlight which was
    // used to indicate the wallpaper was a drop target.
    CHECK(wallpaper_highlight_);
    wallpaper_highlight_.reset();

    // No-op if no holding space `client` is registered since we will be unable
    // to handle the dropped `data`.
    HoldingSpaceClient* const client = HoldingSpaceController::Get()->client();
    if (!client) {
      return ui::mojom::DragOperation::kNone;
    }

    // No-op if the dropped `data` does not contain any unpinned files.
    const std::vector<base::FilePath> unpinned_file_paths =
        ExtractUnpinnedFilePaths(data);
    if (unpinned_file_paths.empty()) {
      return ui::mojom::DragOperation::kNone;
    }

    // Dropping `data` on the wallpaper results in pinning of files to holding
    // space. Note that this will cause holding space to be visible in the shelf
    // if it wasn't already visible.
    client->PinFiles(unpinned_file_paths,
                     holding_space_metrics::EventSource::kWallpaper);

    if (features::IsHoldingSpaceWallpaperNudgeAutoOpenEnabled()) {
      // Open the holding space tray so that the user can see the newly pinned
      // files and understands the relationship between the action they took on
      // the wallpaper and its effect in holding space.
      GetHoldingSpaceTrayNearestPoint(location_in_screen)->ShowBubble();
    } else {
      // Since the holding space tray is not being auto-opened, prevent auto-
      // hiding of the shelf for a second so that the user can see the newly
      // pinned file in the holding space tray and understands the relationship
      // between the action they took on the wallpaper and its effect in holding
      // space.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::DoNothingWithBoundArgs(
              std::make_unique<Shelf::ScopedDisableAutoHide>(
                  GetShelfNearestPoint(location_in_screen))),
          base::Seconds(1));
    }

    return ui::mojom::DragOperation::kCopy;
  }

  void OnDropTargetEvent(ScopedDragDropObserver::EventType event_type,
                         const ui::DropTargetEvent* event) {
    // This code should only be reached if we are observing a drag-and-drop
    // sequence due to the user dragging a file from the Files app over the
    // wallpaper.
    CHECK(drag_drop_observer_);

    std::optional<gfx::Point> location_in_screen;
    if (event_type == ScopedDragDropObserver::EventType::kDragUpdated) {
      location_in_screen = GetLocationInScreen(event);
    }

    OnDragOrDropEvent(std::move(location_in_screen));
  }

  void OnDragOrDropEvent(std::optional<gfx::Point> location_in_screen) {
    // This code should only be reached if we are observing a drag-and-drop
    // sequence due to the user dragging a file from the Files app over the
    // wallpaper.
    CHECK(drag_drop_observer_);

    // If `location_in_screen` is absent, the observed drag-and-drop sequence
    // has been completed or cancelled. We can stop observing drag-and-drop
    // sequences and reset the shelf to its natural state.
    if (!location_in_screen) {
      drag_drop_observer_.reset();
      force_holding_space_show_in_shelf_for_drag_.reset();

      // Reset shelf auto-hide behavior asynchronously so that it won't animate
      // out and immediately back in again if the user drops a file from the
      // Files app over the wallpaper.
      if (disable_shelf_auto_hide_) {
        base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
            FROM_HERE, std::move(disable_shelf_auto_hide_));
      }
      return;
    }

    Shelf* shelf = GetShelfNearestPoint(location_in_screen.value());
    CHECK(shelf);

    // If the shelf is currently being force-shown on the wrong display (i.e.
    // the file has been dragged to a new display), switch to the correct one.
    if (disable_shelf_auto_hide_ &&
        disable_shelf_auto_hide_->weak_shelf() != shelf) {
      disable_shelf_auto_hide_ =
          std::make_unique<Shelf::ScopedDisableAutoHide>(shelf);
    }

    const auto nudge_suppressed_reason = NudgeShouldBeSuppressed();
    const bool nudge_should_be_shown = !nudge_suppressed_reason.has_value();

    // The user should be directed to the tray during drag operations iff the
    // nudge will be shown or drop-to-pin is disabled. This is because we want
    // to direct users to drag to the holding space when drop-to-pin is
    // disabled, but encourage dropping on the desktop when it's enabled.
    const bool should_direct_users_to_tray =
        nudge_should_be_shown ||
        !features::IsHoldingSpaceWallpaperNudgeDropToPinEnabled();

    // Ensure that holding space is visible in the shelf on all displays while
    // the observed drag-and-drop sequence is in progress when we're trying to
    // encourage users to drag files there.
    if (!force_holding_space_show_in_shelf_for_drag_ &&
        should_direct_users_to_tray) {
      force_holding_space_show_in_shelf_for_drag_ =
          std::make_unique<HoldingSpaceController::ScopedForceShowInShelf>();
    }

    // Ensure the shelf is visible on the active display while the observed
    // drag-and-drop sequence is in progress when we're trying to encourage
    // users to drag files there.
    if (!disable_shelf_auto_hide_ && should_direct_users_to_tray) {
      disable_shelf_auto_hide_ =
          std::make_unique<Shelf::ScopedDisableAutoHide>(shelf);
    }

    if (!nudge_should_be_shown || help_bubble_anchor_) {
      if (nudge_suppressed_reason.has_value() &&
          drag_drop_sequence_tracker_.IsTrackingNewSequence()) {
        // NOTE: Suppression should be recorded at most once per sequence.
        holding_space_wallpaper_nudge_metrics::RecordNudgeSuppressed(
            *nudge_suppressed_reason);
      }
      return;
    }

    // Cache the `holding_space_tray` nearest the `location_in_screen` so that
    // we can show an associated help bubble.
    HoldingSpaceTray* const holding_space_tray =
        GetHoldingSpaceTrayNearestPoint(location_in_screen.value());

    // Configure the help bubble.
    user_education::HelpBubbleParams help_bubble_params;
    help_bubble_params.arrow = user_education::HelpBubbleArrow::kBottomRight;
    help_bubble_params.body_text = GetBubbleBodyText();
    help_bubble_params.extended_properties =
        user_education_util::CreateExtendedProperties(HelpBubbleStyle::kNudge);

    // `base::AutoReset` is safe here, because this is guaranteed to be
    // destroyed before destruction of `this` is complete.
    base::AutoReset<uintptr_t> reset_help_bubble_anchor(
        &help_bubble_anchor_, /*new_value=*/0, /*expected_old_value=*/0);

    // While the help bubble is showing, do not allow either the associated
    // `shelf` or `holding_space_tray` to hide. Also reset the pointer to
    // the `help_bubble_anchor_` on close.
    base::OnceClosure close_callback = base::BindOnce(
        [](base::ElapsedTimer elapsed_timer, Shelf::ScopedDisableAutoHide*,
           HoldingSpaceController::ScopedForceShowInShelf*,
           const base::AutoReset<uintptr_t>&) {
          // Record the duration of time that the nudge was shown.
          holding_space_wallpaper_nudge_metrics::RecordNudgeDuration(
              /*duration=*/elapsed_timer.Elapsed());
        },
        base::ElapsedTimer(),
        base::Owned(std::make_unique<Shelf::ScopedDisableAutoHide>(shelf)),
        base::Owned(
            std::make_unique<HoldingSpaceController::ScopedForceShowInShelf>()),
        base::OwnedRef(std::move(reset_help_bubble_anchor)));

    // Attempt to show the help bubble.
    if (auto scoped_help_bubble_closer =
            UserEducationHelpBubbleController::Get()->CreateScopedHelpBubble(
                HelpBubbleId::kHoldingSpaceWallpaperNudge,
                std::move(help_bubble_params), kHoldingSpaceTrayElementId,
                views::ElementTrackerViews::GetContextForView(
                    holding_space_tray),
                std::move(close_callback))) {
      // Mark the nudge as shown and record the corresponding metric.
      holding_space_wallpaper_nudge_prefs::MarkNudgeShown(
          Shell::Get()->session_controller()->GetLastActiveUserPrefService());
      holding_space_wallpaper_nudge_metrics::RecordNudgeShown();

      // If we successfully created a help bubble, then it is safe to replace
      // the current `base::ScopedClosureRunner` because any previous help
      // bubbles have already closed.
      scoped_help_bubble_closer_ = std::move(scoped_help_bubble_closer);

      // Store a pointer to the `HoldingSpaceTray` anchoring the help bubble to
      // test for potential overlap later.
      help_bubble_anchor_ = reinterpret_cast<uintptr_t>(holding_space_tray);

      // If successful in showing the help bubble, ping the `holding_space_tray`
      // to further attract the user's attention.
      UserEducationPingController::Get()->CreatePing(
          PingId::kHoldingSpaceWallpaperNudge, holding_space_tray);
    }
  }

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceControllerDestroying() override {
    holding_space_controller_observer_.Reset();
  }

  void OnHoldingSpaceTrayBubbleVisibilityChanged(const HoldingSpaceTray* tray,
                                                 bool visible) override {
    if (visible && help_bubble_anchor_) {
      // Only force show holding space in the shelf if the model is empty.
      // Otherwise, if the model becomes empty while visible to the user, the
      // tray and bubble will not be hidden. This would occur if, for example,
      // the user pins their first file to holding space via drag-and-drop but
      // then immediately unpins it when holding space is presented.
      force_holding_space_show_in_shelf_for_tray_bubble_ =
          HoldingSpaceController::Get()->model()->items().empty()
              ? std::make_unique<
                    HoldingSpaceController::ScopedForceShowInShelf>()
              : nullptr;

      // If the tray that emitted this event is the one that the currently open
      // help bubble is anchored to, close the help bubble to avoid overlap
      // between the two bubbles.
      if (reinterpret_cast<uintptr_t>(tray) == help_bubble_anchor_) {
        scoped_help_bubble_closer_.RunAndReset();
      }
    } else {
      force_holding_space_show_in_shelf_for_tray_bubble_.reset();
    }
  }

  // holding_space_metrics::Observer:
  // TODO(http://b/311411775): Relocate recording wallpaper nudge histograms
  // into the production metrics code path when cleaning up the experiment.
  void OnHoldingSpaceItemActionRecorded(
      const std::vector<const HoldingSpaceItem*>& items,
      holding_space_metrics::ItemAction action,
      holding_space_metrics::EventSource event_source) override {
    using holding_space_metrics::ItemAction;

    switch (action) {
      case ItemAction::kPin:
        RecordPinInteraction(items, event_source);
        break;
      case ItemAction::kCancel:
      case ItemAction::kCopy:
      case ItemAction::kDrag:
      case ItemAction::kLaunch:
      case ItemAction::kPause:
      case ItemAction::kRemove:
      case ItemAction::kResume:
      case ItemAction::kShowInFolder:
        RecordUsedInteraction(items);
        break;
      case ItemAction::kUnpin:
        break;
    }
  }

  // TODO(http://b/311411775): Relocate recording wallpaper nudge histograms
  // into the production metrics code path when cleaning up the experiment.
  void OnHoldingSpacePodActionRecorded(
      holding_space_metrics::PodAction action) override {
    using holding_space_metrics::PodAction;
    using holding_space_wallpaper_nudge_metrics::Interaction;

    switch (action) {
      case PodAction::kDragAndDropToPin:
        RecordInteraction(Interaction::kDroppedFileOnHoldingSpace);
        break;
      case PodAction::kShowBubble:
        RecordInteraction(Interaction::kOpenedHoldingSpace);
        break;
      case PodAction::kCloseBubble:
      case PodAction::kHidePod:
      case PodAction::kHidePreviews:
      case PodAction::kShowContextMenu:
      case PodAction::kShowPod:
      case PodAction::kShowPreviews:
        break;
    }
  }

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override {
    pref_change_registrar_.Reset();

    // Only observe the `pref_service` for the primary user and, even then, only
    // if the primary user has never pinned a file to holding space.
    if (pref_service != GetPrimaryUserPrefService() ||
        holding_space_prefs::GetTimeOfFirstPin(pref_service).has_value()) {
      return;
    }

    // Observe the primary user's `pref_service` only until their first pin, at
    // which time the appropriate metrics should be recorded.
    pref_change_registrar_.Init(pref_service);
    holding_space_prefs::AddTimeOfFirstPinChangedCallback(
        &pref_change_registrar_,
        base::BindRepeating(
            [](PrefChangeRegistrar& pref_change_registrar) {
              holding_space_wallpaper_nudge_metrics::RecordFirstPin();
              pref_change_registrar.Reset();
            },
            std::ref(pref_change_registrar_)));
  }

  void OnChromeTerminating() override {
    pref_change_registrar_.Reset();
    session_observer_.Reset();
  }

  void OnSessionStateChanged(session_manager::SessionState state) override {
    // Eligibility is only determined on primary account activation.
    if (!user_education_util::IsPrimaryAccountActive()) {
      return;
    }

    // Determine (and store) eligibility. If the user is eligible, then attempt
    // to mark this as the first eligible session.
    if (DetermineEligibility()) {
      holding_space_wallpaper_nudge_prefs::MarkTimeOfFirstEligibleSession(
          Shell::Get()->session_controller()->GetLastActiveUserPrefService());
    }
  }

  // Calculates and persists the user's eligibility for the nudge based on
  // account type and new-ness. This is a simple pref fetch once the eligibility
  // is persisted. Returns whether the user is eligible.
  bool DetermineEligibility() {
    using IneligibleReason =
        holding_space_wallpaper_nudge_metrics::IneligibleReason;

    const auto* const session_controller = Shell::Get()->session_controller();
    auto* const prefs = session_controller->GetLastActiveUserPrefService();

    // Return early if we've already persisted the user's eligibility.
    if (std::optional<bool> eligibility =
            holding_space_wallpaper_nudge_prefs::GetUserEligibility(prefs)) {
      return eligibility.value();
    }

    // Otherwise, determine the user's eligibility and persist it now.
    std::optional<IneligibleReason> ineligible_reason;

    // Only regular users are eligible.
    if (const auto user_type = session_controller->GetUserType();
        user_type != user_manager::UserType::kRegular) {
      ineligible_reason = IneligibleReason::kUserTypeNotRegular;
    }

    // Only un-managed users are eligible.
    if (!ineligible_reason && session_controller->IsActiveAccountManaged()) {
      ineligible_reason = IneligibleReason::kManagedAccount;
    }

    // Only new users are eligible. Note that this check of local newness is
    // necessary in case the cross-device check below proves to be erroneous.
    if (!ineligible_reason && !session_controller->IsUserFirstLogin()) {
      ineligible_reason = IneligibleReason::kUserNotNewLocally;
    }

    // Check cross-device newness.
    if (!ineligible_reason.has_value()) {
      const std::optional<bool>& is_new_user =
          UserEducationController::Get()->IsNewUser(
              user_education_private_api_key_);

      // Only new users are eligible.
      if (is_new_user == false) {
        ineligible_reason = IneligibleReason::kUserNotNewCrossDevice;
      }

      // If we aren't sure if the user is new, err on the side of being overly
      // conservative and treat the user as ineligible.
      if (!is_new_user.has_value()) {
        ineligible_reason = IneligibleReason::kUserNewnessNotAvailable;
      }
    }

    // Persist the user's eligibility.
    if (holding_space_wallpaper_nudge_prefs::SetUserEligibility(
            prefs, /*eligible=*/!ineligible_reason.has_value())) {
      holding_space_wallpaper_nudge_metrics::RecordUserEligibility(
          ineligible_reason);
    }

    // Return the user's eligibility.
    return !ineligible_reason.has_value();
  }

  // Indicates whether the nudge should be suppressed based on when it was last
  // shown, how many times total it's been shown, and whether the user has
  // pinned a file before. It shouldn't be shown more than once a day, no more
  // than 3 times total, and never if the user has pinned a file before.
  std::optional<holding_space_wallpaper_nudge_metrics::SuppressedReason>
  NudgeShouldBeSuppressed() {
    using holding_space_wallpaper_nudge_metrics::SuppressedReason;

    // If the user is not the primary user, suppress the nudge.
    // NOTE: User education in Ash is currently only supported for the primary
    // user profile. This is a self-imposed restriction.
    if (!user_education_util::IsPrimaryAccountActive()) {
      return SuppressedReason::kNotPrimaryAccount;
    }

    const bool forced_eligibility =
        features::IsHoldingSpaceWallpaperNudgeForceEligibilityEnabled();
    const bool accelerated_rate_limiting = features::
        IsHoldingSpaceWallpaperNudgeForceEligibilityAcceleratedRateLimitingEnabled();

    // If eligibility is forced for testing, don't suppress the nudge.
    if (forced_eligibility && !accelerated_rate_limiting) {
      return std::nullopt;
    }

    const auto* const session_controller = Shell::Get()->session_controller();
    PrefService* const prefs =
        session_controller->GetLastActiveUserPrefService();

    // If the user has ever pinned a file, suppress the nudge.
    if (!forced_eligibility &&
        holding_space_prefs::GetTimeOfFirstPin(prefs).has_value()) {
      return SuppressedReason::kUserHasPinned;
    }

    // If the user is ineligible, suppress the nudge.
    if (!(forced_eligibility || DetermineEligibility())) {
      return SuppressedReason::kUserNotEligible;
    }

    const bool should_limit_count =
        !forced_eligibility || accelerated_rate_limiting;

    // If the user has seen the nudge >= 3 times, suppress the nudge.
    if (should_limit_count &&
        holding_space_wallpaper_nudge_prefs::GetNudgeShownCount(prefs) >= 3u) {
      return SuppressedReason::kCountLimitReached;
    }

    const base::TimeDelta timeout =
        accelerated_rate_limiting ? base::Minutes(1) : base::Hours(24);
    const auto time_of_last_nudge =
        holding_space_wallpaper_nudge_prefs::GetLastTimeNudgeWasShown(prefs);

    // If the nudge was seen more recently than `timeout`, suppress the nudge.
    if (time_of_last_nudge.has_value() &&
        base::Time::Now() - time_of_last_nudge.value() < timeout) {
      return SuppressedReason::kTimePeriod;
    }

    // Otherwise, don't suppress the nudge.
    return std::nullopt;
  }

  // Used to track drag-and-drop sequences so as to facilitate the recording of
  // metrics on a once-per-sequence basis.
  DragDropSequenceTracker drag_drop_sequence_tracker_;

  // Used to observe the primary user's first pin to holding space so as to
  // facilitate the recording of metrics.
  PrefChangeRegistrar pref_change_registrar_;

  // A pointer to the `HoldingSpaceTray` anchoring the currently open help
  // bubble. Used to determine if the help bubble should be dismissed to prevent
  // overlap between the help bubble and `HoldingSpaceTrayBubble`. NOTE: Do not
  // dereference this pointer. It is for comparison only, as there is no
  // guarantee that this `HoldingSpaceTray` still exists.
  uintptr_t help_bubble_anchor_ = 0;

  // Used to observe a single drag-and-drop sequence once the user has dragged
  // a file from the Files app over the wallpaper.
  std::unique_ptr<ScopedDragDropObserver> drag_drop_observer_;

  // Used to ensure the shelf is visible on the active display while an
  // observed drag-and-drop sequence is in progress.
  std::unique_ptr<Shelf::ScopedDisableAutoHide> disable_shelf_auto_hide_;

  // Used to ensure that holding space is visible in the shelf on all displays
  // while an observed drag-and-drop sequence is in progress.
  std::unique_ptr<HoldingSpaceController::ScopedForceShowInShelf>
      force_holding_space_show_in_shelf_for_drag_;

  // Used to ensure that holding space is visible in the shelf on all displays
  // while the tray bubble is open.
  std::unique_ptr<HoldingSpaceController::ScopedForceShowInShelf>
      force_holding_space_show_in_shelf_for_tray_bubble_;

  // Used to close the help bubble on drop-to-pin.
  base::ScopedClosureRunner scoped_help_bubble_closer_;

  // The key that allows access to restricted `UserEducationController` APIs.
  UserEducationPrivateApiKey user_education_private_api_key_;

  // Used to highlight the wallpaper when data is dragged over it so that the
  // user better understands the wallpaper is a drop target.
  std::unique_ptr<Highlight> wallpaper_highlight_;

  // Observes the `HoldingSpaceController` to watch for tray bubble visibility.
  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      holding_space_controller_observer_{this};

  // Observes holding space metrics events in order to record downstream
  // wallpaper nudge experiment metrics.
  holding_space_metrics::ScopedObservation holding_space_metrics_observer_{
      this};

  // Observes session changes so that user eligibility can be saved after login.
  base::ScopedObservation<SessionController, SessionObserver> session_observer_{
      this};
};

}  // namespace

// HoldingSpaceWallpaperNudgeController ----------------------------------------

HoldingSpaceWallpaperNudgeController::HoldingSpaceWallpaperNudgeController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // Register our implementation as the singleton delegate for drag-and-drop
  // events over the wallpaper.
  WallpaperController::Get()->SetDragDropDelegate(
      std::make_unique<DragDropDelegate>(UserEducationPrivateApiKey()));
}

HoldingSpaceWallpaperNudgeController::~HoldingSpaceWallpaperNudgeController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
HoldingSpaceWallpaperNudgeController*
HoldingSpaceWallpaperNudgeController::Get() {
  return g_instance;
}

}  // namespace ash
