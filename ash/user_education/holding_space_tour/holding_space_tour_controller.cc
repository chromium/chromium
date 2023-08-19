// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_tour/holding_space_tour_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/scoped_drag_drop_observer.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/status_area_widget.h"
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
#include "base/pickle.h"
#include "base/scoped_observation.h"
#include "components/user_education/common/tutorial_description.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
HoldingSpaceTourController* g_instance = nullptr;

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
    layer()->SetName(HoldingSpaceTourController::kHighlightLayerName);

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
class DragDropDelegate : public WallpaperDragDropDelegate {
 private:
  // WallpaperDragDropDelegate:
  void GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* types) override {
    types->insert(FilesAppFormatType());
  }

  bool CanDrop(const ui::OSExchangeData& data) override {
    // Dropping `data` on the wallpaper has no effect unless doing so would
    // result in pinning of files to holding space.
    return !ExtractUnpinnedFilePaths(data).empty();
  }

  void OnDragEntered(const ui::OSExchangeData& data,
                     const gfx::Point& location_in_screen) override {
    // Highlight the wallpaper when `data` is dragged over it so that the user
    // better understands the wallpaper is a drop target.
    CHECK(!wallpaper_highlight_);
    wallpaper_highlight_ = std::make_unique<Highlight>(
        GetWallpaperViewNearestPoint(location_in_screen));

    // If the `drag_drop_observer_` already exists, we are already observing the
    // current drag-and-drop sequence and can no-op here.
    if (drag_drop_observer_) {
      return;
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
    // Dropping `data` on the wallpaper will have no effect unless doing so
    // would result in pinning of files to holding space.
    return CanDrop(data) ? ui::DragDropTypes::DragOperation::DRAG_COPY
                         : ui::DragDropTypes::DragOperation::DRAG_NONE;
  }

  void OnDragExited() override {
    // When `data` is dragged out of the wallpaper, remove the highlight which
    // was used to indicate the wallpaper was a drop target.
    CHECK(wallpaper_highlight_);
    wallpaper_highlight_.reset();
  }

  ui::mojom::DragOperation OnDrop(
      const ui::OSExchangeData& data,
      const gfx::Point& location_in_screen) override {
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

    absl::optional<gfx::Point> location_in_screen;

    if (event_type == ScopedDragDropObserver::EventType::kDragUpdated) {
      location_in_screen = event->root_location();
      wm::ConvertPointToScreen(
          static_cast<aura::Window*>(event->target())->GetRootWindow(),
          &location_in_screen.value());
    }

    OnDragOrDropEvent(std::move(location_in_screen));
  }

  void OnDragOrDropEvent(absl::optional<gfx::Point> location_in_screen) {
    // This code should only be reached if we are observing a drag-and-drop
    // sequence due to the user dragging a file from the Files app over the
    // wallpaper.
    CHECK(drag_drop_observer_);

    // If `location_in_screen` is absent, the observed drag-and-drop sequence
    // has been completed or cancelled. We can stop observing drag-and-drop
    // sequences and reset the shelf to its natural state.
    if (!location_in_screen) {
      drag_drop_observer_.reset();
      force_holding_space_show_in_shelf_.reset();

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

    // Ensure the shelf is visible on the active display while the observed
    // drag-and-drop sequence is in progress.
    if (!disable_shelf_auto_hide_ ||
        (disable_shelf_auto_hide_->weak_shelf() != shelf)) {
      disable_shelf_auto_hide_ =
          std::make_unique<Shelf::ScopedDisableAutoHide>(shelf);
    }

    // Ensure that holding space is visible in the shelf on all displays while
    // the observed drag-and-drop sequence is in progress.
    if (!force_holding_space_show_in_shelf_) {
      force_holding_space_show_in_shelf_ =
          std::make_unique<HoldingSpaceController::ScopedForceShowInShelf>();
    }

    // Cache the `holding_space_tray` nearest the `location_in_screen` so that
    // we can show an associated help bubble.
    // TODO(http://b/283169466): Rate limit showing the help bubble.
    HoldingSpaceTray* const holding_space_tray =
        GetHoldingSpaceTrayNearestPoint(location_in_screen.value());

    // Configure the help bubble.
    // TODO(http://b/283169365): Finalize strings.
    user_education::HelpBubbleParams help_bubble_params;
    help_bubble_params.arrow = user_education::HelpBubbleArrow::kBottomRight;
    help_bubble_params.body_text =
        u"[i18n] Drop files on the desktop to add them to Tote. You can't add "
        u"files to desktop.";
    help_bubble_params.extended_properties =
        user_education_util::CreateExtendedProperties(HelpBubbleStyle::kNudge);

    // While the help bubble is showing, do not allow either the associated
    // `shelf` or `holding_space_tray` to hide.
    // TODO(http://b/283171784): Explicitly close the help bubble if the user
    // opens holding space or successfully pins a file to holding space.
    base::OnceClosure close_callback = base::BindOnce(
        [](Shelf::ScopedDisableAutoHide*,
           HoldingSpaceController::ScopedForceShowInShelf*) {},
        base::Owned(std::make_unique<Shelf::ScopedDisableAutoHide>(shelf)),
        base::Owned(std::make_unique<
                    HoldingSpaceController::ScopedForceShowInShelf>()));

    // Attempt to show the help bubble.
    if (UserEducationHelpBubbleController::Get()->CreateHelpBubble(
            HelpBubbleId::kHoldingSpaceTour, std::move(help_bubble_params),
            kHoldingSpaceTrayElementId,
            views::ElementTrackerViews::GetContextForView(holding_space_tray),
            std::move(close_callback))) {
      // If successful in showing the help bubble, ping the `holding_space_tray`
      // to further attract the user's attention.
      UserEducationPingController::Get()->CreatePing(PingId::kHoldingSpaceTour,
                                                     holding_space_tray);
    }
  }

  // Used to observe a single drag-and-drop sequence once the user has dragged
  // a file from the Files app over the wallpaper.
  std::unique_ptr<ScopedDragDropObserver> drag_drop_observer_;

  // Used to ensure the shelf is visible on the active display while an
  // observed drag-and-drop sequence is in progress.
  std::unique_ptr<Shelf::ScopedDisableAutoHide> disable_shelf_auto_hide_;

  // Used to ensure that holding space is visible in the shelf on all displays
  // while an observed drag-and-drop sequence is in progress.
  std::unique_ptr<HoldingSpaceController::ScopedForceShowInShelf>
      force_holding_space_show_in_shelf_;

  // Used to highlight the wallpaper when data is dragged over it so that the
  // user better understands the wallpaper is a drop target.
  std::unique_ptr<Highlight> wallpaper_highlight_;
};

}  // namespace

// HoldingSpaceTourController --------------------------------------------------

HoldingSpaceTourController::HoldingSpaceTourController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // Register our implementation as the singleton delegate for drag-and-drop
  // events over the wallpaper.
  WallpaperController::Get()->SetDragDropDelegate(
      std::make_unique<DragDropDelegate>());
}

HoldingSpaceTourController::~HoldingSpaceTourController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
HoldingSpaceTourController* HoldingSpaceTourController::Get() {
  return g_instance;
}

// TODO(http://b/275909980): Implement tutorial descriptions.
std::map<TutorialId, user_education::TutorialDescription>
HoldingSpaceTourController::GetTutorialDescriptions() {
  std::map<TutorialId, user_education::TutorialDescription>
      tutorial_descriptions_by_id;
  tutorial_descriptions_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(TutorialId::kHoldingSpaceTourPrototype1),
      std::forward_as_tuple());
  tutorial_descriptions_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(TutorialId::kHoldingSpaceTourPrototype2),
      std::forward_as_tuple());
  return tutorial_descriptions_by_id;
}

}  // namespace ash
