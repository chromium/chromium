// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_EXAMPLE_H_
#define CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_EXAMPLE_H_

#include "chrome/browser/enterprise/watermark/watermark_view.h"
#include "ui/views/controls/slider.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/views_examples_export.h"

namespace views {
class BoxLayoutView;
class Checkbox;
class Label;
}  // namespace views

struct SliderInfo {
  std::string label;
  double initial_value;
};

class VIEWS_EXAMPLES_EXPORT WatermarkExample
    : public views::examples::ExampleBase,
      public views::SliderListener,
      public views::TextfieldController {
 public:
  WatermarkExample();
  WatermarkExample(const WatermarkExample&) = delete;
  WatermarkExample& operator=(const WatermarkExample&) = delete;
  ~WatermarkExample() override;

  void UpdateWatermarkViewBackground();

  // ExampleBase:
  void CreateExampleView(views::View* container) override;

  // SliderListener
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

 private:
  // Add slider group to the app's controls, return pointer to the
  // group's container. Store references to the sliders and labels so
  // that they can be updated by SliderValueChanged()
  std::unique_ptr<views::BoxLayoutView> AddSliderGroup(
      const std::string& name,
      const std::vector<SliderInfo>& slider_info,
      std::vector<views::Slider*>& sliders,
      std::vector<views::Label*>& labels);

  raw_ptr<enterprise_watermark::WatermarkView> watermark_view_;
  std::vector<views::Slider*> rotation_sliders_;
  std::vector<views::Label*> rotation_slider_labels_;
  std::vector<views::Slider*> translate_sliders_;
  std::vector<views::Label*> translate_slider_labels_;
  raw_ptr<views::Checkbox> background_checkbox_;
};

class WatermarkTextArea : public views::Textarea {
  METADATA_HEADER(WatermarkTextArea, views::Textarea)
 public:
  explicit WatermarkTextArea(enterprise_watermark::WatermarkView* view);

  // views::Textfield
  void OnTextChanged() override;

 private:
  raw_ptr<enterprise_watermark::WatermarkView> watermark_view_;
};

#endif  // CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_EXAMPLE_H_
