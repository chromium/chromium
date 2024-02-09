// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_OPTION_BUTTON_GROUP_H_
#define ASH_STYLE_OPTION_BUTTON_GROUP_H_

#include "ash/ash_export.h"
#include "ash/style/option_button_base.h"
#include "base/memory/raw_ptr.h"

namespace ash {

// OptionButtonGroup is a menu component with a group of option buttons with the
// vertical layout.
class ASH_EXPORT OptionButtonGroup : public views::View {
  METADATA_HEADER(OptionButtonGroup, views::View)

 public:
  explicit OptionButtonGroup(int group_width);
  OptionButtonGroup(int group_width,
                    const gfx::Insets& inside_border_insets,
                    int between_child_spacing,
                    const gfx::Insets& option_button_padding,
                    int image_label_spacing);
  OptionButtonGroup(const OptionButtonGroup&) = delete;
  OptionButtonGroup& operator=(const OptionButtonGroup&) = delete;
  ~OptionButtonGroup() override;

  // Adds a new button to the option button group at the end to the bottom with
  // given callback and label.
  virtual OptionButtonBase* AddButton(
      OptionButtonBase::PressedCallback callback,
      const std::u16string& label) = 0;

  // Selects the button  at given `index`.
  void SelectButtonAtIndex(size_t index);

  // Returns all the selected buttons.
  std::vector<OptionButtonBase*> GetSelectedButtons();

 protected:
  // Updates the enabled state of option buttons, when the switch is
  void OnEnableChanged();

  // The width of the option button group.
  const int group_width_;

  // The padding insets of the option button group.
  const gfx::Insets inside_border_insets_;

  // The padding insets of the buttons.
  const gfx::Insets button_padding_;

  // The padding between the icon and label.
  const int image_label_spacing_;

  std::vector<raw_ptr<OptionButtonBase, VectorExperimental>> buttons_;
  base::CallbackListSubscription enabled_changed_subscription_;
};

}  // namespace ash

#endif  // ASH_STYLE_OPTION_BUTTON_GROUP_H_