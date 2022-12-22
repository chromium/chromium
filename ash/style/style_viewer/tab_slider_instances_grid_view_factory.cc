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
constexpr size_t kGridViewRowNum = 6;
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
  auto icon_slider_two = std::make_unique<TabSlider>();
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
  auto icon_slider_three = std::make_unique<TabSlider>(
      /*has_background_=*/false, /*has_slider_background_animation=*/false);
  icon_slider_three->SetCustomLayout(
      {/*button_container_spacing=*/0, /*between_buttons_spacing=*/16});
  // Add the buttons with ctor arguments of `IconSliderButton`.
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
  auto icon_slider_two_disabled = std::make_unique<TabSlider>();
  auto* disabled_image_button =
      icon_slider_two_disabled->AddButton<IconSliderButton>(
          IconSliderButton::PressedCallback(), &kCaptureModeImageIcon,
          u"image mode");
  disabled_image_button->SetSelected(true);
  icon_slider_two_disabled->AddButton<IconSliderButton>(
      IconSliderButton::PressedCallback(), &kCaptureModeCameraIcon,
      u"video mode");
  icon_slider_two_disabled->SetEnabled(false);

  // Create an instance of label slider with two buttons.
  auto label_slider_two = std::make_unique<TabSlider>();
  // Add the buttons with `TabSliderButton` unique pointer.
  auto* label_button =
      label_slider_two->AddButton(std::make_unique<LabelSliderButton>(
          LabelSliderButton::PressedCallback(), u"Label 1", u"label 1"));
  label_button->SetSelected(true);
  label_slider_two->AddButton(std::make_unique<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"Label 2", u"label 2"));

  // Create an instance of label slider with three buttons with customized
  // layout.
  auto label_slider_three = std::make_unique<TabSlider>();
  label_slider_three->SetCustomLayout(
      {/*button_container_spacing=*/2, /*between_buttons_spacing=*/16});
  // Add the buttons with ctor arguments of `TabSliderButton`.
  auto* label_button_1 = label_slider_three->AddButton<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"Label 1", u"label 1");
  label_button_1->SetSelected(true);
  label_slider_three->AddButton<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"Label 2", u"label 2");
  label_slider_three->AddButton<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"Label 3", u"label 3");

  // Create an instance of disabled label slider with two buttons.
  auto label_slider_two_disabled = std::make_unique<TabSlider>();
  auto* disabled_label_button =
      label_slider_two_disabled->AddButton<LabelSliderButton>(
          LabelSliderButton::PressedCallback(), u"Label 1", u"label 1");
  disabled_label_button->SetSelected(true);
  label_slider_two_disabled->AddButton<LabelSliderButton>(
      LabelSliderButton::PressedCallback(), u"Label 2", u"label 2");
  label_slider_two_disabled->SetEnabled(false);

  grid_view->AddInstance(
      u"Icon tab slider with 2 buttons, recommended layout, selector "
      u"animation, and background",
      std::move(icon_slider_two));
  grid_view->AddInstance(
      u"Icon tab slider with 3 buttons, custom layout, no selector "
      u"animation, and no background",
      std::move(icon_slider_three));
  grid_view->AddInstance(u"Disabled icon slider with 2 buttons",
                         std::move(icon_slider_two_disabled));

  grid_view->AddInstance(
      u"Label tab slider with 2 buttons, recommended layout, selector "
      u"animation, and background",
      std::move(label_slider_two));
  grid_view->AddInstance(
      u"Label tab slider with 3 buttons, recommended layout, selector "
      u"animation, and background",
      std::move(label_slider_three));
  grid_view->AddInstance(u"Disabled label slider with 2 buttons",
                         std::move(label_slider_two_disabled));

  return grid_view;
}

}  // namespace ash
