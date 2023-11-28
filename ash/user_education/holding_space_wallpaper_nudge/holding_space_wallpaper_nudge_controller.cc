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
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"
#include "ash/user_education/user_education_help_bubble_controller.h"
#include "ash/user_education/user_education_ping_controller.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_drag_drop_delegate.h"
#include "base/check_op.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
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

// TODO(http://b/283169365): Finalize strings.
std::u16string GetBubbleBodyText() {
  return features::IsHoldingSpaceWallpaperNudgeDropToPinEnabled()
             ? u"[i18n] Drop files on the desktop to add them to Tote. You "
               u"can't add files to desktop."
             : u"[i18n] Keep important files in Tote instead of on the "
               u"desktop. Just drag files to Tote.";
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

WallpaperView* GetWallpaperViewNearestPoint(
    const gfx::Point& location_in_screen) {
  return RootWindowController::ForWindow(
             GetRootWindowForDisplayId(
                 GetDisplayNearestPoint(location_in_screen).id()))
      ->wallpaper_widget_controller()
      ->wallpaper_view();
}

// Indicates whether the nudge should be shown based on when it was last shown
// and how many times total it's been shown. It should be no more than once
// in a 24 hour period, and no more than 3 times total.
bool NudgeShouldBeShown() {
  if (!features::IsHoldingSpaceWallpaperNudgeRateLimitingEnabled()) {
    return true;
  }

  PrefService* const prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  const auto time_of_last_nudge =
      holding_space_wallpaper_nudge_prefs::GetLastTimeNudgeWasShown(prefs);
  const auto nudge_shown_count =
      holding_space_wallpaper_nudge_prefs::GetNudgeShownCount(prefs);

  bool nudge_shown_recently =
      time_of_last_nudge.has_value() &&
      base::Time::Now() - time_of_last_nudge.value() < base::Hours(24);

  return nudge_shown_count < 3u && !nudge_shown_recently;
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
                         public HoldingSpaceControllerObserver {
 private:
  // WallpaperDragDropDelegate:
  void GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* types) override {
    types->insert(FilesAppFormatType());
  }

  bool CanDrop(const ui::OSExchangeData& data) override {
    // If this `data` can be pinned to holding space, return true to make sure
    // we can track the drag to show the nudge appropriately, even if
    // drop-to-pin is not enabled.
    return !ExtractUnpinnedFilePaths(data).empty();
  }

  void OnDragEntered(const ui::OSExchangeData& data,
                     const gfx::Point& location_in_screen) override {
    if (features::IsHoldingSpaceWallpaperNudgeEnabledCounterfactually()) {
      if (NudgeShouldBeShown()) {
        // Mark the nudge as "shown" for the counterfactual experiment arm.
        holding_space_wallpaper_nudge_prefs::MarkNudgeShown(
            Shell::Get()->session_controller()->GetLastActiveUserPrefService());
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

    // Immediately close the help bubble so that it does not block the holding
    // space. If it has already closed, e.g. due to timeout, the internal
    // callback will have already been canceled and no-op.
    scoped_help_bubble_closer_.RunAndReset();

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
    client->PinFiles(unpinned_file_paths);

    // Open the holding space tray so that the user can see the newly pinned
    // files and understands the relationship between the action they took on
    // the wallpaper and its effect in holding space.
    GetHoldingSpaceTrayNearestPoint(location_in_screen)->ShowBubble();

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
      location_in_screen = event->root_location();
      wm::ConvertPointToScreen(
          static_cast<aura::Window*>(event->target())->GetRootWindow(),
          &location_in_screen.value());
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

    // Ensure that holding space is visible in the shelf on all displays while
    // the observed drag-and-drop sequence is in progress.
    if (!force_holding_space_show_in_shelf_for_drag_) {
      force_holding_space_show_in_shelf_for_drag_ =
          std::make_unique<HoldingSpaceController::ScopedForceShowInShelf>();
    }

    if (!NudgeShouldBeShown() || help_bubble_anchor_) {
      return;
    }

    // Ensure the shelf is visible on the active display while the observed
    // drag-and-drop sequence is in progress.
    if (!disable_shelf_auto_hide_) {
      disable_shelf_auto_hide_ =
          std::make_unique<Shelf::ScopedDisableAutoHide>(shelf);
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
        [](Shelf::ScopedDisableAutoHide*,
           HoldingSpaceController::ScopedForceShowInShelf*,
           const base::AutoReset<uintptr_t>&) {},
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
      holding_space_wallpaper_nudge_prefs::MarkNudgeShown(
          Shell::Get()->session_controller()->GetLastActiveUserPrefService());

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
      force_holding_space_show_in_shelf_for_tray_bubble_ =
          std::make_unique<HoldingSpaceController::ScopedForceShowInShelf>();

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

  // Used to highlight the wallpaper when data is dragged over it so that the
  // user better understands the wallpaper is a drop target.
  std::unique_ptr<Highlight> wallpaper_highlight_;

  // Observes the `HoldingSpaceController` to watch for tray bubble visibility.
  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      holding_space_controller_observer_{this};
};

}  // namespace

// HoldingSpaceWallpaperNudgeController ----------------------------------------

HoldingSpaceWallpaperNudgeController::HoldingSpaceWallpaperNudgeController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // Register our implementation as the singleton delegate for drag-and-drop
  // events over the wallpaper.
  WallpaperController::Get()->SetDragDropDelegate(
      std::make_unique<DragDropDelegate>());
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
