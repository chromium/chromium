// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_settings_view.h"

#include "ash/hud_display/hud_properties.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace hud_display {

class HUDCheckboxHandler {
 public:
  HUDCheckboxHandler(
      views::Checkbox* checkbox,
      base::RepeatingCallback<void(views::Checkbox*)> update_state,
      base::RepeatingCallback<void(views::Checkbox*)> handle_click)
      : checkbox_(checkbox),
        update_state_(update_state),
        handle_click_(handle_click) {}

  HUDCheckboxHandler(const HUDCheckboxHandler&) = delete;
  HUDCheckboxHandler& operator=(const HUDCheckboxHandler&) = delete;

  void UpdateState() const { update_state_.Run(checkbox_); }
  void HandleClick() const { handle_click_.Run(checkbox_); }

  const views::Checkbox* checkbox() const { return checkbox_; }

 private:
  views::Checkbox* const checkbox_;  // not owned.
  base::RepeatingCallback<void(views::Checkbox*)> update_state_;
  base::RepeatingCallback<void(views::Checkbox*)> handle_click_;
};

namespace {

base::RepeatingCallback<void(views::Checkbox*)> GetVisDebugUpdateStateCallback(
    const bool viz::DebugRendererSettings::*field) {
  return base::BindRepeating(
      [](const bool viz::DebugRendererSettings::*field,
         views::Checkbox* checkbox) {
        checkbox->SetChecked(aura::Env::GetInstance()
                                 ->context_factory()
                                 ->GetHostFrameSinkManager()
                                 ->debug_renderer_settings().*
                             field);
      },
      field);
}

base::RepeatingCallback<void(views::Checkbox*)> GetVisDebugHandleClickCallback(
    bool viz::DebugRendererSettings::*field) {
  return base::BindRepeating(
      [](bool viz::DebugRendererSettings::*field, views::Checkbox* checkbox) {
        viz::HostFrameSinkManager* manager = aura::Env::GetInstance()
                                                 ->context_factory()
                                                 ->GetHostFrameSinkManager();
        viz::DebugRendererSettings debug_settings =
            manager->debug_renderer_settings();
        debug_settings.*field = checkbox->GetChecked();
        manager->UpdateDebugRendererSettings(debug_settings);
      },
      field);
}

base::RepeatingCallback<void(views::Checkbox*)> GetCCDebugUpdateStateCallback(
    const bool cc::LayerTreeDebugState::*field) {
  return base::BindRepeating(
      [](const bool cc::LayerTreeDebugState::*field,
         views::Checkbox* checkbox) {
        bool is_enabled = false;
        aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
        for (auto* window : root_windows) {
          ui::Compositor* compositor = window->GetHost()->compositor();
          is_enabled |= compositor->GetLayerTreeDebugState().*field;
        }
        checkbox->SetChecked(is_enabled);
      },
      field);
}

base::RepeatingCallback<void(views::Checkbox*)> GetCCDebugHandleClickCallback(
    bool cc::LayerTreeDebugState::*field) {
  return base::BindRepeating(
      [](bool cc::LayerTreeDebugState::*field, views::Checkbox* checkbox) {
        aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
        for (auto* window : root_windows) {
          ui::Compositor* compositor = window->GetHost()->compositor();
          cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
          state.*field = checkbox->GetChecked();
          compositor->SetLayerTreeDebugState(state);
        }
      },
      field);
}

// views::Checkbox that ignores theme colors.
class SettingsCheckbox : public views::Checkbox {
 public:
  METADATA_HEADER(SettingsCheckbox);

  SettingsCheckbox(const base::string16& label, views::ButtonListener* listener)
      : views::Checkbox(label, listener) {}
  SettingsCheckbox(const SettingsCheckbox& other) = delete;
  SettingsCheckbox operator=(const SettingsCheckbox& other) = delete;

  ~SettingsCheckbox() override = default;

  // views::Checkbox:
  SkColor GetIconImageColor(int icon_state) const override {
    return kHUDDefaultColor;
  }
};

BEGIN_METADATA(SettingsCheckbox, views::Checkbox);
END_METADATA

class AnimationSpeedSlider : public views::Slider {
 public:
  METADATA_HEADER(AnimationSpeedSlider);

  AnimationSpeedSlider(const base::flat_set<float>& values,
                       views::SliderListener* listener = nullptr)
      : views::Slider(listener) {
    SetAllowedValues(&values);
  }

  AnimationSpeedSlider(const AnimationSpeedSlider&) = delete;
  AnimationSpeedSlider operator=(const AnimationSpeedSlider&) = delete;

  ~AnimationSpeedSlider() override = default;

  // views::Slider:
  SkColor GetThumbColor() const override { return kHUDDefaultColor; }

  SkColor GetTroughColor() const override { return kHUDDefaultColor; }
  void OnPaint(gfx::Canvas* canvas) override;
};

BEGIN_METADATA(AnimationSpeedSlider, views::Slider)
END_METADATA

void AnimationSpeedSlider::OnPaint(gfx::Canvas* canvas) {
  views::Slider::OnPaint(canvas);

  // Paint ticks.
  const int kTickHeight = 8;
  const gfx::Rect content = GetContentsBounds();
  const gfx::Insets insets = GetInsets();
  const int y = insets.top() + content.height() / 2 - kTickHeight / 2;

  SkPath path;
  for (const float v : allowed_values()) {
    const float x = insets.left() + content.width() * v;
    path.moveTo(x, y);
    path.lineTo(x, y + kTickHeight);
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setColor(GetThumbColor());
  flags.setStrokeWidth(1);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  canvas->DrawPath(path, flags);
}

// Checkbox group for setting UI animation speed.
class AnimationSpeedControl : public views::SliderListener, public views::View {
 public:
  METADATA_HEADER(AnimationSpeedControl);

  AnimationSpeedControl();
  AnimationSpeedControl(const AnimationSpeedControl&) = delete;
  AnimationSpeedControl& operator=(const AnimationSpeedControl&) = delete;

  ~AnimationSpeedControl() override;

  // views::ButtonListener:
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

  // views::View:
  void Layout() override;

 private:
  // Map slider values to animation scale.
  using SliderValuesMap = base::flat_map<float, float>;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      scoped_animation_duration_scale_mode_;

  views::View* hints_container_ = nullptr;  // not owned.
  AnimationSpeedSlider* slider_ = nullptr;  // not owned.

  SliderValuesMap slider_values_;
};

BEGIN_METADATA(AnimationSpeedControl, views::View)
END_METADATA

AnimationSpeedControl::AnimationSpeedControl() {
  // This view consists of the title, slider values hints and a slider.
  // Values hints live in a separate container.
  // Slider is under that container and is resized to match the hints.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  views::Label* title = AddChildView(std::make_unique<views::Label>(
      base::ASCIIToUTF16("Animation speed:"), views::style::CONTEXT_LABEL));
  title->SetAutoColorReadabilityEnabled(false);
  title->SetEnabledColor(kHUDDefaultColor);

  hints_container_ = AddChildView(std::make_unique<views::View>());
  hints_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  std::vector<float> multipliers;

  auto add_speed_point = [](AnimationSpeedControl* self, views::View* container,
                            std::vector<float>& multipliers, float multiplier,
                            const base::string16& text) {
    const int kLabelBorderWidth = 3;
    views::Label* label = container->AddChildView(
        std::make_unique<views::Label>(text, views::style::CONTEXT_LABEL));
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(kHUDDefaultColor);
    label->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(/*vertical=*/0, /*horizontal=*/kLabelBorderWidth)));
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    multipliers.push_back(multiplier);
  };

  add_speed_point(this, hints_container_, multipliers, 0,
                  base::ASCIIToUTF16("0"));
  add_speed_point(this, hints_container_, multipliers, 0.5,
                  base::ASCIIToUTF16("0.5"));
  add_speed_point(this, hints_container_, multipliers, 1,
                  base::ASCIIToUTF16("1"));
  add_speed_point(this, hints_container_, multipliers, 2,
                  base::ASCIIToUTF16("2"));
  add_speed_point(this, hints_container_, multipliers, 4,
                  base::ASCIIToUTF16("4"));
  add_speed_point(this, hints_container_, multipliers, 10,
                  base::ASCIIToUTF16("10"));

  // Now we need to calculate discrete values for the slider and active slider
  // value.
  std::vector<float> slider_values_list;
  const float steps = multipliers.size() - 1;
  const float active_multiplier =
      ui::ScopedAnimationDurationScaleMode::duration_multiplier();
  float slider_value = -1;
  for (size_t i = 0; i < multipliers.size(); ++i) {
    const float slider_step = i / steps;
    slider_values_list.push_back(slider_step);
    slider_values_[slider_step] = multipliers[i];
    if (multipliers[i] == active_multiplier)
      slider_value = slider_step;

    // If we did not find exact value for the slider, set it to upper bound
    // or to the maximum.
    if (slider_value == -1 &&
        (i == multipliers.size() - 1 || multipliers[i] > active_multiplier))
      slider_value = slider_step;
  }

  slider_ = AddChildView(std::make_unique<AnimationSpeedSlider>(
      base::flat_set<float>(slider_values_list), this));
  slider_->SetProperty(kHUDClickHandler, HTCLIENT);
  if (slider_value != -1)
    slider_->SetValue(slider_value);
}

AnimationSpeedControl::~AnimationSpeedControl() = default;

void AnimationSpeedControl::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  SliderValuesMap::const_iterator it = slider_values_.find(value);
  DCHECK(it != slider_values_.end());
  float multiplier = it->second;
  // There could be only one instance of the scoped modifier at a time.
  // So we need to destroy the existing one before we can create a
  // new one.
  scoped_animation_duration_scale_mode_.reset();
  if (multiplier != 1) {
    scoped_animation_duration_scale_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(multiplier);
  }
}

void AnimationSpeedControl::Layout() {
  gfx::Size max_size;
  // Make all labels equal size.
  for (const auto* label : hints_container_->children())
    max_size.SetToMax(label->GetPreferredSize());

  for (auto* label : hints_container_->children())
    label->SetPreferredSize(max_size);

  gfx::Size hints_total_size = hints_container_->GetPreferredSize();
  // Slider should negin in the middle of the first label, and end in the
  // middle of the last label. But ripple overlays border, so we set total
  // width to match the total hints width and adjust border to make slider
  // correct size.
  gfx::Size slider_size(hints_total_size.width(), 30);
  slider_->SetPreferredSize(slider_size);
  slider_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(/*vertical=*/0, /*horizontal=*/max_size.width() / 2)));
  views::View::Layout();
}

}  // anonymous namespace

BEGIN_METADATA(HUDSettingsView, views::View)
END_METADATA

HUDSettingsView::HUDSettingsView() {
  SetVisible(false);

  // We want AnimationSpeedControl to be stretched horizontally so we turn
  // stretch on by default.
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  SetBorder(views::CreateSolidBorder(1, kHUDDefaultColor));

  // We want the HUD to be draggable when clicked on the whitespace, so we do
  // not want the buttons to extend past the minimum size. To overcome the
  // default horizontal stretch we put them into a separate container with
  // default left alignment.
  views::View* checkbox_contaner =
      AddChildView(std::make_unique<views::View>());
  checkbox_contaner
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);

  auto add_checkbox = [](HUDSettingsView* self, views::View* container,
                         const base::string16& text) -> views::Checkbox* {
    views::Checkbox* checkbox =
        container->AddChildView(std::make_unique<SettingsCheckbox>(text, self));
    checkbox->SetEnabledTextColors(kHUDDefaultColor);
    checkbox->SetProperty(kHUDClickHandler, HTCLIENT);
    return checkbox;
  };

  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(this, checkbox_contaner,
                   base::ASCIIToUTF16("Tint composited content")),
      GetVisDebugUpdateStateCallback(
          &viz::DebugRendererSettings::tint_composited_content),
      GetVisDebugHandleClickCallback(
          &viz::DebugRendererSettings::tint_composited_content)));
  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(this, checkbox_contaner,
                   base::ASCIIToUTF16("Show overdraw feedback")),
      GetVisDebugUpdateStateCallback(
          &viz::DebugRendererSettings::show_overdraw_feedback),
      GetVisDebugHandleClickCallback(
          &viz::DebugRendererSettings::show_overdraw_feedback)));
  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(this, checkbox_contaner,
                   base::ASCIIToUTF16("Show aggregated damage")),
      GetVisDebugUpdateStateCallback(
          &viz::DebugRendererSettings::show_aggregated_damage),
      GetVisDebugHandleClickCallback(
          &viz::DebugRendererSettings::show_aggregated_damage)));
  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(this, checkbox_contaner,
                   base::ASCIIToUTF16("Show paint rect.")),
      GetCCDebugUpdateStateCallback(&cc::LayerTreeDebugState::show_paint_rects),
      GetCCDebugHandleClickCallback(
          &cc::LayerTreeDebugState::show_paint_rects)));
  AddChildView(std::make_unique<AnimationSpeedControl>());
}

HUDSettingsView::~HUDSettingsView() = default;

void HUDSettingsView::ButtonPressed(views::Button* sender,
                                    const ui::Event& /*event*/) {
  for (const auto& handler : checkbox_handlers_) {
    if (sender != handler->checkbox())
      continue;

    handler->HandleClick();
    break;
  }
}

void HUDSettingsView::ToggleVisibility() {
  const bool is_shown = !GetVisible();
  if (is_shown) {
    for (const auto& handler : checkbox_handlers_) {
      handler->UpdateState();
    }
  }
  SetVisible(is_shown);
}

}  // namespace hud_display

}  // namespace ash
