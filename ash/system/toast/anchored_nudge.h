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
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

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
                                 public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(AnchoredNudge);

  explicit AnchoredNudge(const AnchoredNudgeData& nudge_data);
  AnchoredNudge(const AnchoredNudge&) = delete;
  AnchoredNudge& operator=(const AnchoredNudge&) = delete;
  ~AnchoredNudge() override;

  // Getters for `system_nudge_view_` elements.
  views::ImageView* GetImageView();
  const std::u16string& GetBodyText();
  const std::u16string& GetTitleText();
  views::LabelButton* GetFirstButton();
  views::LabelButton* GetSecondButton();

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

  // Sets the arrow of the nudge based on the `shelf` alignment.
  void SetArrowFromShelf(Shelf* shelf);

  // Sets the default anchor rect for nudges that do not have an `anchor_view`.
  void SetDefaultAnchorRect();

  const std::string& id() { return id_; }

 private:
  // Unique id used to find and dismiss the nudge through the manager.
  const std::string id_;

  // Whether the nudge should set its arrow based on shelf alignment.
  const bool anchored_to_shelf_;

  // Owned by the views hierarchy. Contents view of the anchored nudge.
  raw_ptr<SystemNudgeView> system_nudge_view_ = nullptr;

  // Nudge action callbacks.
  NudgeClickCallback click_callback_;
  NudgeDismissCallback dismiss_callback_;

  // Used to maintain the shelf visible while a shelf-anchored nudge is shown.
  std::unique_ptr<Shelf::ScopedDisableAutoHide> disable_shelf_auto_hide_;

  // Used to observe hotseat state to update nudges default location baseline.
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};

  // Observes display configuration changes.
  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_
