// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_TAB_SLIDER_BUTTON_H_
#define ASH_STYLE_TAB_SLIDER_BUTTON_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/style/tab_slider.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Label;
}  // namespace views

namespace ash {

// A base class for tab slider buttons used in `TabSlider`. The button has
// selected and unselected states. When clicking the button, it will be set to
// selected. After a button is added to a tab slider, selecting the button will
// deselect the other buttons in the tab slider.
class ASH_EXPORT TabSliderButton : public views::Button {
  METADATA_HEADER(TabSliderButton, views::Button)

 public:
  TabSliderButton(PressedCallback callback, const std::u16string& tooltip_text);
  TabSliderButton(const TabSliderButton&) = delete;
  TabSliderButton& operator=(const TabSliderButton&) = delete;
  ~TabSliderButton() override;

  bool selected() const { return selected_; }

  // Called when the button is added to a tab slider.
  void AddedToSlider(TabSlider* tab_slider);

  // Changes the selected state.
  void SetSelected(bool selected);

  // Returns the recommended color id for the current button state.
  SkColor GetColorIdOnButtonState();

 private:
  // Called when the button selected state is changed.
  virtual void OnSelectedChanged() = 0;

  // views::Button:
  void NotifyClick(const ui::Event& event) override;

  // Not owned by button.
  raw_ptr<TabSlider> tab_slider_ = nullptr;
  // The selected state indicating if the button is selected.
  bool selected_ = false;
};

// An extension of `TabSliderButton` which is a circle button with an icon in
// the center. The icon has different color schemes for selected, unselected,
// and disabled states.
class ASH_EXPORT IconSliderButton : public TabSliderButton {
  METADATA_HEADER(IconSliderButton, TabSliderButton)

 public:
  IconSliderButton(PressedCallback callback,
                   const gfx::VectorIcon* icon,
                   const std::u16string& tooltip_text_base = u"");
  IconSliderButton(const IconSliderButton&) = delete;
  IconSliderButton& operator=(const IconSliderButton&) = delete;
  ~IconSliderButton() override;

 private:
  // TabSliderButton:
  void OnSelectedChanged() override;

  // views::Button:
  void OnThemeChanged() override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  const raw_ptr<const gfx::VectorIcon> icon_;
};

// An extension of `TabSliderButton` which is rounded rect button with a label
// in the center. The label text has different color schemes for selected,
// unselected, and disabled states.
class ASH_EXPORT LabelSliderButton : public TabSliderButton {
  METADATA_HEADER(LabelSliderButton, TabSliderButton)

 public:
  LabelSliderButton(PressedCallback callback,
                    const std::u16string& text,
                    const std::u16string& tooltip_text_base = u"");
  LabelSliderButton(const LabelSliderButton&) = delete;
  LabelSliderButton& operator=(const LabelSliderButton&) = delete;
  ~LabelSliderButton() override;

 private:
  // Update label color according to the current button state.
  void UpdateLabelColor();

  // TabSliderButton:
  void OnSelectedChanged() override;

  // views::Button:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void StateChanged(ButtonState old_state) override;

  // Owned by the view hierarchy.
  raw_ptr<views::Label> label_;
};

// A `TabSliderButton` which shows an icon either:
// - Next to a label (`horizontal == true`).
// - Above a label (`horizontal == false`).
class ASH_EXPORT IconLabelSliderButton : public TabSliderButton {
  METADATA_HEADER(IconLabelSliderButton, TabSliderButton)

 public:
  static constexpr TabSlider::InitParams kSliderParams{
      /*internal_border_padding=*/4,
      /*between_child_spacing=*/0,
      /*has_background=*/true,
      /*has_selector_animation=*/true,
      /*distribute_space_evenly=*/true};

  IconLabelSliderButton(PressedCallback callback,
                        const gfx::VectorIcon* icon,
                        const std::u16string& text,
                        const std::u16string& tooltip_text_base = u"",
                        bool horizontal = false);
  IconLabelSliderButton(const IconLabelSliderButton&) = delete;
  IconLabelSliderButton& operator=(const IconLabelSliderButton&) = delete;
  ~IconLabelSliderButton() override;

 private:
  // Update label color according to the current button state.
  void UpdateColors();

  // TabSliderButton:
  void OnSelectedChanged() override;

  // Owned by the views hierarchy.
  const raw_ptr<views::ImageView> image_view_;
  const raw_ptr<views::Label> label_;
};

}  // namespace ash

#endif  // ASH_STYLE_TAB_SLIDER_BUTTON_H_
