// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MODE_PIE_MENU_VIEW_H_
#define ASH_WM_MODE_PIE_MENU_VIEW_H_

#include <stack>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageButton;
}  // namespace views

namespace ash {

class PieMenuButton;
class PieMenuView;

// -----------------------------------------------------------------------------
// PieSubMenuContainerView:

// Defines a container for buttons representing menu items in a pie menu.
class ASH_EXPORT PieSubMenuContainerView : public views::View {
  METADATA_HEADER(PieSubMenuContainerView, views::View)

 public:
  PieSubMenuContainerView(const PieSubMenuContainerView&) = delete;
  PieSubMenuContainerView& operator=(const PieSubMenuContainerView&) = delete;
  ~PieSubMenuContainerView() override;

  size_t button_count() const { return buttons_.size(); }

  // Adds a new menu item button with the given `button_id`,
  // `button_label_text`, and an optional `icon` (if non-null). The `button_id`
  // must be unique among all the IDs of the current existing buttons hosted by
  // parent `PieMenuView` and all its sub menus.
  // Returns a pointer to the newly added button.
  views::View* AddMenuButton(int button_id,
                             const std::u16string& button_label_text,
                             const gfx::VectorIcon* icon);

  // Removes all the currently added buttons in this container. This can be used
  // to rebuild the contents of this sub menu from scratch.
  void RemoveAllButtons();

 private:
  friend class PieMenuView;

  explicit PieSubMenuContainerView(PieMenuView* owner_menu_view);

  // The parent owner pie menu that hosts this container. Not null.
  const raw_ptr<PieMenuView> owner_menu_view_;

  // The buttons on this container, which will be painted as slices of a circle
  // in their same order in this vector.
  std::vector<raw_ptr<PieMenuButton, VectorExperimental>> buttons_;
};

// -----------------------------------------------------------------------------
// PieMenuView:

// Defines a pie menu that lists its menu items as radial slices of a circle.
// Each menu item button can open its sub menu with its own buttons, replacing
// the current content of this pie view. A back button is then shown to go back
// to the previous sub menu in the stack. There is always a main menu container
// at all times.
class ASH_EXPORT PieMenuView : public views::View {
  METADATA_HEADER(PieMenuView, views::View)

 public:
  // Defines an interface for the delegate of this class which will be informed
  // when a button on this pie view is pressed.
  class Delegate {
   public:
    // Called when the button with the given `button_id` is pressed.
    virtual void OnPieMenuButtonPressed(int button_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit PieMenuView(Delegate* delegate);
  PieMenuView(const PieMenuView&) = delete;
  PieMenuView& operator=(const PieMenuView&) = delete;
  ~PieMenuView() override;

  PieSubMenuContainerView* main_menu_container() {
    return main_menu_container_;
  }

  // Returns the `PieSubMenuContainerView` that the button whose ID is
  // `button_id` opens when pressed if any, or creates one for it and associates
  // it with that button. A button with `button_id` must exist in this pie menu.
  PieSubMenuContainerView* GetOrAddSubMenuForButton(int button_id);

  // Sets the label of the button whose ID is `button_id` to the given `text`.
  // A button with `button_id` must exist in this pie menu.
  void SetButtonLabelText(int button_id, const std::u16string& text);

  // Pops all the sub menus (if any) and shows the main menu.
  void ReturnToMainMenu();

  // Similar to `GetButtonById()` below but returns the button as a
  // `views::View`. This prevents the need for exposing the actual type of
  // `PieMenuButton`.
  views::View* GetButtonByIdAsView(int button_id) const;

  // Gets the center point in screen coordinates of the contents of the button
  // whose ID is `button_id`. If no such button is found, an empty point is
  // returned.
  gfx::Point GetButtonContentsCenterInScreen(int button_id) const;

  // views::View:
  void Layout(PassKey) override;
  void AddedToWidget() override;
  void OnThemeChanged() override;

 private:
  friend class PieSubMenuContainerView;

  // Called when the given `button` is added on any of the sub menu containers
  // hosted by this view.
  void OnPieMenuButtonAdded(PieMenuButton* button);

  // Called when the given `button` is removed from any of the sub menu
  // containers hosted by this view.
  void OnPieMenuButtonRemoved(PieMenuButton* button);

  // Called when the given `button` is pressed. This will inform the `delegate_`
  // via the `OnPieMenuButtonPressed()` API.
  void OnPieMenuButtonPressed(PieMenuButton* button);

  // Opens the given `sub_menu` by making it the only visible sub menu container
  // which visually replaces the contents of this pie view with the buttons
  // hosted by the given `sub_menu`. The `back_button_` will be shown to allow
  // the user to go back to the previous sub menu in `active_sub_menus_stack_`
  // (if any) or to the `main_menu_container_`.
  void OpenSubMenu(PieSubMenuContainerView* sub_menu);

  // If there are active sub menus in `active_sub_menus_stack_`, this function
  // pops the top-most one such that the previous sub menu shows. Finally when
  // there are no more sub menus, the `main_menu_container_` will show, and the
  // `back_button_` will hide.
  void MaybePopSubMenu();

  // Returns the button whose ID is `button_id` or nullptr if it doesn't exist.
  PieMenuButton* GetButtonById(int button_id) const;

  // The delegate of this view which takes care of handling button presses. Not
  // null.
  const raw_ptr<Delegate, DanglingUntriaged> delegate_;

  // The container hosting the buttons on the main menu of this view. When this
  // is visible, `active_sub_menus_stack_` should be empty, and `back_button_`
  // should be hidden.
  const raw_ptr<PieSubMenuContainerView> main_menu_container_;

  // Since a button on a sub menu can open another sub menu and so on, we keep
  // a stack of currently active sub menus (other than the main menu), so that
  // pressing on `back_button_` pops the top-most sub menu to show the previous
  // one, until there are no more active sub menus, at which point
  // `main_menu_container_` shows up and `back_button_` hides.
  std::stack<raw_ptr<PieSubMenuContainerView, CtnExperimental>>
      active_sub_menus_stack_;

  const raw_ptr<views::ImageButton> back_button_;

  // Maps all the buttons on all sub menu containers of this view by their IDs.
  base::flat_map</*button_id=*/int, raw_ptr<PieMenuButton, CtnExperimental>>
      buttons_by_id_;
};

}  // namespace ash

#endif  // ASH_WM_MODE_PIE_MENU_VIEW_H_
