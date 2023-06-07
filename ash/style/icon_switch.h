// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ICON_SWITCH_H_
#define ASH_STYLE_ICON_SWITCH_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ui/views/view.h"

namespace ash {

// IconSwitch is a switch component with multiple icon buttons. When one button
// is toggled, the other buttons will be untoggled.
class ASH_EXPORT IconSwitch : public views::View, public IconButton::Delegate {
 public:
  METADATA_HEADER(IconSwitch);

  IconSwitch();
  IconSwitch(bool has_background,
             const gfx::Insets& inside_border_insets,
             int between_child_spacing);
  IconSwitch(const IconSwitch&) = delete;
  IconSwitch& operator=(const IconSwitch&) = delete;
  ~IconSwitch() override;

  // Adds a new button to the icon switch at the end to the right with given
  // callback, icon and tooltip text.
  IconButton* AddButton(IconButton::PressedCallback callback,
                        const gfx::VectorIcon* icon,
                        const std::u16string& tooltip_text);

  // Toggles the button on at given index.
  void ToggleButtonOnAtIndex(size_t index);

  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

  // IconButton::Delegate:
  void OnButtonToggled(IconButton* button) override;
  void OnButtonClicked(IconButton* button) override;

 private:
  // Updates the enabled state of toggle buttons, when the switch is
  // enabled/disabled.
  void OnEnableChanged();

  // Gets background color according to the current enabled state and theme.
  SkColor GetBackgroundColor() const;

  // If set to true, the icon switch will have a rounded rect background
  // wrapping all the toggle buttons. Otherwise, there is no background.
  bool has_background_;
  std::vector<IconButton*> buttons_;
  base::CallbackListSubscription enabled_changed_subscription_;
};

}  // namespace ash

#endif  // ASH_STYLE_ICON_SWITCH_H_
