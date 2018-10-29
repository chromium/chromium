// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_popup_item_style.h"

#include "ash/system/tray/tray_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

constexpr int kInactiveAlpha = TrayPopupItemStyle::kInactiveIconAlpha * 0xFF;
constexpr int kDisabledAlpha = 0x61;

}  // namespace

// static
SkColor TrayPopupItemStyle::GetIconColor(ColorStyle color_style,
                                         bool use_unified_theme) {
  const SkColor kBaseIconColor =
      use_unified_theme ? kUnifiedMenuIconColor : gfx::kChromeIconGrey;
  switch (color_style) {
    case ColorStyle::ACTIVE:
      return kBaseIconColor;
    case ColorStyle::INACTIVE:
      return SkColorSetA(kBaseIconColor, kInactiveAlpha);
    case ColorStyle::DISABLED:
      return SkColorSetA(kBaseIconColor, kDisabledAlpha);
    case ColorStyle::CONNECTED:
      return gfx::kPlaceholderColor;
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

TrayPopupItemStyle::TrayPopupItemStyle(FontStyle font_style)
    : TrayPopupItemStyle(font_style, true) {}

TrayPopupItemStyle::TrayPopupItemStyle(FontStyle font_style,
                                       bool use_unified_theme)
    : font_style_(font_style),
      color_style_(ColorStyle::ACTIVE),
      use_unified_theme_(use_unified_theme) {
  if (font_style_ == FontStyle::SYSTEM_INFO)
    color_style_ = ColorStyle::INACTIVE;
}

TrayPopupItemStyle::~TrayPopupItemStyle() = default;

SkColor TrayPopupItemStyle::GetTextColor() const {
  const SkColor kBaseTextColor = use_unified_theme_
                                     ? kUnifiedMenuTextColor
                                     : SkColorSetA(SK_ColorBLACK, 0xDE);

  switch (color_style_) {
    case ColorStyle::ACTIVE:
      return kBaseTextColor;
    case ColorStyle::INACTIVE:
      return SkColorSetA(kBaseTextColor, kInactiveAlpha);
    case ColorStyle::DISABLED:
      return SkColorSetA(kBaseTextColor, kDisabledAlpha);
    case ColorStyle::CONNECTED:
      return use_unified_theme_ ? gfx::kGoogleGreenDark600
                                : gfx::kGoogleGreen700;
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

SkColor TrayPopupItemStyle::GetIconColor() const {
  return GetIconColor(color_style_, use_unified_theme_);
}

void TrayPopupItemStyle::SetupLabel(views::Label* label) const {
  label->SetEnabledColor(GetTextColor());
  label->SetAutoColorReadabilityEnabled(false);

  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  switch (font_style_) {
    case FontStyle::TITLE:
      label->SetFontList(base_font_list.Derive(use_unified_theme_ ? 8 : 1,
                                               gfx::Font::NORMAL,
                                               gfx::Font::Weight::MEDIUM));
      break;
    case FontStyle::DEFAULT_VIEW_LABEL:
      label->SetFontList(base_font_list.Derive(1, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
      break;
    case FontStyle::SUB_HEADER:
      label->SetFontList(base_font_list.Derive(use_unified_theme_ ? 4 : 1,
                                               gfx::Font::NORMAL,
                                               gfx::Font::Weight::MEDIUM));
      label->SetEnabledColor(
          use_unified_theme_
              ? kUnifiedMenuTextColor
              : label->GetNativeTheme()->GetSystemColor(
                    ui::NativeTheme::kColorId_ProminentButtonColor));
      label->SetAutoColorReadabilityEnabled(false);
      break;
    case FontStyle::DETAILED_VIEW_LABEL:
    case FontStyle::SYSTEM_INFO:
      label->SetFontList(base_font_list.Derive(1, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
      break;
    case FontStyle::CLICKABLE_SYSTEM_INFO:
      label->SetFontList(base_font_list.Derive(1, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
      label->SetEnabledColor(label->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_ProminentButtonColor));
      label->SetAutoColorReadabilityEnabled(false);
      break;
    case FontStyle::CAPTION:
      label->SetFontList(base_font_list.Derive(0, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
      break;
  }
}

}  // namespace ash
