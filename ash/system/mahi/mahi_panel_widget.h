// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

class MahiUiController;
class RefreshBannerView;

// The widget that contains the Mahi panel.
// TODO(b/319329379): Use this class in `CreatePanelWidget()` when resizing and
// closing capability is added.
class ASH_EXPORT MahiPanelWidget : public views::Widget,
                                   public ShelfObserver,
                                   views::ViewObserver {
 public:
  MahiPanelWidget(InitParams params, MahiUiController* ui_controller);

  MahiPanelWidget(const MahiPanelWidget&) = delete;
  MahiPanelWidget& operator=(const MahiPanelWidget&) = delete;

  ~MahiPanelWidget() override;

  // Creates and shows the panel widget with an animation. The widget is
  // positioned based on the provided `mahi_menu_bounds` and `display_id`.
  static views::UniqueWidgetPtr CreateAndShowPanelWidget(
      int64_t display_id,
      const gfx::Rect& mahi_menu_bounds,
      MahiUiController* ui_controller);

  static const char* GetName();

 private:
  // ShelfObserver:
  void OnShelfWorkAreaInsetsChanged() override;

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  bool is_refresh_view_visible_ = false;

  // Owned by views hierarchy.
  raw_ptr<RefreshBannerView> refresh_view_ = nullptr;

  base::ScopedObservation<views::View, views::ViewObserver>
      refresh_view_observation_{this};

  // Used to observe the work area bounds to update the panel bounds.
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_
