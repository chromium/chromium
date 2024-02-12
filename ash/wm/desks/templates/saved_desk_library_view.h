// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_LIBRARY_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_LIBRARY_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DeskMiniView;
class DeskTemplate;
class SavedDeskGridView;
class SavedDeskItemView;
class SavedDeskLibraryEventHandler;
class SavedDeskLibraryWindowTargeter;
class ScrollViewGradientHelper;

// This view is the content of the saved desk library widget. Depending on which
// saved desk features are enabled, it can show one or more `SavedDeskGridView`s
// that each hold a number of saved desks. It is owned by the `OverviewGrid`.
class SavedDeskLibraryView : public views::View, public aura::WindowObserver {
  METADATA_HEADER(SavedDeskLibraryView, views::View)

 public:
  SavedDeskLibraryView();
  SavedDeskLibraryView(const SavedDeskLibraryView&) = delete;
  SavedDeskLibraryView& operator=(const SavedDeskLibraryView&) = delete;
  ~SavedDeskLibraryView() override;

  // Creates and returns the widget that contains the SavedDeskLibraryView in
  // overview mode. This does not show the widget.
  static std::unique_ptr<views::Widget> CreateSavedDeskLibraryWidget(
      aura::Window* root);

  const std::vector<raw_ptr<SavedDeskGridView, VectorExperimental>>&
  grid_views() const {
    return grid_views_;
  }

  // Retrieves the item view for a given saved desk, or nullptr.
  SavedDeskItemView* GetItemForUUID(const base::Uuid& uuid);

  // Updates existing saved desks and adds new saved desks to the grid. Also
  // sorts entries in alphabetical order. If `order_first_uuid` is valid, the
  // corresponding entry will be placed first. This will animate the entries to
  // their final positions if `animate` is true. Currently only allows a maximum
  // of 6 saved desks to be shown in the grid.
  void AddOrUpdateEntries(
      const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>&
          entries,
      const base::Uuid& order_first_uuid,
      bool animate);

  // Deletes all entries identified by `uuids`. If `delete_animation` is false,
  // then the respective item views will just disappear instead of fading out.
  void DeleteEntries(const std::vector<base::Uuid>& uuids,
                     bool delete_animation);

  // This performs the launch animation for Save & Recall. The `DeskItemView`
  // identified by `uuid` is animated up into the position of the desk preview
  // housed in `mini_view`. It then crossfades into the desk preview. The
  // `DeskItemView` is also removed from the grid.
  void AnimateDeskLaunch(const base::Uuid& uuid, DeskMiniView* mini_view);

 private:
  friend class SavedDeskLibraryEventHandler;
  friend class SavedDeskLibraryViewTestApi;
  friend class SavedDeskLibraryWindowTargeter;

  bool IsAnimating() const;

  // Called from `SavedDeskLibraryWindowTargeter`. Returns true if
  // `screen_location` intersects with an interactive part of the library UI.
  bool IntersectsWithUi(const gfx::Point& screen_location) const;

  // If this view is attached to a widget, returns its window (or nullptr).
  aura::Window* GetWidgetWindow();

  void OnLocatedEvent(ui::LocatedEvent* event, bool is_touch);

  // This returns the screen space bounds of the desk preview that `mini_view`
  // holds. It is intended to be called when launching a Save & Recall desk so
  // that the `SavedDeskItemView` can be animated up to the desk bar view.
  // It takes animation into consideration and will return the position where
  // the desk preview will end up, rather than where it currently is.
  std::optional<gfx::Rect> GetDeskPreviewBoundsForLaunch(
      const DeskMiniView* mini_view);

  // views::View:
  void AddedToWidget() override;
  void Layout(PassKey) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Pointers to the grids with saved desks of specific types. These will be set
  // depending on which features are enabled.
  raw_ptr<SavedDeskGridView> desk_template_grid_view_ = nullptr;
  raw_ptr<SavedDeskGridView> save_and_recall_grid_view_ = nullptr;

  // Used for scroll functionality of the library page. Owned by views
  // hierarchy.
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;

  // Adds a fade in/out gradient to the top/bottom of `scroll_view_`.
  std::unique_ptr<ScrollViewGradientHelper> scroll_view_gradient_helper_;

  // Holds the active ones, for convenience.
  std::vector<raw_ptr<SavedDeskGridView, VectorExperimental>> grid_views_;

  // Label that shows up when the library has no items.
  raw_ptr<views::Label> no_items_label_ = nullptr;

  // Handles mouse/touch events on saved desk library widget.
  std::unique_ptr<SavedDeskLibraryEventHandler> event_handler_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_LIBRARY_VIEW_H_
