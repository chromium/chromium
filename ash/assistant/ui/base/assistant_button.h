// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_
#define ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ButtonListener;
class ImageButton;
}  // namespace views

namespace ash {

enum class AssistantButtonId;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantButton
    : public views::ImageButton,
      public views::ButtonListener {
 public:
  AssistantButton(views::ButtonListener* listener, AssistantButtonId button_id);
  ~AssistantButton() override;

  // Creates an ImageButton with the default Assistant styles.
  static views::ImageButton* Create(
      views::ButtonListener* listener,
      const gfx::VectorIcon& icon,
      int size_in_dip,
      int icon_size_in_dip,
      int accessible_name_id,
      AssistantButtonId button_id,
      base::Optional<int> tooltip_id = base::nullopt,
      SkColor icon_color = gfx::kGoogleGrey700);

  // views::Button:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  views::ButtonListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(AssistantButton);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_
