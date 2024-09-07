// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/login/ui/login_pin_view.h"

#include <memory>

#include "ash/login/ui/login_button.h"
#include "ash/login/ui/views_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
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
constexpr int kButtonHeightDp = 60;
constexpr int kButtonWidthDp = 64;
constexpr int kButtonBackgroundDiameter = 48;
constexpr gfx::Size kButtonSize = gfx::Size(kButtonWidthDp, kButtonHeightDp);

std::u16string GetButtonLabelForNumber(int value) {
  DCHECK(value >= 0 && value < int{std::size(kPinLabels)});
  return base::NumberToString16(value);
}

std::u16string GetButtonSubLabelForNumber(int value) {
  DCHECK(value >= 0 && value < int{std::size(kPinLabels)});
  return base::ASCIIToUTF16(kPinLabels[value]);
}

// Returns the view id for the given pin number.
int GetViewIdForPinNumber(int number) {
  // 0 is a valid pin number but it is also the default view id. Shift all ids
  // over so 0 can be found.
  return number + 1;
}

// A base class for pin button in the pin keyboard.
class BasePinButton : public views::View {
  METADATA_HEADER(BasePinButton, views::View)

 public:
  BasePinButton(const gfx::Size& size,
                const std::u16string& accessible_name,
                const base::RepeatingClosure& on_press)
      : on_press_(on_press) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
    GetViewAccessibility().SetName(accessible_name);
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    SetPreferredSize(size);
    SetBackground(holding_space_util::CreateCircleBackground(
        cros_tokens::kCrosSysSystemBaseElevated, kButtonBackgroundDiameter));

    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    SetLayoutManager(std::move(layout));

    // Layer rendering is needed for animation. Enable it here for
    // focus painter to paint.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    views::InkDrop::Install(this, std::make_unique<views::InkDropHost>(this));
    views::InkDropHost* const ink_drop_host = views::InkDrop::Get(this);
    ink_drop_host->SetMode(
        views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
    ink_drop_host->SetBaseColorId(kColorAshInkDrop);

    ink_drop_host->SetCreateHighlightCallback(base::BindRepeating(
        [](BasePinButton* host) {
          auto highlight = std::make_unique<views::InkDropHighlight>(
              gfx::SizeF(host->size()),
              views::InkDrop::Get(host)->GetBaseColor());
          highlight->set_visible_opacity(1.0f);
          return highlight;
        },
        this));

    ink_drop_host->SetCreateRippleCallback(base::BindRepeating(
        [](BasePinButton* host) -> std::unique_ptr<views::InkDropRipple> {
          const gfx::Point center = host->GetLocalBounds().CenterPoint();
          const gfx::Rect bounds(center.x() - kInkDropCornerRadiusDp,
                                 center.y() - kInkDropCornerRadiusDp,
                                 kInkDropCornerRadiusDp * 2,
                                 kInkDropCornerRadiusDp * 2);

          return std::make_unique<views::FloodFillInkDropRipple>(
              views::InkDrop::Get(host), host->size(),
              host->GetLocalBounds().InsetsFrom(bounds),
              views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
              views::InkDrop::Get(host)->GetBaseColor(),
              /*visible_opacity=*/1.f);
        },
        this));

    views::FocusRing::Install(this);
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
    login_views_utils::ConfigureRectFocusRingCircleInkDrop(
        this, views::FocusRing::Get(this), kInkDropCornerRadiusDp);
  }

  BasePinButton(const BasePinButton&) = delete;
  BasePinButton& operator=(const BasePinButton&) = delete;

  ~BasePinButton() override = default;

  // views::View:
  void OnFocus() override {
    View::OnFocus();
    SchedulePaint();
  }

  void OnBlur() override {
    View::OnBlur();
    SchedulePaint();
  }

  void OnEvent(ui::Event* event) override {
    bool is_key_press = event->type() == ui::EventType::kKeyPressed &&
                        (event->AsKeyEvent()->code() == ui::DomCode::ENTER ||
                         event->AsKeyEvent()->code() == ui::DomCode::SPACE);
    bool is_mouse_press = event->type() == ui::EventType::kMousePressed;
    bool is_gesture_tap = event->type() == ui::EventType::kGestureTapDown;

    if (is_key_press || is_mouse_press || is_gesture_tap) {
      DispatchPress(event);
      return;
    }

    views::View::OnEvent(event);
  }

 protected:
  // Called when the button has been pressed.
  virtual void DispatchPress(ui::Event* event) {
    if (event) {
      event->SetHandled();
    }

    views::InkDrop::Get(this)->AnimateToState(
        views::InkDropState::ACTION_TRIGGERED,
        ui::LocatedEvent::FromIfValid(event));
    SchedulePaint();

    // |on_press_| may delete us.
    if (on_press_) {
      on_press_.Run();
    }
  }

  // Handler for press events. May be null.
  base::RepeatingClosure on_press_;
};

BEGIN_METADATA(BasePinButton)
END_METADATA

}  // namespace

// A PIN button that displays a digit number and corresponding letter mapping.
class LoginPinView::DigitPinButton : public BasePinButton {
  METADATA_HEADER(DigitPinButton, BasePinButton)

 public:
  DigitPinButton(int value,
                 bool show_sub_label,
                 const gfx::Size& size,
                 const LoginPinView::OnPinKey& on_key)
      : BasePinButton(size,
                      GetButtonLabelForNumber(value),
                      base::BindRepeating(on_key, value)) {
    SetID(GetViewIdForPinNumber(value));
    const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
    label_ = AddChildView(new views::Label(GetButtonLabelForNumber(value),
                                           views::style::CONTEXT_BUTTON,
                                           views::style::STYLE_PRIMARY));
    label_->SetAutoColorReadabilityEnabled(false);
    label_->SetSubpixelRenderingEnabled(false);
    label_->SetFontList(base_font_list.Derive(8 /*size_delta*/,
                                              gfx::Font::FontStyle::NORMAL,
                                              gfx::Font::Weight::NORMAL));
    label_->SetEnabledColorId(kColorAshIconColorPrimary);

    if (show_sub_label) {
      sub_label_ = AddChildView(new views::Label(
          GetButtonSubLabelForNumber(value), views::style::CONTEXT_BUTTON,
          views::style::STYLE_SECONDARY));
      sub_label_->SetAutoColorReadabilityEnabled(false);
      sub_label_->SetSubpixelRenderingEnabled(false);
      sub_label_->SetFontList(
          base_font_list.Derive(-1 /*size_delta*/, gfx::Font::FontStyle::NORMAL,
                                gfx::Font::Weight::NORMAL));
      sub_label_->SetEnabledColorId(kColorAshTextColorSecondary);
    }
  }

  DigitPinButton(const DigitPinButton&) = delete;
  DigitPinButton& operator=(const DigitPinButton&) = delete;

  ~DigitPinButton() override = default;

 private:
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::Label> sub_label_ = nullptr;
};

BEGIN_METADATA(LoginPinView, DigitPinButton)
END_METADATA

// A PIN button that displays backspace icon.
class LoginPinView::BackspacePinButton : public BasePinButton {
  METADATA_HEADER(BackspacePinButton, BasePinButton)

 public:
  BackspacePinButton(const gfx::Size& size,
                     const base::RepeatingClosure& on_press)
      : BasePinButton(size,
                      l10n_util::GetStringUTF16(
                          IDS_ASH_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME),
                      on_press) {
    image_ = AddChildView(new views::ImageView());
    SetEnabled(false);
  }

  BackspacePinButton(const BackspacePinButton&) = delete;
  BackspacePinButton& operator=(const BackspacePinButton&) = delete;

  ~BackspacePinButton() override = default;

  void SetTimersForTesting(std::unique_ptr<base::OneShotTimer> delay_timer,
                           std::unique_ptr<base::RepeatingTimer> repeat_timer) {
    delay_timer_ = std::move(delay_timer);
    repeat_timer_ = std::move(repeat_timer);
  }

  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override {
    return this;
  }

  std::u16string GetTooltipText(const gfx::Point& p) const override {
    return GetViewAccessibility().GetCachedName();
  }

  void OnEnabledChanged() {
    if (!GetEnabled()) {
      views::InkDrop::Get(this)->AnimateToState(
          views::InkDropState::DEACTIVATED, nullptr);
      CancelRepeat();
    }
    UpdateImage();
  }

  // BasePinButton:
  void OnEvent(ui::Event* event) override {
    BasePinButton::OnEvent(event);
    if (event->handled()) {
      return;
    }
    // If this is a button release style event cancel any repeat.
    if (event->type() == ui::EventType::kGestureTapCancel ||
        event->type() == ui::EventType::kGestureEnd ||
        event->type() == ui::EventType::kMouseReleased) {
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
      delay_timer_->Start(FROM_HERE,
                          base::Milliseconds(kInitialBackspaceDelayMs),
                          base::BindOnce(&BackspacePinButton::DispatchPress,
                                         base::Unretained(this), nullptr));

      if (event) {
        event->SetHandled();
      }

      views::InkDrop::Get(this)->AnimateToState(
          views::InkDropState::ACTIVATED, ui::LocatedEvent::FromIfValid(event));
      SchedulePaint();

      return;
    }

    // If here, then this function was fired by the delay_timer_. We need to
    // make sure the repeat_timer_ is running so the function will fire again.
    if (!repeat_timer_->IsRunning()) {
      repeat_timer_->Start(
          FROM_HERE, base::Milliseconds(kRepeatingBackspaceDelayMs),
          base::BindRepeating(&BackspacePinButton::DispatchPress,
                              base::Unretained(this), nullptr));
    }

    // Run handler.
    if (on_press_) {
      on_press_.Run();
    }
  }

 private:
  // Cancels a long-press. If the press event has not been triggered yet this
  // will trigger it.
  void CancelRepeat() {
    if (!is_held_) {
      return;
    }

    bool did_submit = !delay_timer_->IsRunning();
    delay_timer_->Stop();
    repeat_timer_->Stop();
    is_held_ = false;

    if (!did_submit && on_press_) {
      on_press_.Run();
    }

    views::InkDrop::Get(this)->AnimateToState(views::InkDropState::DEACTIVATED,
                                              nullptr);
    SchedulePaint();
  }

  void UpdateImage() {
    ui::ColorId color_id = GetEnabled() ? kColorAshIconColorPrimary
                                        : kColorAshIconPrimaryDisabledColor;
    image_->SetImage(
        ui::ImageModel::FromVectorIcon(kLockScreenBackspaceIcon, color_id));
  }

  bool is_held_ = false;
  std::unique_ptr<base::OneShotTimer> delay_timer_ =
      std::make_unique<base::OneShotTimer>();
  std::unique_ptr<base::RepeatingTimer> repeat_timer_ =
      std::make_unique<base::RepeatingTimer>();

  raw_ptr<views::ImageView> image_ = nullptr;
  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(base::BindRepeating(
          &LoginPinView::BackspacePinButton::OnEnabledChanged,
          base::Unretained(this)));
};

BEGIN_METADATA(LoginPinView, BackspacePinButton)
END_METADATA

// A PIN button to press to submit the PIN / password.
class LoginPinView::SubmitPinButton : public BasePinButton {
  METADATA_HEADER(SubmitPinButton, BasePinButton)

 public:
  SubmitPinButton(const gfx::Size& size, const base::RepeatingClosure& on_press)
      : BasePinButton(size,
                      l10n_util::GetStringUTF16(
                          IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME),
                      on_press) {
    image_ = AddChildView(std::make_unique<views::ImageView>());
    SetEnabled(false);
  }

  SubmitPinButton(const SubmitPinButton&) = delete;
  SubmitPinButton& operator=(const SubmitPinButton&) = delete;
  ~SubmitPinButton() override = default;

 private:
  void UpdateImage() {
    ui::ColorId color_id = GetEnabled() ? kColorAshIconColorPrimary
                                        : kColorAshIconPrimaryDisabledColor;
    image_->SetImage(
        ui::ImageModel::FromVectorIcon(kLockScreenArrowIcon, color_id));
  }

  raw_ptr<views::ImageView> image_ = nullptr;
  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&LoginPinView::SubmitPinButton::UpdateImage,
                              base::Unretained(this)));
};

BEGIN_METADATA(LoginPinView, SubmitPinButton)
END_METADATA

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
  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  GetButton(number)->OnEvent(&event);
}

LoginPinView::LoginPinView(Style keyboard_style,
                           const OnPinKey& on_key,
                           const OnPinBackspace& on_backspace,
                           const OnPinSubmit& on_submit)
    : NonAccessibleView(kLoginPinViewClassName) {
  DCHECK(on_key);
  DCHECK(on_backspace);

  // Layer rendering.
  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  bool show_letters = keyboard_style == Style::kAlphanumeric;

  auto add_digit_button = [&](View* row, int value) {
    digit_buttons_.push_back(row->AddChildView(
        new DigitPinButton(value, show_letters, kButtonSize, on_key)));
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
  backspace_ = row->AddChildView(
      std::make_unique<BackspacePinButton>(kButtonSize, on_backspace));
  add_digit_button(row, 0);

  // Only add the submit button if the callback is valid.
  if (!on_submit.is_null()) {
    submit_button_ = row->AddChildView(
        std::make_unique<SubmitPinButton>(kButtonSize, on_submit));
  }
}

LoginPinView::~LoginPinView() = default;

void LoginPinView::NotifyAccessibilityLocationChanged() {
  this->NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged,
                                 false /*send_native_event*/);
  for (NonAccessibleView* row : rows_) {
    row->NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged,
                                  false /*send_native_event*/);
  }
}

void LoginPinView::OnPasswordTextChanged(bool is_empty) {
  backspace_->SetEnabled(!is_empty);
  if (submit_button_) {
    submit_button_->SetEnabled(!is_empty);
  }
}

NonAccessibleView* LoginPinView::BuildAndAddRow() {
  auto* row = new NonAccessibleView();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  AddChildView(row);
  rows_.push_back(row);
  return row;
}

BEGIN_METADATA(LoginPinView)
END_METADATA

}  // namespace ash
