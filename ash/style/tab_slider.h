// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_TAB_SLIDER_H_
#define ASH_STYLE_TAB_SLIDER_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class TabSliderButton;

// A tab slider has multiple slider buttons placed horizontally in a button
// container. At any time, only one of the buttons can be selected. Selecting a
// button will deselect all the other buttons. A selector painted with a fully
// rounded rectangle shows behind the selected button. When another button is
// selected, the selector will move from the position of previously selected
// button to the position of currently selected button.
class ASH_EXPORT TabSlider : public views::View {
 public:
  METADATA_HEADER(TabSlider);

  // The layout parameters used to configure the layout of the button
  // container.
  struct LayoutParams {
    int internal_border_padding = 0;
    int between_buttons_spacing = 0;
  };

  // `has_background` indicates whether there is a fully rounded rect
  // background for the tab slider.
  // `has_selector_animation` indicates whether an animation should be shown
  // when the selector moves between buttons.
  // `distribute_space_evenly` indicates whether the extra space should be
  // distributed evenly between buttons.
  explicit TabSlider(bool has_background = true,
                     bool has_selector_animation = true,
                     bool distribute_space_evenly = true);
  TabSlider(const TabSlider&) = delete;
  TabSlider& operator=(const TabSlider&) = delete;
  ~TabSlider() override;

  // Add a button with the button's unique pointer. For example
  // AddButton(std::make_unique<SliderButtonType>(...)).
  template <typename T>
  T* AddButton(std::unique_ptr<T> button) {
    T* raw_ptr = button.get();
    AddButtonInternal(button.release());
    return raw_ptr;
  }

  // Add a button with the button's ctor arguments. For example
  // AddButton<SliderButtonType>(arg1, arg2, ...).
  template <typename T, typename... Args>
  T* AddButton(Args... args) {
    auto button = std::make_unique<T>(args...);
    return AddButton(std::move(button));
  }

  // Sets custom button container layout. When the custom layout is set,
  // the button container will no longer use the button recommended layout.
  void SetCustomLayout(const LayoutParams& layout_params);

  // Called when a button is selected.
  void OnButtonSelected(TabSliderButton* button);

  // views::View:
  void Layout() override;

 private:
  // The view of the selector.
  class SelectorView;

  // Adds the button as a child of the button container and inserts it into the
  // 'buttons_' list.
  void AddButtonInternal(TabSliderButton* button);

  // Called when a button is added to the slider.
  void OnButtonAdded(TabSliderButton* button);

  // Updates the LayoutManager based on how many views exist,
  // `distribute_space_evenly_`, and `custom_layout_params_`.
  void UpdateLayout();

  // Called when the enabled state is changed.
  void OnEnabledStateChanged();

  // Owned by view hierarchy.
  SelectorView* selector_view_;
  std::vector<TabSliderButton*> buttons_;

  // Parameters for a custom layout. Set by either individual buttons, or
  // through `SetCustomLayout()`.
  LayoutParams custom_layout_params_;

  // Whether child buttons should be forced to evenly share space.
  const bool distribute_space_evenly_;

  // By default, respect buttons recommended layout.
  bool use_button_recommended_layout_ = true;

  base::CallbackListSubscription enabled_changed_subscription_;
};

}  // namespace ash

#endif  // ASH_STYLE_TAB_SLIDER_H_
