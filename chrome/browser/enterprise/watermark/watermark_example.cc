// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_example.h"

#include <memory>

#include "cc/paint/paint_canvas.h"
#include "chrome/browser/enterprise/watermark/watermark_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager.h"

namespace {

class GradientView : public views::View {
 public:
  METADATA_HEADER(GradientView, views::View)

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    SkColor left = SkColorSetARGB(0xff, 0xff, 0xff, 0xff);
    SkColor right = SkColorSetARGB(0xff, 0, 0, 0);
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(width(), 0), gfx::Point(0, height()), left, right));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRect(GetLocalBounds(), flags);
  }
};

class CustomLayoutManager : views::LayoutManager {
  // views::LayoutManager
  void Layout(views::View* host) override {}

 private:
  raw_ptr<views::View> contents_view_;
  raw_ptr<views::View> watermark_view_;
};

BEGIN_METADATA(GradientView)
END_METADATA

}  // namespace

WatermarkExample::WatermarkExample()
    : ExampleBase("Watermark"),
      rotation_sliders_(4),
      rotation_slider_labels_(4),
      translate_sliders_(2),
      translate_slider_labels_(2) {}

void WatermarkExample::CreateExampleView(views::View* container) {
  auto* box_layout =
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // top view
  auto* watermark_container =
      container->AddChildView(std::make_unique<views::View>());
  watermark_container->SetUseDefaultFillLayout(true);
  watermark_container->AddChildView(std::make_unique<GradientView>());
  watermark_container->SetPaintToLayer();
  watermark_view_ = watermark_container->AddChildView(
      std::make_unique<enterprise_watermark::WatermarkView>(
          "Private! Confidential"));
  box_layout->SetFlexForView(watermark_container, 13);

  // Background checkbox and text
  auto* checkbox_container =
      container->AddChildView(std::make_unique<views::BoxLayoutView>());
  background_checkbox_ =
      checkbox_container->AddChildView(std::make_unique<views::Checkbox>(
          u"Show Background",
          base::BindRepeating(&WatermarkExample::UpdateWatermarkViewBackground,
                              base::Unretained(this))));

  auto* watermark_textarea = checkbox_container->AddChildView(
      std::make_unique<WatermarkTextArea>(watermark_view_));
  watermark_textarea->SetController(this);
  watermark_textarea->GetViewAccessibility().SetName(u"WM textarea");
  watermark_textarea->SetDefaultWidthInChars(100);

  box_layout->SetFlexForView(checkbox_container, 1);

  // Rotation Axis
  std::vector<SliderInfo> slider_infos = {
      {.label = "x", .initial_value = 0.0},
      {.label = "y", .initial_value = 0.0},
      {.label = "z", .initial_value = 0.0},
      {.label = "angle", .initial_value = 0.0}};

  auto slider_container = AddSliderGroup(
      "Rotation", slider_infos, rotation_sliders_, rotation_slider_labels_);
  auto* ptr = container->AddChildView(std::move(slider_container));
  box_layout->SetFlexForView(ptr, 1);

  // Translate sliders
  std::vector<SliderInfo> translate_slider_infos = {
      {.label = "x", .initial_value = 0.5},
      {.label = "y", .initial_value = 0.5}};

  slider_container =
      AddSliderGroup("Translation", translate_slider_infos, translate_sliders_,
                     translate_slider_labels_);
  ptr = container->AddChildView(std::move(slider_container));
  box_layout->SetFlexForView(ptr, 1);

  // had to call Slider::SetValue() after, because the SliderValueChanged()
  // callback references all sliders, so I needed all of them to be constructed
  // before calling SetValue()
  for (int i = 0; i < 4; ++i) {
    rotation_sliders_[i]->SetValue(slider_infos[i].initial_value);
  }

  for (int i = 0; i < 2; ++i) {
    translate_sliders_[i]->SetValue(slider_infos[i].initial_value);
  }

  watermark_view_->SetSize({2000, 2000});
}

std::unique_ptr<views::BoxLayoutView> WatermarkExample::AddSliderGroup(
    const std::string& name,
    const std::vector<SliderInfo>& slider_infos,
    std::vector<views::Slider*>& sliders,
    std::vector<views::Label*>& labels) {
  assert(slider_infos.size() == sliders.size());
  assert(slider_infos.size() == labels.size());

  auto slider_container = std::make_unique<views::BoxLayoutView>();
  slider_container->SetLayoutManager(std::make_unique<views::BoxLayout>());
  slider_container->AddChildView(
      std::make_unique<views::Label>(base::ASCIIToUTF16(name)));
  for (size_t i = 0; i < slider_infos.size(); ++i) {
    auto slider_view = std::make_unique<views::BoxLayoutView>();
    slider_view->SetLayoutManager(std::make_unique<views::BoxLayout>());
    slider_view->AddChildView(std::make_unique<views::Label>(base::ASCIIToUTF16(
        base::StringPrintf("%s: ", slider_infos[i].label.c_str()))));
    labels[i] = slider_view->AddChildView(
        std::make_unique<views::Label>(base::ASCIIToUTF16(
            base::StringPrintf("%.3lf ", slider_infos[i].initial_value))));
    auto* slider =
        slider_view->AddChildView(std::make_unique<views::Slider>(this));
    // Setting view accessibility is required by a DCHECK, I just set it
    // randomly for now
    slider->GetViewAccessibility().SetName("Slider",
                                           ax::mojom::NameFrom::kAttribute);
    sliders[i] = slider;
    slider_container->AddChildView(std::move(slider_view));
  }
  return slider_container;
}

void WatermarkExample::SliderValueChanged(views::Slider* sender,
                                          float value,
                                          float old_value,
                                          views::SliderChangeReason reason) {
  // Set the view's rotation
  gfx::Transform transform;
  transform.Translate(translate_sliders_[0]->GetValue() * 500 - 250,
                      translate_sliders_[1]->GetValue() * 500 - 250);

  transform.RotateAbout(rotation_sliders_[0]->GetValue(),
                        rotation_sliders_[1]->GetValue(),
                        rotation_sliders_[2]->GetValue(),
                        rotation_sliders_[3]->GetValue() * 360.0);
  watermark_view_->SetTransform(transform);

  // update rotation labels
  for (int i = 0; i < 3; ++i) {
    rotation_slider_labels_[i]->SetText(base::ASCIIToUTF16(
        base::StringPrintf("%.3lf ", rotation_sliders_[i]->GetValue())));
  }

  rotation_slider_labels_[3]->SetText(base::ASCIIToUTF16(
      base::StringPrintf("%.3lf ", rotation_sliders_[3]->GetValue() * 360.0)));

  // update translation labels
  for (int i = 0; i < 2; ++i) {
    translate_slider_labels_[i]->SetText(base::ASCIIToUTF16(base::StringPrintf(
        "%.3lf ", translate_sliders_[i]->GetValue() * 500 - 250)));
  }
}

void WatermarkExample::UpdateWatermarkViewBackground() {
  if (background_checkbox_->GetChecked()) {
    watermark_view_->SetBackgroundColor(SkColorSetARGB(0xc, 0xff, 0, 0));
  } else {
    watermark_view_->SetBackgroundColor(SkColorSetARGB(0, 0, 0, 0));
  }
}

WatermarkExample::~WatermarkExample() = default;

// WatermarkTextArea
WatermarkTextArea::WatermarkTextArea(enterprise_watermark::WatermarkView* view)
    : watermark_view_(view) {}

void WatermarkTextArea::OnTextChanged() {
  Textfield::OnTextChanged();
  watermark_view_->SetString(base::UTF16ToUTF8(GetText()));
}

BEGIN_METADATA(WatermarkTextArea)
END_METADATA
