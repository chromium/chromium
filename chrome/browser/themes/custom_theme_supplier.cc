// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/custom_theme_supplier.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/native_theme.h"

namespace {

using TP = ThemeProperties;

// The minimum contrast the omnibox background must have against the toolbar.
constexpr float kMinOmniboxToolbarContrast = 1.3f;

ui::ColorTransform ChooseOmniboxBgBlendTarget() {
  return base::BindRepeating(
      [](SkColor input_color, const ui::ColorMixer& mixer) {
        const SkColor toolbar_color = mixer.GetResultColor(kColorToolbar);
        const SkColor endpoint_color =
            color_utils::GetEndpointColorWithMinContrast(toolbar_color);
        return (color_utils::GetContrastRatio(toolbar_color, endpoint_color) >=
                kMinOmniboxToolbarContrast)
                   ? endpoint_color
                   : color_utils::GetColorWithMaxContrast(endpoint_color);
      });
}

}  // namespace

CustomThemeSupplier::~CustomThemeSupplier() = default;

std::string_view CustomThemeSupplier::extension_id() const {
  return ThemeHelper::kDefaultThemeID;
}

void CustomThemeSupplier::StartUsingTheme() {}

void CustomThemeSupplier::StopUsingTheme() {}

bool CustomThemeSupplier::GetTint(int id, color_utils::HSL* hsl) const {
  return false;
}

bool CustomThemeSupplier::GetColor(int id, SkColor* color) const {
  return false;
}

bool CustomThemeSupplier::GetDisplayProperty(int id, int* result) const {
  return false;
}

gfx::Image CustomThemeSupplier::GetImageNamed(int id) const {
  return gfx::Image();
}

scoped_refptr<base::RefCountedMemory> CustomThemeSupplier::GetRawData(
    int idr_id,
    ui::ResourceScaleFactor scale_factor) const {
  return nullptr;
}

bool CustomThemeSupplier::HasCustomImage(int id) const {
  return false;
}

ui::NativeTheme* CustomThemeSupplier::GetNativeTheme() const {
  return ui::NativeTheme::GetInstanceForNativeUi();
}

void CustomThemeSupplier::SetColor(int id, SkColor color) {
  NOTREACHED();
}

void CustomThemeSupplier::SetColorIfUnspecified(int id, SkColor color) {
  SkColor unused;
  if (!GetColor(id, &unused)) {
    SetColor(id, color);
  }
}

void CustomThemeSupplier::SetFrameAndToolbarRelatedColors() {
  // Propagate the user-specified toolbar color to similar elements.
  SkColor toolbar_color;
  if (GetColor(TP::COLOR_TOOLBAR, &toolbar_color)) {
    // If the toolbar color is set but the text color is not, ensure it has
    // sufficient contrast.
    SkColor toolbar_text_color =
        TP::GetDefaultColor(TP::COLOR_TOOLBAR_TEXT, /*incognito=*/false);
    toolbar_text_color =
        color_utils::BlendForMinContrast(toolbar_text_color, toolbar_color)
            .color;
    SetColorIfUnspecified(TP::COLOR_TOOLBAR_TEXT, toolbar_text_color);
  } else {
    toolbar_color = TP::GetDefaultColor(TP::COLOR_TOOLBAR, /*incognito=*/false);
  }
  SkColor toolbar_button_icon_color;
  color_utils::HSL button_tint;
  if (GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, &toolbar_button_icon_color)) {
    SetColor(TP::COLOR_TOOLBAR_BUTTON_ICON_HOVERED, toolbar_button_icon_color);
    SetColor(TP::COLOR_TOOLBAR_BUTTON_ICON_PRESSED, toolbar_button_icon_color);
    SetColor(TP::COLOR_TAB_THROBBER_SPINNING, toolbar_button_icon_color);
    SetColor(TP::COLOR_TAB_THROBBER_WAITING, toolbar_button_icon_color);
  } else if (GetTint(TP::TINT_BUTTONS, &button_tint)) {
    // Duplicate how COLOR_TOOLBAR_BUTTON_ICON will be computed.
    // TODO(pkasting): Should this code be shared with
    // ThemeHelper::GetDefaultColor() somehow?
    const SkColor button_color =
        color_utils::HSLShift(gfx::kGoogleGrey700, button_tint);
    SetColor(TP::COLOR_TAB_THROBBER_SPINNING, button_color);
    SetColor(TP::COLOR_TAB_THROBBER_WAITING, button_color);
  }
  SkColor toolbar_text_color;
  if (GetColor(TP::COLOR_TOOLBAR_TEXT, &toolbar_text_color)) {
    SetColorIfUnspecified(TP::COLOR_BOOKMARK_TEXT, toolbar_text_color);
    SetColorIfUnspecified(TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE,
                          toolbar_text_color);
  }
  SkColor tab_foreground_color;
  if (GetColor(TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE,
               &tab_foreground_color)) {
    SetColor(TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE,
             tab_foreground_color);
  }
  SkColor omnibox_background_color;
  if (GetColor(TP::COLOR_OMNIBOX_BACKGROUND, &omnibox_background_color)) {
    omnibox_background_color = color_utils::GetResultingPaintColor(
        omnibox_background_color, toolbar_color);
    SetColor(TP::COLOR_OMNIBOX_BACKGROUND, omnibox_background_color);
  } else {
    // TODO(pkasting): This should be shared with the omnibox color mixer.
    omnibox_background_color = gfx::kGoogleGrey100;
  }
  SkColor omnibox_text_color;
  if (GetColor(TP::COLOR_OMNIBOX_TEXT, &omnibox_text_color)) {
    SetColor(TP::COLOR_OMNIBOX_TEXT,
             color_utils::GetResultingPaintColor(omnibox_text_color,
                                                 omnibox_background_color));
  }
}

void CustomThemeSupplier::AddColorMixers(
    ui::ColorProvider* provider,
    const ui::ColorProviderKey& key) const {
  ui::ColorMixer& mixer = provider->AddMixer();

  // TODO(http://crbug.com/41410580): Enable for all cases.
  mixer[kColorToolbarBackgroundSubtleEmphasis] = ui::BlendForMinContrast(
      kColorToolbar, kColorToolbar, ChooseOmniboxBgBlendTarget(),
      kMinOmniboxToolbarContrast);

  // A map from theme property IDs to color IDs for use in color mixers.
  constexpr struct {
    int property_id;
    int color_id;
  } kThemePropertiesMap[] = {
      {TP::COLOR_BOOKMARK_TEXT, kColorBookmarkBarForeground},
      {TP::COLOR_CONTROL_BUTTON_BACKGROUND, kColorCaptionButtonBackground},
      {TP::COLOR_FRAME_ACTIVE, ui::kColorFrameActive},
      {TP::COLOR_FRAME_INACTIVE, ui::kColorFrameInactive},
      {TP::COLOR_NTP_BACKGROUND, kColorNewTabPageBackground},
      {TP::COLOR_NTP_HEADER, kColorNewTabPageHeader},
      {TP::COLOR_NTP_LINK, kColorNewTabPageLink},
      {TP::COLOR_NTP_LOGO, kColorNewTabPageLogo},
      {TP::COLOR_NTP_SECTION_BORDER, kColorNewTabPageSectionBorder},
      {TP::COLOR_NTP_TEXT, kColorNewTabPageText},
      {TP::COLOR_OMNIBOX_TEXT, kColorOmniboxText},
      {TP::COLOR_OMNIBOX_BACKGROUND, kColorOmniboxResultsBackground},
      {TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE,
       kColorTabBackgroundInactiveFrameActive},
      {TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE,
       kColorTabBackgroundInactiveFrameInactive},
      {TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE,
       kColorTabForegroundActiveFrameActive},
      {TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE,
       kColorTabForegroundActiveFrameInactive},
      {TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE,
       kColorTabForegroundInactiveFrameActive},
      {TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE,
       kColorTabForegroundInactiveFrameInactive},
      {TP::COLOR_TAB_THROBBER_SPINNING, kColorTabThrobber},
      {TP::COLOR_TAB_THROBBER_WAITING, kColorTabThrobberPreconnect},
      {TP::COLOR_TOOLBAR, kColorToolbar},
      {TP::COLOR_TOOLBAR_BUTTON_ICON, kColorToolbarButtonIcon},
      {TP::COLOR_TOOLBAR_BUTTON_ICON_HOVERED, kColorToolbarButtonIconHovered},
      {TP::COLOR_TOOLBAR_BUTTON_ICON_PRESSED, kColorToolbarButtonIconPressed},
      {TP::COLOR_TOOLBAR_TEXT, kColorToolbarText},
      {TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE,
       kColorWindowControlButtonBackgroundActive},
      {TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE,
       kColorWindowControlButtonBackgroundInactive}};

  SkColor color;
  for (const auto& entry : kThemePropertiesMap) {
    if (GetColor(entry.property_id, &color)) {
      mixer[entry.color_id] = {color};
    }
  }

  if (GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, &color)) {
    mixer[kColorBookmarkFavicon] = {kColorToolbarButtonIcon};
  }

  if (GetColor(TP::COLOR_BOOKMARK_TEXT, &color)) {
    mixer[kColorBookmarkFolderIcon] =
        ui::DeriveDefaultIconColor(kColorBookmarkBarForeground);
  }

  int result = 0;
  if (GetDisplayProperty(TP::SHOULD_FILL_BACKGROUND_TAB_COLOR, &result) &&
      result == 0) {
    mixer[kColorNewTabButtonBackgroundFrameActive] = {SK_ColorTRANSPARENT};
    mixer[kColorNewTabButtonBackgroundFrameInactive] = {SK_ColorTRANSPARENT};
  }
}
