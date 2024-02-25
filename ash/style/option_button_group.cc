// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/option_button_group.h"

#include "ash/style/option_button_base.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Insets kDefaultInsideBorderInsets(2);
constexpr int kDefaultChildSpacing = 0;

}  // namespace

OptionButtonGroup::OptionButtonGroup(int group_width)
    : OptionButtonGroup(group_width,
                        kDefaultInsideBorderInsets,
                        kDefaultChildSpacing,
                        OptionButtonBase::kDefaultPadding,
                        OptionButtonBase::kImageLabelSpacingDP) {}

OptionButtonGroup::OptionButtonGroup(int group_width,
                                     const gfx::Insets& inside_border_insets,
                                     int between_child_spacing,
                                     const gfx::Insets& option_button_padding,
                                     int image_label_spacing)
    : group_width_(group_width),
      inside_border_insets_(inside_border_insets),
      button_padding_(option_button_padding),
      image_label_spacing_(image_label_spacing) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, inside_border_insets,
      between_child_spacing));

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      &OptionButtonGroup::OnEnableChanged, base::Unretained(this)));
}

OptionButtonGroup::~OptionButtonGroup() = default;

void OptionButtonGroup::SelectButtonAtIndex(size_t index) {
  DCHECK_LT(index, buttons_.size());
  buttons_[index]->SetSelected(true);
}

std::vector<OptionButtonBase*> OptionButtonGroup::GetSelectedButtons() {
  std::vector<OptionButtonBase*> selected_buttons;

  for (ash::OptionButtonBase* button : buttons_) {
    if (button->selected())
      selected_buttons.push_back(button);
  }

  return selected_buttons;
}

void OptionButtonGroup::OnEnableChanged() {
  const bool enabled = GetEnabled();

  for (ash::OptionButtonBase* button : buttons_) {
    button->SetEnabled(enabled);
  }
}

BEGIN_METADATA(OptionButtonGroup)
END_METADATA

}  // namespace ash
