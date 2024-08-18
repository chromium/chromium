// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_SLIDER_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_SLIDER_H_

#include "ash/ash_export.h"
#include "ui/views/controls/slider.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace ui {
class Event;
}  // namespace ui

namespace ash {

// This slider view is used in quick settings in the status area. It will be
// used in the `QuickSettingsView` and `TrayBubbleView`. This slider view
// supports different styles. `kDefault` slider is used in `QuickSettingsView`
// and in `TrayBubbleView`. `kRadioActive` slider will be used for the active
// input/output device in `AudioDetailedView`. `kRadioInactive` slider will be
// used for the inactive device in `AudioDetailedView`.
class ASH_EXPORT QuickSettingsSlider : public views::Slider {
  METADATA_HEADER(QuickSettingsSlider, views::Slider)

 public:
  // Represents the style of the slider.
  enum class Style {
    // Represents the slider where the full part is a rounded corner rectangle
    // with a height of `kFullSliderThickness`, and the empty part is a rounded
    // corner rectangle with a height of `kEmptySliderThickness`. These two
    // parts are center-aligned horizontally. The ends of both parts have fully
    // rounded corners.
    kDefault,
    // Same style as `kDefault`, except for the thumb and trough are in gray for
    // the muted default sliders.
    kDefaultMuted,
    // Represents the style where both the full part and the empty part of the
    // slider have a height of `kFullSliderThickness`. The ends are fully
    // rounded.
    kRadioActive,
    // Same style as `kRadioActive`, except for the thumb and trough are in
    // gray for the muted radio sliders. Only the active radio sliders will have
    // the muted state.
    kRadioActiveMuted,
    // Represents the style where the full part and the empty part also have the
    // same height of `kFullSliderThickness`, except that the ends are not fully
    // rounded but have a radius of `kInactiveRadioSliderRoundedRadius`.
    kRadioInactive
  };

  QuickSettingsSlider(views::SliderListener* listener, Style slider_style);
  QuickSettingsSlider(const QuickSettingsSlider&) = delete;
  QuickSettingsSlider& operator=(const QuickSettingsSlider&) = delete;
  ~QuickSettingsSlider() override;

  // Setter and Getter of the slider style. Schedules paint after setting the
  // style since styles and colors may change for the radio sliders because of
  // the active status change. If the slider is the `kRadioInactive`, also
  // disables the focus behavior for it.
  void SetSliderStyle(Style style);
  Style slider_style() const { return slider_style_; }

  // Gets the bounds and rounded corner radius for `kRadioInactive` to draw the
  // focus ring around it in `AudioDetailedView`.
  gfx::Rect GetInactiveRadioSliderRect();
  int GetInactiveRadioSliderRoundedCornerRadius();

  void set_is_toggleable_volume_slider(bool is_toggleable_volume_slider) {
    is_toggleable_volume_slider_ = is_toggleable_volume_slider;
  }
  bool is_toggleable_volume_slider() { return is_toggleable_volume_slider_; }

 private:
  // views::Slider:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  SkColor GetThumbColor() const override;
  SkColor GetTroughColor() const override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // The style of the slider.
  Style slider_style_;

  // Indicates if the slider can be toggled to mute/unmute volume. Used for
  // additional accessibility warnings to make sure a user cannot accidentally
  // turn off volume.
  bool is_toggleable_volume_slider_ = false;
};

// A slider that ignores inputs. This will be used in the
// `UnifiedKeyboardBrightnessView` and `UnifiedKeyboardBacklightToggleView`.
class ASH_EXPORT ReadOnlySlider : public QuickSettingsSlider {
  METADATA_HEADER(ReadOnlySlider, QuickSettingsSlider)

 public:
  explicit ReadOnlySlider(Style slider_style);
  ReadOnlySlider(const ReadOnlySlider&) = delete;
  ReadOnlySlider& operator=(const ReadOnlySlider&) = delete;
  ~ReadOnlySlider() override;

 private:
  // views::View:
  bool CanAcceptEvent(const ui::Event& event) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_SLIDER_H_
