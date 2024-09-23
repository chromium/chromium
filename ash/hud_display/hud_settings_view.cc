// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_settings_view.h"

#include <set>
#include <string>

#include "ash/hud_display/ash_tracing_handler.h"
#include "ash/hud_display/hud_display.h"
#include "ash/hud_display/hud_properties.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace hud_display {
namespace {

constexpr SkColor kHUDDisabledButtonColor =
    SkColorSetA(kHUDDefaultColor, 0xFF * 0.5);

// Thickness of border around settings.
constexpr int kHUDSettingsBorderWidth = 1;

ui::ScopedAnimationDurationScaleMode* scoped_animation_duration_scale_mode =
    nullptr;

}  // anonymous namespace

class HUDCheckboxHandler {
 public:
  HUDCheckboxHandler(
      views::Checkbox* checkbox,
      base::RepeatingCallback<void(views::Checkbox*)> update_state)
      : checkbox_(checkbox), update_state_(update_state) {}

  HUDCheckboxHandler(const HUDCheckboxHandler&) = delete;
  HUDCheckboxHandler& operator=(const HUDCheckboxHandler&) = delete;

  void UpdateState() const { update_state_.Run(checkbox_.get()); }

 private:
  const raw_ptr<views::Checkbox> checkbox_;  // not owned.
  base::RepeatingCallback<void(views::Checkbox*)> update_state_;
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
        for (aura::Window* window : root_windows) {
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
        for (aura::Window* window : root_windows) {
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
  METADATA_HEADER(SettingsCheckbox, views::Checkbox)

 public:
  SettingsCheckbox(const std::u16string& label, const std::u16string& tooltip)
      : views::Checkbox(label, views::Button::PressedCallback()) {
    SetTooltipText(tooltip);
  }
  SettingsCheckbox(const SettingsCheckbox& other) = delete;
  SettingsCheckbox operator=(const SettingsCheckbox& other) = delete;

  ~SettingsCheckbox() override = default;

  // views::Checkbox:
  SkColor GetIconImageColor(int icon_state) const override {
    return kHUDDefaultColor;
  }
};

BEGIN_METADATA(SettingsCheckbox)
END_METADATA

class AnimationSpeedSlider : public views::Slider {
  METADATA_HEADER(AnimationSpeedSlider, views::Slider)

 public:
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

BEGIN_METADATA(AnimationSpeedSlider)
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
  METADATA_HEADER(AnimationSpeedControl, views::View)

 public:
  AnimationSpeedControl();
  AnimationSpeedControl(const AnimationSpeedControl&) = delete;
  AnimationSpeedControl& operator=(const AnimationSpeedControl&) = delete;

  ~AnimationSpeedControl() override;

  // views::SliderListener:
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

  // views::View:
  void Layout(PassKey) override;

 private:
  // Map slider values to animation scale.
  using SliderValuesMap = base::flat_map<float, float>;

  raw_ptr<views::View> hints_container_ = nullptr;  // not owned.
  raw_ptr<AnimationSpeedSlider> slider_ = nullptr;  // not owned.

  SliderValuesMap slider_values_;
};

BEGIN_METADATA(AnimationSpeedControl)
END_METADATA

AnimationSpeedControl::AnimationSpeedControl() {
  // This view consists of the title, slider values hints and a slider.
  // Values hints live in a separate container.
  // Slider is under that container and is resized to match the hints.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  views::Label* title = AddChildView(std::make_unique<views::Label>(
      u"Animation speed:", views::style::CONTEXT_LABEL));
  title->SetAutoColorReadabilityEnabled(false);
  title->SetEnabledColor(kHUDDefaultColor);

  hints_container_ = AddChildView(std::make_unique<views::View>());
  hints_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  std::vector<float> multipliers;

  auto add_speed_point = [](AnimationSpeedControl* self, views::View* container,
                            std::vector<float>& multipliers, float multiplier,
                            const std::u16string& text) {
    constexpr int kLabelBorderWidth = 3;
    views::Label* label = container->AddChildView(
        std::make_unique<views::Label>(text, views::style::CONTEXT_LABEL));
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(kHUDDefaultColor);
    label->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::VH(0, kLabelBorderWidth)));
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    multipliers.push_back(multiplier);
  };

  add_speed_point(this, hints_container_, multipliers, 0, u"0");
  add_speed_point(this, hints_container_, multipliers, 0.5, u"0.5");
  add_speed_point(this, hints_container_, multipliers, 1, u"1");
  add_speed_point(this, hints_container_, multipliers, 2, u"2");
  add_speed_point(this, hints_container_, multipliers, 4, u"4");
  add_speed_point(this, hints_container_, multipliers, 10, u"10");

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

  // Because the slider is focusable, it needs to have an accessible name so
  // that the screen reader knows what to announce. Indicating the slider is
  // labelled by the title will cause ViewAccessibility to set the name.
  slider_->GetViewAccessibility().SetName(*title);
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
  delete scoped_animation_duration_scale_mode;
  scoped_animation_duration_scale_mode = nullptr;
  if (multiplier != 1) {
    scoped_animation_duration_scale_mode =
        new ui::ScopedAnimationDurationScaleMode(multiplier);
  }
}

void AnimationSpeedControl::Layout(PassKey) {
  gfx::Size max_size;
  // Make all labels equal size.
  for (const views::View* label : hints_container_->children()) {
    max_size.SetToMax(
        label->GetPreferredSize(views::SizeBounds(label->width(), {})));
  }

  for (views::View* label : hints_container_->children()) {
    label->SetPreferredSize(max_size);
  }

  gfx::Size hints_total_size = hints_container_->GetPreferredSize();
  // Slider should begin in the middle of the first label, and end in the
  // middle of the last label. But ripple overlays border, so we set total
  // width to match the total hints width and adjust border to make slider
  // correct size.
  gfx::Size slider_size(hints_total_size.width(), 30);
  slider_->SetPreferredSize(slider_size);
  slider_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(0, max_size.width() / 2)));
  LayoutSuperclass<views::View>(this);
}

class HUDActionButton : public views::LabelButton {
  METADATA_HEADER(HUDActionButton, views::LabelButton)

 public:
  HUDActionButton(views::Button::PressedCallback::Callback callback,
                  const std::u16string& text)
      : LabelButton(callback, text) {
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetEnabledTextColors(kHUDBackground);
    SetProperty(kHUDClickHandler, HTCLIENT);
    constexpr float kActionButtonCournerRadius = 2;
    SetBackground(views::CreateRoundedRectBackground(
        kHUDDefaultColor, kActionButtonCournerRadius));
    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
    on_enabled_changed_subscription_ =
        AddEnabledChangedCallback(base::BindRepeating(
            &HUDActionButton::OnEnabedChanged, base::Unretained(this)));
  }

  HUDActionButton(const HUDActionButton&) = delete;
  HUDActionButton& operator=(const HUDActionButton&) = delete;

  ~HUDActionButton() override = default;

  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::LabelButton::PaintButtonContents(canvas);
    if (spinner_refresh_timer_.IsRunning()) {
      base::Time now = base::Time::Now();
      gfx::Rect spinner = GetContentsBounds();
      int spinner_width = std::min(spinner.width(), spinner.height());
      spinner.ClampToCenteredSize(gfx::Size(spinner_width, spinner_width));
      gfx::PaintThrobberSpinning(canvas, spinner,
                                 SkColorSetA(SK_ColorWHITE, 0xFF * (.5)),
                                 (now - spinner_created_) / 8);
    }
  }

  void DisableWithSpinner() {
    DCHECK(!spinner_refresh_timer_.IsRunning());
    SetEnabled(false);
    constexpr base::TimeDelta interval = base::Seconds(0.5);
    spinner_created_ = base::Time::Now();
    spinner_refresh_timer_.Start(
        FROM_HERE, interval,
        base::BindRepeating(
            [](views::View* button) { button->SchedulePaint(); },
            base::Unretained(this)));
    SchedulePaint();
  }

  void UpdateBackgroundColor() override {
    if (GetVisualState() == STATE_DISABLED) {
      GetBackground()->SetNativeControlColor(kHUDDisabledButtonColor);
    } else {
      GetBackground()->SetNativeControlColor(kHUDDefaultColor);
    }
  }

 private:
  void OnEnabedChanged() {
    if (GetEnabled())
      spinner_refresh_timer_.Stop();
  }

  base::CallbackListSubscription on_enabled_changed_subscription_;
  base::Time spinner_created_;
  base::RepeatingTimer spinner_refresh_timer_;
};

BEGIN_METADATA(HUDActionButton)
END_METADATA

}  // anonymous namespace

BEGIN_METADATA(HUDSettingsView)
END_METADATA

HUDSettingsView::HUDSettingsView(HUDDisplayView* hud_display) {
  SetVisible(false);

  // We want AnimationSpeedControl to be stretched horizontally so we turn
  // stretch on by default.
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  SetBorder(
      views::CreateSolidBorder(kHUDSettingsBorderWidth, kHUDDefaultColor));

  // We want the HUD to be draggable when clicked on the whitespace, so we do
  // not want the buttons to extend past the minimum size. To overcome the
  // default horizontal stretch we put them into a separate container with
  // default left alignment.
  views::View* checkbox_container =
      AddChildView(std::make_unique<views::View>());
  checkbox_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);

  auto add_checkbox =
      [](HUDSettingsView* self, views::View* container,
         const std::u16string& text, const std::u16string& tooltip,
         base::RepeatingCallback<void(views::Checkbox*)> callback) {
        views::Checkbox* checkbox = container->AddChildView(
            std::make_unique<SettingsCheckbox>(text, tooltip));
        checkbox->SetCallback(
            base::BindRepeating(std::move(callback), checkbox));
        checkbox->SetEnabledTextColors(kHUDDefaultColor);
        checkbox->SetProperty(kHUDClickHandler, HTCLIENT);
        return checkbox;
      };

  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(
          this, checkbox_container, u"Tint composited content",
          u"Equivalent to --tint-composited-content command-line option.",
          GetVisDebugHandleClickCallback(
              &viz::DebugRendererSettings::tint_composited_content)),
      GetVisDebugUpdateStateCallback(
          &viz::DebugRendererSettings::tint_composited_content)));
  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(
          this, checkbox_container, u"Show overdraw feedback",
          u"Equivalent to --show-overdraw-feedback command-line option.",
          GetVisDebugHandleClickCallback(
              &viz::DebugRendererSettings::show_overdraw_feedback)),
      GetVisDebugUpdateStateCallback(
          &viz::DebugRendererSettings::show_overdraw_feedback)));
  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(
          this, checkbox_container, u"Show aggregated damage",
          u"Equivalent to --show-aggregated-damage command-line option.",
          GetVisDebugHandleClickCallback(
              &viz::DebugRendererSettings::show_aggregated_damage)),
      GetVisDebugUpdateStateCallback(
          &viz::DebugRendererSettings::show_aggregated_damage)));
  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(this, checkbox_container, u"Show paint rect.",
                   u"Equivalent to --ui-show-paint-rects command-line option.",
                   GetCCDebugHandleClickCallback(
                       &cc::LayerTreeDebugState::show_paint_rects)),
      GetCCDebugUpdateStateCallback(
          &cc::LayerTreeDebugState::show_paint_rects)));
  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(this, checkbox_container, u"HUD is overlay.",
                   u"Flips HUD overlay mode flag.",
                   base::BindRepeating(
                       [](HUDDisplayView* hud_display, views::Checkbox*) {
                         hud_display->ToggleOverlay();
                       },
                       base::Unretained(hud_display))),
      base::BindRepeating(
          [](HUDDisplayView* hud_display, views::Checkbox* checkbox) {
            checkbox->SetChecked(hud_display->IsOverlay());
          },
          base::Unretained(hud_display))));
  AddChildView(std::make_unique<AnimationSpeedControl>());

  // Ui Dev Tools controls.
  constexpr int kUiDevToolsControlButtonMargin = 6;
  views::View* ui_devtools_controls =
      AddChildView(std::make_unique<views::View>());
  ui_devtools_controls
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);

  // Show cursor position.
  constexpr int kCursorPositionDisplayButtonMargin = 6;
  views::View* cursor_position_display =
      AddChildView(std::make_unique<views::View>());
  cursor_position_display
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);

  // Tracing controls.
  constexpr int kTracingControlButtonMargin = 6;
  views::View* tracing_controls = AddChildView(std::make_unique<views::View>());
  tracing_controls
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);

  ui_devtools_controls->SetBorder(
      views::CreateEmptyBorder(kUiDevToolsControlButtonMargin));
  ui_dev_tools_control_button_ =
      ui_devtools_controls->AddChildView(std::make_unique<HUDActionButton>(
          base::BindRepeating(&HUDSettingsView::OnEnableUiDevToolsButtonPressed,
                              base::Unretained(this)),
          std::u16string()));
  UpdateDevToolsControlButtonLabel();

  cursor_position_display->SetBorder(
      views::CreateEmptyBorder(kCursorPositionDisplayButtonMargin));
  cursor_position_display_button_ =
      cursor_position_display->AddChildView(std::make_unique<HUDActionButton>(
          base::BindRepeating(
              &HUDSettingsView::OnEnableCursorPositionDisplayButtonPressed,
              base::Unretained(this)),
          u"Show cursor position"));

  tracing_controls->SetBorder(
      views::CreateEmptyBorder(kTracingControlButtonMargin));
  tracing_control_button_ =
      tracing_controls->AddChildView(std::make_unique<HUDActionButton>(
          base::BindRepeating(&HUDSettingsView::OnEnableTracingButtonPressed,
                              base::Unretained(this)),
          std::u16string()));

  const int kLabelBorderWidth = 3;
  tracing_status_message_ =
      tracing_controls->AddChildView(std::make_unique<views::Label>(
          std::u16string(), views::style::CONTEXT_LABEL));
  tracing_status_message_->SetAutoColorReadabilityEnabled(false);
  tracing_status_message_->SetEnabledColor(kHUDDefaultColor);
  tracing_status_message_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(0, kLabelBorderWidth)));
  tracing_status_message_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::Label* pii_label = tracing_controls->AddChildView(std::make_unique<
                                                           views::Label>(
      u"WARNING: Trace files may contain Personally Identifiable Information. "
      u"You should use discretion when sharing your trace files.",
      views::style::CONTEXT_LABEL));
  pii_label->SetMultiLine(true);
  pii_label->SetAutoColorReadabilityEnabled(false);
  pii_label->SetEnabledColor(kHUDDefaultColor);
  pii_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(0, kLabelBorderWidth)));
  pii_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  UpdateTracingControlButton();

  AshTracingManager::Get().AddObserver(this);
  aura::Env* env = aura::Env::GetInstance();
  env->AddEventObserver(this, env,
                        std::set<ui::EventType>({ui::EventType::kMouseDragged,
                                                 ui::EventType::kMouseMoved}));
}

HUDSettingsView::~HUDSettingsView() {
  AshTracingManager::Get().RemoveObserver(this);
  aura::Env::GetInstance()->RemoveEventObserver(this);
}

void HUDSettingsView::OnTracingStatusChange() {
  UpdateTracingControlButton();
}

void HUDSettingsView::OnEnableUiDevToolsButtonPressed(const ui::Event& event) {
  if (Shell::Get()->shell_delegate()->IsUiDevToolsStarted()) {
    Shell::Get()->shell_delegate()->StopUiDevTools();
  } else {
    Shell::Get()->shell_delegate()->StartUiDevTools();
  }
  UpdateDevToolsControlButtonLabel();
}

void HUDSettingsView::UpdateDevToolsControlButtonLabel() {
  if (!Shell::Get()->shell_delegate()->IsUiDevToolsStarted()) {
    ui_dev_tools_control_button_->SetText(u"Create Ui Dev Tools");
  } else {
    const int port = Shell::Get()->shell_delegate()->GetUiDevToolsPort();
    ui_dev_tools_control_button_->SetText(base::ASCIIToUTF16(
        base::StringPrintf("Ui Dev Tools: ON, port %d", port).c_str()));
  }
}

void HUDSettingsView::OnEnableCursorPositionDisplayButtonPressed(
    const ui::Event& event) {
  showing_cursor_position_ = !showing_cursor_position_;
  if (showing_cursor_position_) {
    cursor_position_display_button_->SetText(base::ASCIIToUTF16(base::StrCat(
        {"Cursor: ",
         aura::Env::GetInstance()->last_mouse_location().ToString()})));
  } else {
    cursor_position_display_button_->SetText(u"Show cursor position");
  }
}

void HUDSettingsView::OnEvent(ui::Event* event) {
  views::View::OnEvent(event);
}

void HUDSettingsView::OnEvent(const ui::Event& event) {
  if (!showing_cursor_position_ || !event.IsMouseEvent()) {
    return;
  }

  cursor_position_display_button_->SetText(base::ASCIIToUTF16(
      base::StrCat({"Cursor: ", event.AsMouseEvent()->location().ToString()})));
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

void HUDSettingsView::OnEnableTracingButtonPressed(const ui::Event& event) {
  ToggleTracing();
}

void HUDSettingsView::ToggleTracingForTesting() {
  ToggleTracing();
}

void HUDSettingsView::ToggleTracing() {
  AshTracingManager& manager = AshTracingManager::Get();
  tracing_control_button_->DisableWithSpinner();
  if (manager.IsTracingStarted()) {
    manager.Stop();
  } else {
    manager.Start();
  }
}

void HUDSettingsView::UpdateTracingControlButton() {
  AshTracingManager& manager = AshTracingManager::Get();
  if (!manager.IsBusy())
    tracing_control_button_->SetEnabled(true);

  tracing_status_message_->SetText(
      base::ASCIIToUTF16(manager.GetStatusMessage()));
  if (manager.IsTracingStarted()) {
    tracing_control_button_->SetText(u"Stop tracing.");
  } else {
    tracing_control_button_->SetText(u"Start tracing.");
  }
}

}  // namespace hud_display

}  // namespace ash
