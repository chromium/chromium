// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_tour/holding_space_tour_controller.h"

#include <memory>
#include <string>

#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/scoped_drag_drop_observer.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_types.h"
#include "ash/wallpaper/wallpaper_drag_drop_delegate.h"
#include "base/check_op.h"
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
    // TODO(http://b/279031685): Ask Files app team to own a util API for this.
    base::Pickle pickled_data;
    if (data.GetPickledData(FilesAppFormatType(), &pickled_data)) {
      std::u16string file_system_sources;
      ui::ReadCustomDataForType(pickled_data.data(), pickled_data.size(),
                                u"fs/sources", &file_system_sources);
      return !file_system_sources.empty();
    }
    return true;
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

  void OnDropTargetEvent(const ui::DropTargetEvent* event) {
    // This code should only be reached if we are observing a drag-and-drop
    // sequence due to the user dragging a file from the Files app over the
    // wallpaper.
    CHECK(drag_drop_observer_);

    absl::optional<gfx::Point> location_in_screen;

    if (event) {
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
