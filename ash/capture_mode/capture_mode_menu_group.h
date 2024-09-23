// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_GROUP_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_GROUP_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "base/memory/raw_ptr.h"
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
// independent section in the settings menu. Each group can be created with a
// header that has an icon and a label for the group, or be header-less.
class ASH_EXPORT CaptureModeMenuGroup : public views::View {
  METADATA_HEADER(CaptureModeMenuGroup, views::View)

 public:
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

  // This version of the constructor creates a header-less menu group. Note that
  // menu groups without headers is not designed for settings that are managed
  // by policy. The `inside_border_insets` are used as paddings around the menu
  // options and items in this group.
  CaptureModeMenuGroup(Delegate* delegate,
                       const gfx::Insets& inside_border_insets);

  // If `managed_by_policy` is true, the header of this menu group will show an
  // enterprise-managed feature icon next to the `header_label`.
  CaptureModeMenuGroup(Delegate* delegate,
                       const gfx::VectorIcon& header_icon,
                       std::u16string header_label,
                       bool managed_by_policy = false);

  CaptureModeMenuGroup(const CaptureModeMenuGroup&) = delete;
  CaptureModeMenuGroup& operator=(const CaptureModeMenuGroup&) = delete;

  ~CaptureModeMenuGroup() override;

  // Returns true if this menu group is for a setting that is managed by a
  // policy set by admins.
  bool IsManagedByPolicy() const;

  // Adds an option which has an optional icon, label, and a checked image icon
  // to the the menu group. `option_icon` can be provided as `nullptr` if no
  // icon is desired for the option. When the option is selected, its checked
  // icon is visible. Otherwise its checked icon is invisible. One and only one
  // option's checked icon is visible in one menu group all the time.
  void AddOption(const gfx::VectorIcon* option_icon,
                 std::u16string option_label,
                 int option_id);

  // Deletes all options in `options_`.
  void DeleteOptions();

  // If an option with the given `option_id` exists, it will be updated with the
  // given `option_label`. Otherwise, a new option will be added. Note that
  // `option_icon` is optional and can be provided as `nullptr` if no icon is
  // desired for the option.
  void AddOrUpdateExistingOption(const gfx::VectorIcon* option_icon,
                                 std::u16string option_label,
                                 int option_id);

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
                   std::u16string item_label,
                   bool enabled);

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
  views::View* SetOptionCheckedForTesting(int option_id, bool checked) const;

 private:
  friend class CaptureModeSettingsTestApi;
  FRIEND_TEST_ALL_PREFIXES(CaptureModeSettingsTest, AccessibleName);

  // Acts as a common constructor that's called by the above public
  // constructors.
  CaptureModeMenuGroup(Delegate* delegate,
                       std::unique_ptr<CaptureModeMenuHeader> menu_header,
                       const gfx::Insets& inside_border_insets);

  // Returns the option whose ID is |option_id|, and nullptr if no such option
  // exists.
  CaptureModeOption* GetOptionById(int option_id) const;

  // This is the callback function on option click. It will select the
  // clicked/pressed button, and unselect any previously selected button.
  void HandleOptionClick(int option_id);

  views::View* menu_header() const;

  // CaptureModeSettingsView is the |delegate_| here. It's owned by
  // its views hierarchy.
  const raw_ptr<const Delegate> delegate_;

  // The menu header of `this`. It's owned by the views hierarchy. Can be null
  // if this group is header-less.
  raw_ptr<CaptureModeMenuHeader> menu_header_ = nullptr;

  // Options added via calls "AddOption()". Options are owned by theirs views
  // hierarchy.
  std::vector<raw_ptr<CaptureModeOption, VectorExperimental>> options_;

  // It's a container view for |options_|. It's owned by its views hierarchy.
  // We need it for grouping up options. For example, when user selects a custom
  // folder, we need to add it to the end of the options instead of adding it
  // after the menu item.
  raw_ptr<views::View> options_container_;

  // Menu items added by calling AddMenuItem().
  std::vector<raw_ptr<CaptureModeMenuItem, VectorExperimental>> menu_items_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_GROUP_H_
