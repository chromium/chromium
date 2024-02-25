// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_TAB_SLIDER_H_
#define ASH_STYLE_TAB_SLIDER_H_

#include <cstddef>
#include <utility>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/table_layout_view.h"

namespace ash {

class TabSliderButton;

// A tab slider has multiple slider buttons placed horizontally in a button
// container. At any time, only one of the buttons can be selected. Selecting a
// button will deselect all the other buttons. A selector painted with a fully
// rounded rectangle shows behind the selected button. When another button is
// selected, the selector will move from the position of previously selected
// button to the position of currently selected button.
class ASH_EXPORT TabSlider : public views::TableLayoutView {
  METADATA_HEADER(TabSlider, views::TableLayoutView)

 public:
  // The init parameters used to initialize the layout, appearance, and behavior
  // of the tab slider.
  struct InitParams {
    int internal_border_padding;
    int between_buttons_spacing;
    // Indicates whether there is a fully rounded rect background for the tab
    // slider.
    bool has_background;
    // Indicates whether an animation should be shown when the selector moves
    // between buttons.
    bool has_selector_animation;
    // Indicates whether the extra space should be distributed evenly between
    // buttons.
    bool distribute_space_evenly;
  };

  static constexpr InitParams kDefaultParams = {2, 0, true, true, true};

  // `max_tab_num` is the maximum number of tabs in the slider.
  explicit TabSlider(size_t max_tab_num,
                     const InitParams& params = kDefaultParams);
  TabSlider(const TabSlider&) = delete;
  TabSlider& operator=(const TabSlider&) = delete;
  ~TabSlider() override;

  views::View* GetSelectorView();
  TabSliderButton* GetButtonAtIndex(size_t index);

  // Add a button with the button's unique pointer. For example
  // AddButton(std::make_unique<SliderButtonType>(...)).
  template <typename T>
  T* AddButton(std::unique_ptr<T> button) {
    T* button_ptr = button.get();
    AddButtonInternal(button.release());
    return button_ptr;
  }

  // Add a button with the button's ctor arguments. For example
  // AddButton<SliderButtonType>(arg1, arg2, ...).
  template <typename T, typename... Args>
  T* AddButton(Args&&... args) {
    auto button = std::make_unique<T>(std::forward<Args>(args)...);
    return AddButton(std::move(button));
  }

  // Called when a button is selected.
  void OnButtonSelected(TabSliderButton* button);

  // views::View:
  void Layout(PassKey) override;

 private:
  // The view of the selector.
  class SelectorView;

  // Initialize the layout according to the total number of tabs and init
  // parameters.
  void Init();

  // Adds the button as a child of the button container and inserts it into the
  // 'buttons_' list.
  void AddButtonInternal(TabSliderButton* button);

  // Called when the enabled state is changed.
  void OnEnabledStateChanged();

  const size_t max_tab_num_;
  const InitParams params_;

  // Owned by view hierarchy.
  raw_ptr<SelectorView> selector_view_;
  std::vector<raw_ptr<TabSliderButton, VectorExperimental>> buttons_;

  base::CallbackListSubscription enabled_changed_subscription_;
};

}  // namespace ash

#endif  // ASH_STYLE_TAB_SLIDER_H_
