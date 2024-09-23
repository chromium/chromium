// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BUTTON_DESK_BUTTON_CONTAINER_H_
#define ASH_WM_DESKS_DESK_BUTTON_DESK_BUTTON_CONTAINER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash {

class Desk;
class DeskButton;
class DeskButtonWidget;
class DeskSwitchButton;
class SessionController;
class Shelf;

class ASH_EXPORT DeskButtonContainer : public DeskProfilesDelegate::Observer,
                                       public DesksController::Observer,
                                       public SessionObserver,
                                       public views::View {
  METADATA_HEADER(DeskButtonContainer, views::View)

 public:
  DeskButtonContainer();
  DeskButtonContainer(const DeskButtonContainer&) = delete;
  DeskButtonContainer& operator=(const DeskButtonContainer&) = delete;
  ~DeskButtonContainer() override;

  static bool ShouldShowDeskProfilesUi();

  static int GetMaxLength(bool zero_state);

  bool zero_state() const { return zero_state_; }
  void set_zero_state(bool zero_state) { zero_state_ = zero_state; }
  Shelf* shelf() const { return shelf_; }
  DeskButtonWidget* desk_button_widget() const { return desk_button_widget_; }
  DeskButton* desk_button() const { return desk_button_; }
  DeskSwitchButton* prev_desk_button() const { return prev_desk_button_; }
  DeskSwitchButton* next_desk_button() const { return next_desk_button_; }

  // DeskProfilesDelegate::Observer:
  void OnProfileUpsert(const LacrosProfileSummary& summary) override;
  void OnProfileRemoved(uint64_t profile_id) override;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk, bool from_undo) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskReordered(int old_index, int new_index) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override;

  // SessionObserver:
  void OnFirstSessionStarted() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;

  // Invoked after the shelf alignment change. It's used to update the container
  // UI properly.
  void PrepareForAlignmentChange();

  // Returns the preferred length for the container. The numeric value indicates
  // the required width for horizontal shelf or the required height for side
  // shelf. It's used by the widget to layout contents properly.
  int GetPreferredLength() const;

  // Returns true if the desk button UI is visible and the given
  // `screen_location` intersects with the UI.
  bool IntersectsWithDeskButtonUi(const gfx::Point& screen_location) const;

  std::u16string GetTitleForView(const views::View* view) const;

  // Initializes the view. Must be called before any meaningful UIs can be laid
  // out.
  void Init(DeskButtonWidget* desk_button_widget);

  // Updates UI status without re-layout.
  void UpdateUi(const Desk* active_desk);

  // Updates UI and updates layout if preferred size changes.
  void UpdateUiAndLayoutIfNeeded(const Desk* active_desk);

  // Updates locale-specific settings within this container.
  void HandleLocaleChange();

  // Shows the context menu for `source` and `event` when the desk button is
  // *not* activated. Please note, it re-uses the shelf view as the context menu
  // controller so that they show the same menu items.
  void MaybeShowContextMenu(views::View* source, ui::LocatedEvent* event);

 private:
  bool zero_state_ = false;
  raw_ptr<Shelf> shelf_ = nullptr;
  raw_ptr<DeskButtonWidget> desk_button_widget_ = nullptr;
  raw_ptr<DeskButton> desk_button_ = nullptr;
  raw_ptr<DeskSwitchButton> prev_desk_button_ = nullptr;
  raw_ptr<DeskSwitchButton> next_desk_button_ = nullptr;
  base::ScopedObservation<DesksController, DesksController::Observer>
      desks_observation_{this};
  base::ScopedObservation<DeskProfilesDelegate, DeskProfilesDelegate::Observer>
      desk_profiles_observer_{this};
  base::ScopedObservation<SessionController, SessionObserver> session_observer_{
      this};
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, DeskButtonContainer, views::View)
VIEW_BUILDER_METHOD(Init, DeskButtonWidget*)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ash::DeskButtonContainer)

#endif  // ASH_WM_DESKS_DESK_BUTTON_DESK_BUTTON_CONTAINER_H_
