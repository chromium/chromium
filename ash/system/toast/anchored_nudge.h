// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_
#define ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell_observer.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ui {
class GestureEvent;
class MouseEvent;
}  // namespace ui

namespace aura {
class Window;
}

namespace ash {

class SystemNudgeView;

// Creates and manages the widget and contents view for an anchored nudge.
// TODO(b/285988235): `AnchoredNudge` will replace the existing `SystemNudge`
// and take over its name.
class ASH_EXPORT AnchoredNudge : public display::DisplayObserver,
                                 public ShelfObserver,
                                 public ShellObserver,
                                 public views::BubbleDialogDelegateView,
                                 public views::WidgetObserver {
  METADATA_HEADER(AnchoredNudge, views::BubbleDialogDelegateView)

 public:
  AnchoredNudge(AnchoredNudgeData& nudge_data,
                base::RepeatingCallback<void(/*has_hover_or_focus=*/bool)>
                    hover_or_focus_changed_callback);
  AnchoredNudge(const AnchoredNudge&) = delete;
  AnchoredNudge& operator=(const AnchoredNudge&) = delete;
  ~AnchoredNudge() override;

  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  // views::View:
  void AddedToWidget() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // Sets the arrow of the nudge based on the `shelf` alignment.
  void SetArrowFromShelf(Shelf* shelf);

  // Sets the default anchor rect for nudges that do not have an `anchor_view`.
  void SetDefaultAnchorRect();

  const std::string& id() const { return id_; }

  NudgeCatalogName catalog_name() const { return catalog_name_; }

 private:
  // Unique id used to find and dismiss the nudge through the manager.
  const std::string id_;

  // Used to identify nudges that share an id but have different catalog names.
  const NudgeCatalogName catalog_name_;

  // Whether the nudge should set its arrow based on shelf alignment.
  const bool anchored_to_shelf_;

  // Whether the nudge should set its bounds anchored by its corners.
  const bool is_corner_anchored_;

  // Whether the nudge should set its parent as the `anchor_view`.
  const bool set_anchor_view_as_parent_ = false;

  // If not null, the nudge will anchor to one of the anchor widget internal
  // corners. Currently only supports anchoring to the bottom corners.
  raw_ptr<views::Widget> anchor_widget_ = nullptr;

  // The corner of the `anchor_widget_` to which the nudge will anchor.
  views::BubbleBorder::Arrow anchor_widget_corner_ =
      views::BubbleBorder::Arrow::BOTTOM_LEFT;

  // Owned by the views hierarchy. Contents view of the anchored nudge.
  raw_ptr<SystemNudgeView> system_nudge_view_ = nullptr;

  // Nudge action callbacks.
  NudgeClickCallback click_callback_;
  NudgeDismissCallback dismiss_callback_;

  // Used to maintain the shelf visible while a shelf-anchored nudge is shown.
  std::unique_ptr<Shelf::ScopedDisableAutoHide> disable_shelf_auto_hide_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      anchor_widget_scoped_observation_{this};

  // Used to observe hotseat state to update nudges default location baseline.
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};

  // Observes display configuration changes.
  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_
