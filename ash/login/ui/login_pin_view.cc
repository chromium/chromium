// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_pin_view.h"

#include <memory>

#include "ash/login/ui/login_button.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace {

// Values for the ink drop.
constexpr int kInkDropCornerRadiusDp = 24;

constexpr const char* kPinLabels[] = {
    "+",      // 0
    "",       // 1
    " ABC",   // 2
    " DEF",   // 3
    " GHI",   // 4
    " JKL",   // 5
    " MNO",   // 6
    " PQRS",  // 7
    " TUV",   // 8
    " WXYZ",  // 9
};

constexpr const char kLoginPinViewClassName[] = "LoginPinView";

// How long does the user have to long-press the backspace button before it
// auto-submits?
constexpr int kInitialBackspaceDelayMs = 500;
// After the first auto-submit, how long until the next backspace event fires?
constexpr int kRepeatingBackspaceDelayMs = 150;

// Button sizes.
constexpr int kButtonHeightDp = 56;
constexpr int kButtonWidthDp = 72;
constexpr gfx::Size kButtonSize = gfx::Size(kButtonWidthDp, kButtonHeightDp);

base::string16 GetButtonLabelForNumber(int value) {
  DCHECK(value >= 0 && value < int{base::size(kPinLabels)});
  return base::ASCIIToUTF16(std::to_string(value));
}

base::string16 GetButtonSubLabelForNumber(int value) {
  DCHECK(value >= 0 && value < int{base::size(kPinLabels)});
  return base::ASCIIToUTF16(kPinLabels[value]);
}

// Returns the view id for the given pin number.
int GetViewIdForPinNumber(int number) {
  // 0 is a valid pin number but it is also the default view id. Shift all ids
  // over so 0 can be found.
  return number + 1;
}

// A base class for pin button in the pin keyboard.
class BasePinButton : public views::InkDropHostView {
 public:
  BasePinButton(const LoginPalette& palette,
                const gfx::Size& size,
                const base::string16& accessible_name,
                const base::RepeatingClosure& on_press)
      : on_press_(on_press),
        palette_(palette),
        accessible_name_(accessible_name) {
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    SetPreferredSize(size);

    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    SetLayoutManager(std::move(layout));

    // Layer rendering is needed for animation. Enable it here for
    // focus painter to paint.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);

    views::FocusRing* focus_ring = views::FocusRing::Install(this);
    login_views_utils::ConfigureRectFocusRingCircleInkDrop(
        this, focus_ring, kInkDropCornerRadiusDp);
  }

  ~BasePinButton() override = default;

  // views::InkDropHostView:
  void OnPaint(gfx::Canvas* canvas) override {
    InkDropHostView::OnPaint(canvas);
  }
  void OnFocus() override {
    InkDropHostView::OnFocus();
    SchedulePaint();
  }
  void OnBlur() override {
    InkDropHostView::OnBlur();
    SchedulePaint();
  }
  void OnEvent(ui::Event* event) override {
    bool is_key_press = event->type() == ui::ET_KEY_PRESSED &&
                        (event->AsKeyEvent()->code() == ui::DomCode::ENTER ||
                         event->AsKeyEvent()->code() == ui::DomCode::SPACE);
    bool is_mouse_press = event->type() == ui::ET_MOUSE_PRESSED;
    bool is_gesture_tap = event->type() == ui::ET_GESTURE_TAP_DOWN;

    if (is_key_press || is_mouse_press || is_gesture_tap) {
      DispatchPress(event);
      return;
    }

    views::InkDropHostView::OnEvent(event);
  }
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(accessible_name_);
    node_data->role = ax::mojom::Role::kButton;
  }
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    gfx::Point center = GetLocalBounds().CenterPoint();
    gfx::Rect bounds(center.x() - kInkDropCornerRadiusDp,
                     center.y() - kInkDropCornerRadiusDp,
                     kInkDropCornerRadiusDp * 2, kInkDropCornerRadiusDp * 2);

    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), GetLocalBounds().InsetsFrom(bounds),
        GetInkDropCenterBasedOnLastEvent(), palette_.pin_ink_drop_ripple_color,
        /*visible_opacity=*/1.f);
  }
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    auto highlight = std::make_unique<views::InkDropHighlight>(
        gfx::SizeF(size()), palette_.pin_ink_drop_highlight_color);
    highlight->set_visible_opacity(1.f);
    return highlight;
  }

 protected:
  // Called when the button has been pressed.
  virtual void DispatchPress(ui::Event* event) {
    if (event)
      event->SetHandled();

    AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED,
                   ui::LocatedEvent::FromIfValid(event));
    SchedulePaint();

    // |on_press_| may delete us.
    if (on_press_)
      on_press_.Run();
  }

  // Handler for press events. May be null.
  base::RepeatingClosure on_press_;

  LoginPalette palette_;

 private:
  const base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(BasePinButton);
};

// A PIN button that displays a digit number and corresponding letter mapping.
class DigitPinButton : public BasePinButton {
 public:
  DigitPinButton(int value,
                 bool show_sub_label,
                 const LoginPalette& palette,
                 const gfx::Size& size,
                 const LoginPinView::OnPinKey& on_key)
      : BasePinButton(palette,
                      size,
                      GetButtonLabelForNumber(value),
                      base::BindRepeating(on_key, value)) {
    SetID(GetViewIdForPinNumber(value));
    const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
    views::Label* label = new views::Label(GetButtonLabelForNumber(value),
                                           views::style::CONTEXT_BUTTON,
                                           views::style::STYLE_PRIMARY);
    label->SetEnabledColor(palette.button_enabled_color);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);
    label->SetFontList(base_font_list.Derive(8 /*size_delta*/, gfx::Font::FontStyle::NORMAL,
                                             gfx::Font::Weight::NORMAL));
    AddChildView(label);

    if (show_sub_label) {
      views::Label* sub_label = new views::Label(
          GetButtonSubLabelForNumber(value), views::style::CONTEXT_BUTTON,
          views::style::STYLE_SECONDARY);
      sub_label->SetEnabledColor(palette.button_annotation_color);
      sub_label->SetAutoColorReadabilityEnabled(false);
      sub_label->SetSubpixelRenderingEnabled(false);
      sub_label->SetFontList(base_font_list.Derive(-1 /*size_delta*/, gfx::Font::FontStyle::NORMAL,
                                                   gfx::Font::Weight::NORMAL));
      AddChildView(sub_label);
    }
  }

  ~DigitPinButton() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DigitPinButton);
};

}  // namespace

// A PIN button that displays backspace icon.
class LoginPinView::BackspacePinButton : public BasePinButton {
 public:
  BackspacePinButton(const LoginPalette& palette,
                     const gfx::Size& size,
                     const base::RepeatingClosure& on_press)
      : BasePinButton(palette,
                      size,
                      l10n_util::GetStringUTF16(
                          IDS_ASH_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME),
                      on_press),
        palette_(palette) {
    image_ = new views::ImageView();
    image_->SetImage(gfx::CreateVectorIcon(kLockScreenBackspaceIcon,
                                           palette_.button_enabled_color));
    AddChildView(image_);
    SetEnabled(false);
  }

  ~BackspacePinButton() override = default;

  void SetTimersForTesting(std::unique_ptr<base::OneShotTimer> delay_timer,
                           std::unique_ptr<base::RepeatingTimer> repeat_timer) {
    delay_timer_ = std::move(delay_timer);
    repeat_timer_ = std::move(repeat_timer);
  }

  void OnEnabledChanged() {
    SkColor color = palette_.button_enabled_color;
    if (!GetEnabled()) {
      AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
      color = SkColorSetA(color, login_constants::kButtonDisabledAlpha);
      CancelRepeat();
    }

    image_->SetImage(gfx::CreateVectorIcon(kLockScreenBackspaceIcon, color));
  }

  // BasePinButton:
  void OnEvent(ui::Event* event) override {
    BasePinButton::OnEvent(event);
    if (event->handled())
      return;
    // If this is a button release style event cancel any repeat.
    if (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
        event->type() == ui::ET_GESTURE_END ||
        event->type() == ui::ET_MOUSE_RELEASED) {
      CancelRepeat();
    }
  }
  void DispatchPress(ui::Event* event) override {
    // Key events have their own repeat logic that is managed by the system.
    if (event && event->IsKeyEvent()) {
      BasePinButton::DispatchPress(event);
      return;
    }

    // If this is the first time the button has been pressed, do not fire a
    // submit event immediately. Instead, trigger the delay timer. The
    // cancellation logic handles the edge case of a button just being tapped.
    if (!is_held_) {
      is_held_ = true;
      DCHECK(!delay_timer_->IsRunning());
      DCHECK(!repeat_timer_->IsRunning());
      delay_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kInitialBackspaceDelayMs),
          base::BindOnce(&BackspacePinButton::DispatchPress,
                         base::Unretained(this), nullptr));

      if (event)
        event->SetHandled();

      AnimateInkDrop(views::InkDropState::ACTIVATED,
                     ui::LocatedEvent::FromIfValid(event));
      SchedulePaint();

      return;
    }

    // If here, then this function was fired by the delay_timer_. We need to
    // make sure the repeat_timer_ is running so the function will fire again.
    if (!repeat_timer_->IsRunning()) {
      repeat_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kRepeatingBackspaceDelayMs),
          base::BindRepeating(&BackspacePinButton::DispatchPress,
                              base::Unretained(this), nullptr));
    }

    // Run handler.
    if (on_press_)
      on_press_.Run();
  }

 private:
  // Cancels a long-press. If the press event has not been triggered yet this
  // will trigger it.
  void CancelRepeat() {
    if (!is_held_)
      return;

    bool did_submit = !delay_timer_->IsRunning();
    delay_timer_->Stop();
    repeat_timer_->Stop();
    is_held_ = false;

    if (!did_submit && on_press_)
      on_press_.Run();

    AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
    SchedulePaint();
  }

  bool is_held_ = false;
  std::unique_ptr<base::OneShotTimer> delay_timer_ =
      std::make_unique<base::OneShotTimer>();
  std::unique_ptr<base::RepeatingTimer> repeat_timer_ =
      std::make_unique<base::RepeatingTimer>();

  views::ImageView* image_ = nullptr;
  views::PropertyChangedSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(base::BindRepeating(
          &LoginPinView::BackspacePinButton::OnEnabledChanged,
          base::Unretained(this)));

  LoginPalette palette_;

  DISALLOW_COPY_AND_ASSIGN(BackspacePinButton);
};

// A PIN button to press to submit the PIN / password.
class LoginPinView::SubmitPinButton : public BasePinButton {
 public:
  SubmitPinButton(const LoginPalette& palette,
                  const gfx::Size& size,
                  const base::RepeatingClosure& on_press)
      : BasePinButton(palette,
                      size,
                      l10n_util::GetStringUTF16(
                          IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME),
                      on_press),
        image_(new views::ImageView()),
        palette_(palette) {
    image_->SetImage(gfx::CreateVectorIcon(kLockScreenArrowIcon,
                                           palette_.button_enabled_color));
    AddChildView(image_);
    SetEnabled(false);
  }

  SubmitPinButton(const SubmitPinButton&) = delete;
  SubmitPinButton& operator=(const SubmitPinButton&) = delete;
  ~SubmitPinButton() override = default;

  void OnEnabledChanged() {
    SkColor color = palette_.button_enabled_color;
    if (!GetEnabled())
      color = SkColorSetA(color, login_constants::kButtonDisabledAlpha);

    image_->SetImage(gfx::CreateVectorIcon(kLockScreenArrowIcon, color));
  }

 private:
  views::ImageView* image_ = nullptr;
  views::PropertyChangedSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&LoginPinView::SubmitPinButton::OnEnabledChanged,
                              base::Unretained(this)));

  LoginPalette palette_;
};

// static
gfx::Size LoginPinView::TestApi::GetButtonSize(Style style) {
  return gfx::Size(kButtonWidthDp, kButtonHeightDp);
}

LoginPinView::TestApi::TestApi(LoginPinView* view) : view_(view) {}

LoginPinView::TestApi::~TestApi() = default;

views::View* LoginPinView::TestApi::GetButton(int number) const {
  return view_->GetViewByID(GetViewIdForPinNumber(number));
}

views::View* LoginPinView::TestApi::GetBackspaceButton() const {
  return view_->backspace_;
}

views::View* LoginPinView::TestApi::GetSubmitButton() const {
  return view_->submit_button_;
}

void LoginPinView::TestApi::SetBackspaceTimers(
    std::unique_ptr<base::OneShotTimer> delay_timer,
    std::unique_ptr<base::RepeatingTimer> repeat_timer) {
  view_->backspace_->SetTimersForTesting(std::move(delay_timer),
                                         std::move(repeat_timer));
}

void LoginPinView::TestApi::ClickOnDigit(int number) const {
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  GetButton(number)->OnEvent(&event);
}

LoginPinView::LoginPinView(Style keyboard_style,
                           const LoginPalette& palette,
                           const OnPinKey& on_key,
                           const OnPinBackspace& on_backspace,
                           const OnPinSubmit& on_submit)
    : NonAccessibleView(kLoginPinViewClassName), palette_(palette) {
  DCHECK(on_key);
  DCHECK(on_backspace);

  // Layer rendering.
  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  bool show_letters = keyboard_style == Style::kAlphanumeric;

  auto add_digit_button = [&](View* row, int value) {
    row->AddChildView(
        new DigitPinButton(value, show_letters, palette_, kButtonSize, on_key));
  };

  // 1-2-3
  auto* row = BuildAndAddRow();
  add_digit_button(row, 1);
  add_digit_button(row, 2);
  add_digit_button(row, 3);

  // 4-5-6
  row = BuildAndAddRow();
  add_digit_button(row, 4);
  add_digit_button(row, 5);
  add_digit_button(row, 6);

  // 7-8-9
  row = BuildAndAddRow();
  add_digit_button(row, 7);
  add_digit_button(row, 8);
  add_digit_button(row, 9);

  // backspace-0-submit
  row = BuildAndAddRow();
  backspace_ = row->AddChildView(std::make_unique<BackspacePinButton>(
      palette_, kButtonSize, on_backspace));
  add_digit_button(row, 0);

  // Only add the submit button if the callback is valid.
  if (!on_submit.is_null()) {
    submit_button_ = row->AddChildView(
        std::make_unique<SubmitPinButton>(palette_, kButtonSize, on_submit));
  }
}

LoginPinView::~LoginPinView() = default;

void LoginPinView::NotifyAccessibilityLocationChanged() {
  this->NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged,
                                 false /*send_native_event*/);
  for (NonAccessibleView* row : rows) {
    row->NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged,
                                  false /*send_native_event*/);
  }
}

void LoginPinView::OnPasswordTextChanged(bool is_empty) {
  backspace_->SetEnabled(!is_empty);
  if (submit_button_)
    submit_button_->SetEnabled(!is_empty);
}

NonAccessibleView* LoginPinView::BuildAndAddRow() {
  auto* row = new NonAccessibleView();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  AddChildView(row);
  rows.push_back(row);
  return row;
}

}  // namespace ash
