// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_PROFILES_BUTTON_H_
#define ASH_WM_DESKS_DESK_PROFILES_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_context_menu.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

class DeskMiniView;

class ASH_EXPORT DeskProfilesButton : public views::ImageButton,
                                      public Desk::Observer {
  METADATA_HEADER(DeskProfilesButton, views::ImageButton)

 public:
  // Creates a DeskProfilesButton for desk `desk`.
  DeskProfilesButton(Desk* desk, DeskMiniView* desk_mini_view);
  DeskProfilesButton(const DeskProfilesButton&) = delete;
  DeskProfilesButton& operator=(const DeskProfilesButton&) = delete;
  ~DeskProfilesButton() override;

  // This is non-null when the profile menu is visible.
  DeskActionContextMenu* menu() { return context_menu_.get(); }

  // Desk::Observer:
  void OnContentChanged() override {}
  void OnDeskDestroyed(const Desk* desk) override;
  void OnDeskNameChanged(const std::u16string& new_name) override {}
  void OnDeskProfileChanged(uint64_t new_lacros_profile_id) override;

  // views::ImageButton:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

 private:
  void OnButtonPressed(const ui::Event& event);

  // Loads the icon that is currently associated with `desk_`.
  void LoadIconForProfile();

  // Helper function to create context menu when needed.
  void CreateMenu(gfx::Point location_in_screen,
                  ui::MenuSourceType menu_source);

  // Invoked when the context menu is closed.
  void OnMenuClosed();

  // Invoked when the user has selected a lacros profile from the context menu.
  void OnSetLacrosProfileId(uint64_t lacros_profile_id);

  // The associated desk.
  raw_ptr<Desk> desk_;

  // The mini view that owns this button.
  raw_ptr<DeskMiniView> mini_view_;

  // The context menu used to change the profile associated with the desk.
  std::unique_ptr<DeskActionContextMenu> context_menu_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_PROFILES_BUTTON_H_
