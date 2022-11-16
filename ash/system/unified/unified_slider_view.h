// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/style/icon_button.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Label;
}  // namespace views

namespace ash {

class UnifiedSliderListener : public views::SliderListener {
 public:
  ~UnifiedSliderListener() override = default;

  // Instantiates UnifiedSliderView. The view will be onwed by views hierarchy.
  // The view should be always deleted after the controller is destructed.
  virtual views::View* CreateView() = 0;

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

// Base view class of a slider row in UnifiedSystemTray. It has a button on the
// left side and a slider on the right side.
class UnifiedSliderView : public views::View {
 public:
  // If |readonly| is set, the slider will not accept any user events.
  UnifiedSliderView(views::Button::PressedCallback callback,
                    UnifiedSliderListener* listener,
                    const gfx::VectorIcon& icon,
                    int accessible_name_id,
                    bool readonly = false);

  UnifiedSliderView(const UnifiedSliderView&) = delete;
  UnifiedSliderView& operator=(const UnifiedSliderView&) = delete;

  ~UnifiedSliderView() override;

  IconButton* button() { return button_; }
  views::Slider* slider() { return slider_; }
  views::Label* toast_label() { return toast_label_; }

  // Sets a slider value. If |by_user| is false, accessibility events will not
  // be triggered.
  void SetSliderValue(float value, bool by_user);

  // views::View:
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 protected:
  void CreateToastLabel();

 private:
  // Unowned. Owned by views hierarchy.
  IconButton* const button_;
  views::Slider* const slider_;
  views::Label* toast_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
