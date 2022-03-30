// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_GROUP_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_GROUP_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

class CaptureModeMenuHeader;
class CaptureModeMenuItem;
class CaptureModeOption;

// Defines a view that groups together related capture mode settings in an
// independent section in the settings menu. Each group has a header icon and a
// header label.
class ASH_EXPORT CaptureModeMenuGroup : public views::View {
 public:
  METADATA_HEADER(CaptureModeMenuGroup);

  class Delegate {
   public:
    // Called when user selects an option.
    virtual void OnOptionSelected(int option_id) const = 0;

    // Called to determine if an option with the given `option_id` is selected.
    virtual bool IsOptionChecked(int option_id) const = 0;

    // Called to determine if an option with the given `option_id` is enabled.
    virtual bool IsOptionEnabled(int option_id) const = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // If `managed_by_policy` is true, the header of this menu group will show an
  // enterprise-managed feature icon next to the `header_label`.
  CaptureModeMenuGroup(Delegate* delegate,
                       const gfx::VectorIcon& header_icon,
                       std::u16string header_label,
                       bool managed_by_policy = false);
  CaptureModeMenuGroup(const CaptureModeMenuGroup&) = delete;
  CaptureModeMenuGroup& operator=(const CaptureModeMenuGroup&) = delete;
  ~CaptureModeMenuGroup() override;

  // Adds an option which has text and a checked image icon to the the menu
  // group. When the option is selected, its checked icon is visible. Otherwise
  // its checked icon is invisible. One and only one option's checked icon is
  // visible all the time.
  void AddOption(std::u16string option_label, int option_id);

  // Deletes all options in `options_`.
  void DeleteOptions();

  // If an option with the given |option_id| exists, it will be updated with the
  // given |option_label|. Otherwise, a new option will be added.
  void AddOrUpdateExistingOption(std::u16string option_label, int option_id);

  // Refreshes which options are currently selected and showing checked icons
  // next to their labels. This calls back into the |Delegate| to check each
  // option's selection state.
  void RefreshOptionsSelections();

  // Removes an option with the given |option_id| if it exists. Does nothing
  // otherwise.
  void RemoveOptionIfAny(int option_id);

  // Adds a menu item which has text to the menu group. Each menu item can have
  // its own customized behavior. For example, file save menu group's menu item
  // will open a folder window for user to select a new folder to save the
  // captured filed on click/press.
  void AddMenuItem(views::Button::PressedCallback callback,
                   std::u16string item_label);

  // Returns true if the option with the given `option_id` is checked, if such
  // option exists.
  bool IsOptionChecked(int option_id) const;

  // Returns true if the option with the given `option_id` is enabled, if such
  // option exists.
  bool IsOptionEnabled(int option_id) const;

  // Appends the enabled items from `options_` and `menu_items_` to the given
  // `highlightable_items`.
  void AppendHighlightableItems(
      std::vector<CaptureModeSessionFocusCycler::HighlightableView*>&
          highlightable_items);

  // For tests only.
  views::View* GetOptionForTesting(int option_id);
  views::View* GetSelectFolderMenuItemForTesting();
  std::u16string GetOptionLabelForTesting(int option_id) const;

 private:
  friend class CaptureModeSettingsTestApi;

  // Returns the option whose ID is |option_id|, and nullptr if no such option
  // exists.
  CaptureModeOption* GetOptionById(int option_id) const;

  // This is the callback function on option click. It will select the
  // clicked/pressed button, and unselect any previously selected button.
  void HandleOptionClick(int option_id);

  // CaptureModeSettingsView is the |delegate_| here. It's owned by
  // its views hierarchy.
  const Delegate* const delegate_;

  // The menu header of `this`. It's owned by the views hierarchy.
  CaptureModeMenuHeader* menu_header_;

  // Options added via calls "AddOption()". Options are owned by theirs views
  // hierarchy.
  std::vector<CaptureModeOption*> options_;

  // It's a container view for |options_|. It's owned by its views hierarchy.
  // We need it for grouping up options. For example, when user selects a custom
  // folder, we need to add it to the end of the options instead of adding it
  // after the menu item.
  views::View* options_container_;

  // Menu items added by calling AddMenuItem().
  std::vector<CaptureModeMenuItem*> menu_items_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_GROUP_H_
