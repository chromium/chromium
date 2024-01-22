// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_PROFILES_VIEW_H_
#define ASH_WM_DESKS_DESK_PROFILES_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/desk.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace ash {

class ASH_EXPORT DeskProfilesButton : public views::ImageButton,
                                      public Desk::Observer {
  METADATA_HEADER(DeskProfilesButton, views::ImageButton)

 public:
  // TestApi is used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(DeskProfilesButton* button) : button_(button) {}
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    ~TestApi() = default;

    // Wrapper function for testing.
    views::MenuItemView* GetMenuItemByID(int id);

   private:
    const raw_ptr<DeskProfilesButton> button_;
  };

  explicit DeskProfilesButton(views::Button::PressedCallback callback,
                              Desk* desk);
  DeskProfilesButton(const DeskProfilesButton&) = delete;
  DeskProfilesButton& operator=(const DeskProfilesButton&) = delete;
  ~DeskProfilesButton() override;

  const Desk* desk() const { return desk_; }

  void UpdateIcon();

  // If the context menu is currently open.
  bool IsMenuShowing() const;

  // Desk::Observer:
  void OnContentChanged() override {}
  void OnDeskDestroyed(const Desk* desk) override;
  void OnDeskNameChanged(const std::u16string& new_name) override {}

  // views::ImageButton:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  friend class DeskProfilesMenuModelAdapter;

  // This class is the context menu controller used by `DeskProfilesButton`.
  class MenuController;

  // Helper function to create context menu when needed.
  void CreateMenu(const ui::LocatedEvent& event);

  // The associated desk.
  raw_ptr<Desk> desk_;  // Not owned.
  raw_ptr<views::ImageView> icon_ = nullptr;
  gfx::ImageSkia icon_image_;

  // The context menu, which will be set as the controller to show the list of
  // profiles available for setting, and options to manage profiles.
  std::unique_ptr<MenuController> context_menu_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_PROFILES_VIEW_H_
