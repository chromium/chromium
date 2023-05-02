// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_tour/holding_space_tour_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/scoped_drag_drop_observer.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/user_education/user_education_types.h"
#include "ash/wallpaper/wallpaper_drag_drop_delegate.h"
#include "base/check_op.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/files/file_path.h"
#include "base/pickle.h"
#include "components/user_education/common/tutorial_description.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
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

  ui::mojom::DragOperation OnDrop(
      const ui::OSExchangeData& data,
      const gfx::Point& location_in_screen) override {
    HoldingSpaceClient* const client = HoldingSpaceController::Get()->client();
    if (!client) {
      return ui::mojom::DragOperation::kNone;
    }

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
    GetShelfNearestPoint(location_in_screen)
        ->status_area_widget()
        ->holding_space_tray()
        ->ShowBubble();

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
      disable_shelf_auto_hide_.reset();
      force_holding_space_show_in_shelf_.reset();
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
