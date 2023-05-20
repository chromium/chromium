// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

namespace {

// Arrow symbols for arrow keys.
constexpr char16_t kLeftArrow[] = u"←";
constexpr char16_t kUpArrow[] = u"↑";
constexpr char16_t kRightArrow[] = u"→";
constexpr char16_t kDownArrow[] = u"↓";
constexpr char16_t kBackQuote[] = u"`";
constexpr char16_t kMinus[] = u"-";
constexpr char16_t kEqual[] = u"=";
constexpr char16_t kBracketLeft[] = u"[";
constexpr char16_t kBracketRight[] = u"]";
constexpr char16_t kBackSlash[] = u"\\";
constexpr char16_t kSemicolon[] = u";";
constexpr char16_t kQuote[] = u"'";
constexpr char16_t kComma[] = u",";
constexpr char16_t kPeriod[] = u".";
constexpr char16_t kSlash[] = u"/";
constexpr char16_t kBackSpace[] = u"⌫";
constexpr char16_t kEnter[] = u"↵";
constexpr char16_t kSpace[] = u"␣";
constexpr char16_t kEscape[] = u"esc";

// Modifier keys.
constexpr char16_t kAlt[] = u"alt";
constexpr char16_t kCtrl[] = u"ctrl";
constexpr char16_t kShift[] = u"shift";
constexpr char16_t kCap[] = u"cap";
}  // namespace

std::unique_ptr<views::View> CreateNameTag(const std::u16string& title,
                                           const std::u16string& sub_title) {
  auto name_tag = std::make_unique<views::View>();
  name_tag->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  name_tag->AddChildView(
      ash::bubble_utils::CreateLabel(ash::TypographyToken::kCrosButton1, title,
                                     cros_tokens::kCrosRefNeutral100));
  name_tag->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosAnnotation2, sub_title,
      cros_tokens::kCrosSysSecondary));
  return name_tag;
}

std::unique_ptr<views::View> CreateActionTapEditForKeyboard(Action* action) {
  return std::make_unique<EditLabel>(action);
}

std::unique_ptr<views::View> CreateActionMoveEditForKeyboard(Action* action) {
  auto keys = std::make_unique<views::View>();
  // Create a 2x3 table with column and row padding of 4.
  keys->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                  /*v_align=*/views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/4)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/4)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize)
      .AddPaddingRow(/*vertical_resize=*/views::TableLayout::kFixedSize,
                     /*height=*/4)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);

  int index = 0;
  for (int i = 0; i < 6; i++) {
    // Column 1 row 1 and Column 3 row 1 are empty.
    if (i == 0 || i == 2) {
      keys->AddChildView(std::make_unique<views::View>());
    } else {
      keys->AddChildView(std::make_unique<EditLabel>(action, index++));
    }
  }
  return keys;
}

std::u16string GetDisplayText(const ui::DomCode code) {
  switch (code) {
    case ui::DomCode::NONE:
      return kUnknownBind;
    case ui::DomCode::ARROW_LEFT:
      return kLeftArrow;
    case ui::DomCode::ARROW_RIGHT:
      return kRightArrow;
    case ui::DomCode::ARROW_UP:
      return kUpArrow;
    case ui::DomCode::ARROW_DOWN:
      return kDownArrow;
    case ui::DomCode::BACKQUOTE:
      return kBackQuote;
    case ui::DomCode::MINUS:
      return kMinus;
    case ui::DomCode::EQUAL:
      return kEqual;
    case ui::DomCode::BRACKET_LEFT:
      return kBracketLeft;
    case ui::DomCode::BRACKET_RIGHT:
      return kBracketRight;
    case ui::DomCode::BACKSLASH:
      return kBackSlash;
    case ui::DomCode::SEMICOLON:
      return kSemicolon;
    case ui::DomCode::QUOTE:
      return kQuote;
    case ui::DomCode::COMMA:
      return kComma;
    case ui::DomCode::PERIOD:
      return kPeriod;
    case ui::DomCode::SLASH:
      return kSlash;
    case ui::DomCode::BACKSPACE:
      return kBackSpace;
    case ui::DomCode::ENTER:
      return kEnter;
    case ui::DomCode::ESCAPE:
      return kEscape;
    // Modifier keys.
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
      return kAlt;
    case ui::DomCode::CONTROL_LEFT:
    case ui::DomCode::CONTROL_RIGHT:
      return kCtrl;
    case ui::DomCode::SHIFT_LEFT:
    case ui::DomCode::SHIFT_RIGHT:
      return kShift;
    case ui::DomCode::CAPS_LOCK:
      return kCap;
    case ui::DomCode::SPACE:
      return kSpace;
    default:
      std::string dom_code_string =
          ui::KeycodeConverter::DomCodeToCodeString(code);
      if (base::StartsWith(dom_code_string, "Key",
                           base::CompareCase::SENSITIVE)) {
        return base::UTF8ToUTF16(base::ToLowerASCII(dom_code_string.substr(3)));
      }
      if (base::StartsWith(dom_code_string, "Digit",
                           base::CompareCase::SENSITIVE)) {
        return base::UTF8ToUTF16(dom_code_string.substr(5));
      }
      // TODO(b/282843422): better display for number pad. Current it shows in
      // the format of "numpad1" since the number keys on number pad are not
      // considered the same as numbers on the main keyboard.
      auto lower = base::ToLowerASCII(dom_code_string);
      return base::UTF8ToUTF16(lower);
  }
}

std::u16string GetDisplayTextAccessibleName(const std::u16string& text) {
  if (text.compare(kSpace) == 0) {
    return l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_KEY_LABEL_SPACE);
  } else if (text.compare(kEnter) == 0) {
    return l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_KEY_LABEL_ENTER);
  } else if (text.compare(kBackSpace) == 0) {
    return l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_KEY_LABEL_BACKSPACE);
  } else {
    return text;
  }
}

}  // namespace arc::input_overlay
