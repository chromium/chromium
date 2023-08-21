// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"

namespace ash {

namespace {

// Configurations of grid view for `TabSlider` instances.
constexpr size_t kGridViewRowNum = 9;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 3;
constexpr size_t kGirdViewColGroupSize = 1;

}  // namespace

std::unique_ptr<SystemUIComponentsGridView> CreateTabSliderInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);

  // Create an instance of icon slider with two buttons, a slider background,
  // the recommended layout, and a selector animation.
  auto icon_slider_two = std::make_unique<TabSlider>(/*max_tab_num=*/2);
  // Add the buttons with `IconSliderButton` unique pointer.
  auto* image_button =
      icon_slider_two->AddButton(std::make_unique<IconSliderButton>(
          IconSliderButton::PressedCallback(), &kCaptureModeImageIcon,
          u"image mode"));
  image_button->SetSelected(true);
  icon_slider_two->AddButton(std::make_unique<IconSliderButton>(
      IconSliderButton::PressedCallback(), &kCaptureModeCameraIcon,
      u"video mode"));

  // Create an instance of icon slider with three buttons, no background, a
  // customized layout, and no slider animation.
  TabSlider::InitParams params = TabSlider::kDefaultParams;
  params.internal_border_padding = 0;
  params.between_buttons_spacing = 16;
  params.has_background = false;
  params.has_selector_animation = false;
  auto icon_slider_three = std::make_unique<TabSlider>(
      /*max_tab_num=*/3, params);
  auto* fullscreen_button = icon_slider_three->AddButton<IconSliderButton>(
      IconSliderButton::PressedCallback(), &kCaptureModeFullscreenIcon,
      u"fullscreen mode");
  fullscreen_button->SetSelected(true);
  icon_slider_three->AddButton<IconSliderButton>(
      IconSliderButton::PressedCallback(), &kCaptureModeRegionIcon,
      u"region mode");
  icon_slider_three->AddButton<IconSliderButton>(
      IconSliderButton::PressedCallback(), &kCaptureModeWindowIcon,
      u"window mode");

  // Create an instance of disabled icon slider with two buttons.
  auto icon_slider_two_disabled =
      std::make_unique<TabSlider>(/*max_tab_num=*/2);
  auto* disabled_image_button =
      icon_slider_two_disabled->AddButton<IconSliderButton>(
          IconSliderButton::PressedCallback(), &kCaptureModeImageIcon,
          u"image mode");
  disabled_image_button->SetSelected(true);
  icon_slider_two_disabled->AddButton<IconSliderButton>(
      IconSliderButton::PressedCallback(), &kCaptureModeCameraIcon,
      u"video mode");
  icon_slider_two_disabled->SetEnabled(false);

  // Create an instance of label slider with two buttons and unevenly
  // distributed space.
  params = TabSlider::kDefaultParams;
  params.distribute_space_evenly = false;
  auto label_slider_two_unevenly = std::make_unique<TabSlider>(
      /*max_tab_num=*/2, params);
  // Add the buttons with `TabSliderButton` unique pointer.
  auto* label_button =
      label_slider_two_unevenly->AddButton(std::make_unique<LabelSliderButton>(
          LabelSliderButton::PressedCallback(), u"one", u"label 1"));
  label_button->SetSelected(true);
  label_slider_two_unevenly->AddButton(std::make_unique<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"one two three", u"label 2"));

  // Create an instance of label slider with two buttons and evenly distributed
  // space.
  auto label_slider_two = std::make_unique<TabSlider>(/*max_tab_num=*/2);
  auto* label_button_two_1 = label_slider_two->AddButton<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"one", u"label 1");
  label_button_two_1->SetSelected(true);
  label_slider_two->AddButton<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"one two three", u"label 2");

  // Create an instance of label slider with three buttons and evenly
  // distributed space.
  auto label_slider_three = std::make_unique<TabSlider>(/*max_tab_num=*/3);
  auto* label_button_three_1 = label_slider_three->AddButton<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"one", u"label 1");
  label_button_three_1->SetSelected(true);
  label_slider_three->AddButton<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"one two three", u"label 2");
  label_slider_three->AddButton<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"one two three four five",
      u"label 3");

  // Create an instance of icon + label slider with two buttons and unevenly
  // distributed space.
  params = IconLabelSliderButton::kSliderParams;
  params.distribute_space_evenly = false;
  auto icon_label_slider_two_unevenly = std::make_unique<TabSlider>(
      /*max_tab_num=*/2, params);
  auto* icon_label_button =
      icon_label_slider_two_unevenly->AddButton<IconLabelSliderButton>(
          IconLabelSliderButton::PressedCallback(),
          &kPrivacyIndicatorsCameraIcon, u"one", u"button 1");
  icon_label_button->SetSelected(true);
  icon_label_slider_two_unevenly->AddButton<IconLabelSliderButton>(
      IconLabelSliderButton::PressedCallback(),
      &kPrivacyIndicatorsMicrophoneIcon, u"one two three", u"button 2");

  // Create an instance of icon + label slider with three buttons and evenly
  // distributed space.
  auto icon_label_slider_three = std::make_unique<TabSlider>(
      /*max_tab_num=*/3, IconLabelSliderButton::kSliderParams);
  auto* icon_label_button_1 =
      icon_label_slider_three->AddButton<IconLabelSliderButton>(
          IconLabelSliderButton::PressedCallback(),
          &kPrivacyIndicatorsMicrophoneIcon, u"one", u"button 1");
  icon_label_button_1->SetSelected(true);
  icon_label_slider_three->AddButton<IconLabelSliderButton>(
      IconLabelSliderButton::PressedCallback(),
      &kPrivacyIndicatorsMicrophoneIcon, u"one two three", u"button 2");
  icon_label_slider_three->AddButton<IconLabelSliderButton>(
      IconLabelSliderButton::PressedCallback(),
      &kPrivacyIndicatorsMicrophoneIcon, u"ont two three four five",
      u"button 3");

  // Create an instance of disabled icon + label slider with two buttons.
  auto icon_label_slider_two_disabled =
      std::make_unique<TabSlider>(/*max_tab_num=*/2);
  auto* icon_label_button_disabled =
      icon_label_slider_two_disabled->AddButton<IconLabelSliderButton>(
          IconLabelSliderButton::PressedCallback(),
          &kPrivacyIndicatorsCameraIcon, u"one", u"button 1");
  icon_label_button_disabled->SetSelected(true);
  icon_label_slider_two_disabled->AddButton<IconLabelSliderButton>(
      IconLabelSliderButton::PressedCallback(),
      &kPrivacyIndicatorsMicrophoneIcon, u"one two three", u"button 2");
  icon_label_slider_two_disabled->SetEnabled(false);

  grid_view->AddInstance(
      u"Icon tab slider with 2 buttons, recommended layout, selector \n"
      u"animation, and background",
      std::move(icon_slider_two));
  grid_view->AddInstance(
      u"Icon tab slider with 3 buttons, custom layout, no selector \n"
      u"animation, and no background",
      std::move(icon_slider_three));
  grid_view->AddInstance(u"Disabled icon slider with 2 buttons",
                         std::move(icon_slider_two_disabled));

  grid_view->AddInstance(
      u"Label tab slider with 2 buttons, recommended layout, selector \n"
      u"animation, background, and unevenly distributed spaces",
      std::move(label_slider_two_unevenly));
  grid_view->AddInstance(
      u"Label tab slider with 2 buttons, recommended layout, selector \n"
      u"animation, background, and evenly distributed space",
      std::move(label_slider_two));
  grid_view->AddInstance(
      u"Label tab slider with 3 buttons, recommended layout, selector \n"
      u"animation, background, and evenly distributed space",
      std::move(label_slider_three));

  grid_view->AddInstance(
      u"Icon + label tab slider with 2 buttons, recommended layout, selector \n"
      u"animation, background, and unevenly distributed space",
      std::move(icon_label_slider_two_unevenly));
  grid_view->AddInstance(
      u"Icon + label tab slider with 3 buttons, recommended layout, selector \n"
      u"animation, background, and evenly distributed space",
      std::move(icon_label_slider_three));
  grid_view->AddInstance(u"Disabled icon + label slider with 2 buttons",
                         std::move(icon_label_slider_two_disabled));

  return grid_view;
}

}  // namespace ash
