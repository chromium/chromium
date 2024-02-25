// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/style/icon_button.h"
#include "ash/system/unified/quick_settings_slider.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Slider;
class View;
}  // namespace views

namespace ash {
class UnifiedSliderView;

class UnifiedSliderListener : public views::SliderListener {
 public:
  ~UnifiedSliderListener() override = default;

  // Instantiates `UnifiedSliderView`.
  virtual std::unique_ptr<UnifiedSliderView> CreateView() = 0;

  // Returns the slider catalog name which is used for UMA tracking. Please
  // remember to call the corresponding tracking method (`TrackToggleUMA` and
  // `TrackValueChangeUMA`) in the `SliderButtonPressed` and
  // `SliderValueChanged` implementation.
  virtual QsSliderCatalogName GetCatalogName() = 0;

  // Tracks the toggling behavior, usually happens in `SliderButtonPressed`.If
  // the feature has no `target_toggle_state` state, pass `true` to this method.
  void TrackToggleUMA(bool target_toggle_state);

  // Tracks slider value change behavior, usually happens in
  // `SliderValueChanged`.
  void TrackValueChangeUMA(bool going_up);
};

// Base view class of a slider row in `UnifiedSystemTray`. The slider has an
// `ImageView` icon on top of the slider.
class UnifiedSliderView : public views::View {
  METADATA_HEADER(UnifiedSliderView, views::View)

 public:
  // `is_togglable` determines whether `slider_button_` is togglable or not.
  // If `read_only` is set, the slider will not accept any user events.
  // `slider_style` is `kDefaultSliderStyle` by default. `kRadioSliderStyle`
  // should be set explicitly.
  UnifiedSliderView(views::Button::PressedCallback callback,
                    UnifiedSliderListener* listener,
                    const gfx::VectorIcon& icon,
                    int accessible_name_id,
                    bool is_togglable = true,
                    bool read_only = false,
                    QuickSettingsSlider::Style slider_style =
                        QuickSettingsSlider::Style::kDefault);

  UnifiedSliderView(const UnifiedSliderView&) = delete;
  UnifiedSliderView& operator=(const UnifiedSliderView&) = delete;

  ~UnifiedSliderView() override;

  IconButton* button() { return button_; }
  views::Slider* slider() { return slider_; }
  IconButton* slider_button() { return slider_button_; }

  // Sets a slider value. If `by_user` is false, accessibility events will not
  // be triggered.
  void SetSliderValue(float value, bool by_user);

  // views::View:
  void OnEvent(ui::Event* event) override;

 private:
  raw_ptr<const gfx::VectorIcon> icon_;
  const bool is_togglable_;

  // Unowned. Owned by views hierarchy.
  raw_ptr<IconButton> button_ = nullptr;
  raw_ptr<views::Slider> slider_ = nullptr;
  raw_ptr<IconButton> slider_button_ = nullptr;
  raw_ptr<views::View> container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
