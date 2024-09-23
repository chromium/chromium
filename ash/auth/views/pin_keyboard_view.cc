// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/pin_keyboard_view.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr int kButtonSize = 48;
constexpr int kButtonsVerticalPadding = 12;
constexpr int kButtonsHorizontalPadding = 16;
constexpr int kPinKeyboardHeightDp =
    4 * kButtonSize + 3 * kButtonsVerticalPadding;
constexpr int kPinKeyboardWidthDp =
    3 * kButtonSize + 2 * kButtonsHorizontalPadding;

constexpr ui::ColorId kButtonBackgroundColorId =
    cros_tokens::kCrosSysSystemBaseElevated;
constexpr ui::ColorId kButtonContentColorId = cros_tokens::kCrosSysOnSurface;

void StyleButton(IconButton* button_ptr) {
  button_ptr->SetBackgroundColor(kButtonBackgroundColorId);
  button_ptr->SetIconColor(kButtonContentColorId);
}

}  // namespace

//----------------------- PinKeyboardView Test API ------------------------

PinKeyboardView::TestApi::TestApi(PinKeyboardView* view) : view_(view) {
  CHECK(view_);
}

PinKeyboardView::TestApi::~TestApi() = default;

views::Button* PinKeyboardView::TestApi::backspace_button() {
  return view_->backspace_button_;
}

views::Button* PinKeyboardView::TestApi::digit_button(int digit) {
  CHECK(view_->digit_buttons_.contains(digit));
  CHECK(view_->digit_buttons_[digit]);
  return view_->digit_buttons_[digit];
}

bool PinKeyboardView::TestApi::GetEnabled() {
  return view_->GetEnabled();
}

void PinKeyboardView::TestApi::SetEnabled(bool enabled) {
  view_->SetEnabled(enabled);
}

void PinKeyboardView::TestApi::AddObserver(
    PinKeyboardView::Observer* observer) {
  view_->AddObserver(observer);
}

void PinKeyboardView::TestApi::RemoveObserver(
    PinKeyboardView::Observer* observer) {
  view_->RemoveObserver(observer);
}

PinKeyboardView* PinKeyboardView::TestApi::GetView() {
  return view_;
}

//----------------------- PinKeyboardView ------------------------

PinKeyboardView::PinKeyboardView() {
  GetViewAccessibility().SetRole(ax::mojom::Role::kKeyboard);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_AUTH_PIN_KEYBOARD));
  // The pin pad is always LTR.
  SetFlipCanvasOnPaintForRTLUI(false);

  // 3x4 with the following layout
  // 1 2 3
  // 4 5 6
  // 7 8 9
  //<- 0
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kStart,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kFixed, kButtonSize, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        kButtonsHorizontalPadding)
      .AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, kButtonSize, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        kButtonsHorizontalPadding)
      .AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, kButtonSize, 0)
      .AddRows(1, 0, kButtonSize)
      .AddPaddingRow(0, kButtonsVerticalPadding)
      .AddRows(1, 0, kButtonSize)
      .AddPaddingRow(0, kButtonsVerticalPadding)
      .AddRows(1, 0, kButtonSize)
      .AddPaddingRow(0, kButtonsVerticalPadding)
      .AddRows(1, 0, kButtonSize);

  //----------------------- First row ------------------------

  AddDigitButton(1);
  AddDigitButton(2);
  AddDigitButton(3);

  //----------------------- Second row ------------------------

  AddDigitButton(4);
  AddDigitButton(5);
  AddDigitButton(6);

  //----------------------- Third row ------------------------

  AddDigitButton(7);
  AddDigitButton(8);
  AddDigitButton(9);

  //----------------------- Fourth row ------------------------

  backspace_button_ = AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&PinKeyboardView::OnBackspacePressed,
                          weak_ptr_factory_.GetWeakPtr()),
      IconButton::Type::kXLarge, &kLockScreenBackspaceIcon,
      IDS_ASH_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME));
  StyleButton(backspace_button_);
  AddDigitButton(0);
}

PinKeyboardView::~PinKeyboardView() = default;

gfx::Size PinKeyboardView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kPinKeyboardWidthDp, kPinKeyboardHeightDp);
}

void PinKeyboardView::OnDigitButtonPressed(int digit) {
  CHECK_GE(digit, 0);
  CHECK_LT(digit, 10);
  CHECK(GetEnabled());
  for (auto& observer : observers_) {
    observer.OnDigitButtonPressed(digit);
  }
}

void PinKeyboardView::OnBackspacePressed() {
  CHECK(GetEnabled());
  for (auto& observer : observers_) {
    observer.OnBackspacePressed();
  }
}

void PinKeyboardView::AddDigitButton(int digit) {
  CHECK_GE(digit, 0);
  CHECK_LT(digit, 10);

  IconButton::Builder builder;
  builder.SetType(IconButton::Type::kXLarge)
      .SetAccessibleName(base::NumberToString16(digit))
      .SetCallback(base::BindRepeating(&PinKeyboardView::OnDigitButtonPressed,
                                       weak_ptr_factory_.GetWeakPtr(), digit))
      .SetTogglable(false)
      .SetEnabled(true)
      .SetSymbol(static_cast<char>('0' + digit));

  auto* button_ptr = AddChildView(builder.Build());

  digit_buttons_[digit] = button_ptr;
  StyleButton(button_ptr);
}

void PinKeyboardView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PinKeyboardView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

BEGIN_METADATA(PinKeyboardView)
END_METADATA

}  // namespace ash
