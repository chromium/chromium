// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_TAB_SLIDER_BUTTON_H_
#define ASH_STYLE_TAB_SLIDER_BUTTON_H_

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
 public:
  METADATA_HEADER(TabSliderButton);

  explicit TabSliderButton(PressedCallback callback);
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

  // Returns the recommended layout parameters for tab slider. Note that the
  // recommended layout parameters are only used as a minimum spacing reference.
  // The slider will adjust the layout based on the the current and recommended
  // layout spacings.
  virtual absl::optional<TabSlider::LayoutParams> GetRecommendedSliderLayout()
      const;

 private:
  // Called when the button selected state is changed.
  virtual void OnSelectedChanged() = 0;

  // views::Button:
  void NotifyClick(const ui::Event& event) override;

  // Not owned by button.
  raw_ptr<TabSlider, ExperimentalAsh> tab_slider_ = nullptr;
  // The selected state indicating if the button is selected.
  bool selected_ = false;
};

// An extension of `TabSliderButton` which is a circle button with an icon in
// the center. The icon has different color schemes for selected, unselected,
// and disabled states.
class ASH_EXPORT IconSliderButton : public TabSliderButton {
 public:
  METADATA_HEADER(IconSliderButton);

  IconSliderButton(PressedCallback callback,
                   const gfx::VectorIcon* icon,
                   const std::u16string& tooltip_text = u"");
  IconSliderButton(const IconSliderButton&) = delete;
  IconSliderButton& operator=(const IconSliderButton&) = delete;
  ~IconSliderButton() override;

  // TabSliderButton:
  absl::optional<TabSlider::LayoutParams> GetRecommendedSliderLayout()
      const override;

 private:
  // TabSliderButton:
  void OnSelectedChanged() override;

  // views::Button:
  void OnThemeChanged() override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  const raw_ptr<const gfx::VectorIcon, ExperimentalAsh> icon_;
};

// An extension of `TabSliderButton` which is rounded rect button with a label
// in the center. The label text has different color schemes for selected,
// unselected, and disabled states.
class ASH_EXPORT LabelSliderButton : public TabSliderButton {
 public:
  METADATA_HEADER(LabelSliderButton);

  LabelSliderButton(PressedCallback callback,
                    const std::u16string& text,
                    const std::u16string& tooltip_text = u"");
  LabelSliderButton(const LabelSliderButton&) = delete;
  LabelSliderButton& operator=(const LabelSliderButton&) = delete;
  ~LabelSliderButton() override;

  // TabSliderButton:
  absl::optional<TabSlider::LayoutParams> GetRecommendedSliderLayout()
      const override;

 private:
  // Update label color according to the current button state.
  void UpdateLabelColor();

  // TabSliderButton:
  void OnSelectedChanged() override;

  // views::Button:
  int GetHeightForWidth(int w) const override;
  gfx::Size CalculatePreferredSize() const override;
  void StateChanged(ButtonState old_state) override;

  // Owned by the view hierarchy.
  raw_ptr<views::Label, ExperimentalAsh> label_;
};

// A `TabSliderButton` which shows an icon above a label.
class ASH_EXPORT IconLabelSliderButton : public TabSliderButton {
 public:
  METADATA_HEADER(IconLabelSliderButton);

  IconLabelSliderButton(PressedCallback callback,
                        const gfx::VectorIcon* icon,
                        const std::u16string& text,
                        const std::u16string& tooltip_text = u"");
  IconLabelSliderButton(const IconLabelSliderButton&) = delete;
  IconLabelSliderButton& operator=(const IconLabelSliderButton&) = delete;
  ~IconLabelSliderButton() override;

  // TabSliderButton:
  absl::optional<TabSlider::LayoutParams> GetRecommendedSliderLayout()
      const override;

 private:
  // Update label color according to the current button state.
  void UpdateColors();

  // TabSliderButton:
  void OnSelectedChanged() override;

  // Owned by the views hierarchy.
  const raw_ptr<views::ImageView, ExperimentalAsh> image_view_;
  const raw_ptr<views::Label, ExperimentalAsh> label_;
};

}  // namespace ash

#endif  // ASH_STYLE_TAB_SLIDER_BUTTON_H_
