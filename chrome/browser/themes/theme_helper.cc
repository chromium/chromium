// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/no_destructor.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

namespace {

using TP = ThemeProperties;

// The default theme if we've gone to the theme gallery and installed the
// "Default" theme. We have to detect this case specifically. (By the time we
// realize we've installed the default theme, we already have an extension
// unpacked on the filesystem.)
constexpr char kDefaultThemeGalleryID[] = "hkacjpbfdknhflllbcmjibkdeoafencn";

const std::array<SkColor, 2> GetTabGroupColors(int color_id) {
  switch (color_id) {
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_BLUE:
    case TP::COLOR_TAB_GROUP_DIALOG_BLUE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_BLUE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_BLUE:
      return {gfx::kGoogleBlue600, gfx::kGoogleBlue300};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_RED:
    case TP::COLOR_TAB_GROUP_DIALOG_RED:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_RED:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_RED:
      return {gfx::kGoogleRed600, gfx::kGoogleRed300};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_YELLOW:
    case TP::COLOR_TAB_GROUP_DIALOG_YELLOW:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_YELLOW:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_YELLOW:
      return {gfx::kGoogleYellow900, gfx::kGoogleYellow300};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_GREEN:
    case TP::COLOR_TAB_GROUP_DIALOG_GREEN:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREEN:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREEN:
      return {gfx::kGoogleGreen600, gfx::kGoogleGreen300};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_PINK:
    case TP::COLOR_TAB_GROUP_DIALOG_PINK:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PINK:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PINK:
      return {gfx::kGooglePink700, gfx::kGooglePink300};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_PURPLE:
    case TP::COLOR_TAB_GROUP_DIALOG_PURPLE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PURPLE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PURPLE:
      return {gfx::kGooglePurple600, gfx::kGooglePurple200};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_CYAN:
    case TP::COLOR_TAB_GROUP_DIALOG_CYAN:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_CYAN:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_CYAN:
      return {gfx::kGoogleCyan900, gfx::kGoogleCyan300};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_GREY:
    case TP::COLOR_TAB_GROUP_DIALOG_GREY:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREY:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREY:
    default:
      return {gfx::kGoogleGrey700, gfx::kGoogleGrey400};
  }
}

// Translate the relevant ThemeProperty color ids to SecurityChipColorIds so
// that the security chip color implementation can be shared between NativeTheme
// and ThemeProvider.
ui::NativeTheme::SecurityChipColorId GetSecurityChipColorId(int color_id) {
  static const base::NoDestructor<
      base::flat_map<int, ui::NativeTheme::SecurityChipColorId>>
      color_id_map({
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT,
           ui::NativeTheme::SecurityChipColorId::DEFAULT},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE,
           ui::NativeTheme::SecurityChipColorId::SECURE},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS,
           ui::NativeTheme::SecurityChipColorId::DANGEROUS},
      });
  return color_id_map->at(color_id);
}

SkColor IncreaseLightness(SkColor color, double percent) {
  color_utils::HSL result;
  color_utils::SkColorToHSL(color, &result);
  result.l += (1 - result.l) * percent;
  return color_utils::HSLToSkColor(result, SkColorGetA(color));
}

// For legacy reasons, the theme supplier requires the incognito variants of
// color IDs.  This converts from normal to incognito IDs where they exist.
int GetIncognitoId(int id) {
  switch (id) {
    case TP::COLOR_FRAME_ACTIVE:
      return TP::COLOR_FRAME_ACTIVE_INCOGNITO;
    case TP::COLOR_FRAME_INACTIVE:
      return TP::COLOR_FRAME_INACTIVE_INCOGNITO;
    case TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE:
      return TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO;
    case TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE:
      return TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO;
    case TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE:
      return TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO;
    case TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE:
      return TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO;
    case TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE:
      return TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_ACTIVE;
    case TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE:
      return TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_INACTIVE;
    default:
      return id;
  }
}

// Key for cache of separator colors; pair is <tab color, frame color>.
using SeparatorColorKey = std::pair<SkColor, SkColor>;
using SeparatorColorCache = std::map<SeparatorColorKey, SkColor>;

SeparatorColorCache& GetSeparatorColorCache() {
  static base::NoDestructor<SeparatorColorCache> cache;
  return *cache;
}

}  // namespace

const char ThemeHelper::kDefaultThemeID[] = "";

// static
bool ThemeHelper::IsExtensionTheme(const CustomThemeSupplier* theme_supplier) {
  return theme_supplier && theme_supplier->get_theme_type() ==
                               CustomThemeSupplier::ThemeType::EXTENSION;
}

// static
bool ThemeHelper::IsAutogeneratedTheme(
    const CustomThemeSupplier* theme_supplier) {
  return theme_supplier && theme_supplier->get_theme_type() ==
                               CustomThemeSupplier::ThemeType::AUTOGENERATED;
}

// static
bool ThemeHelper::IsDefaultTheme(const CustomThemeSupplier* theme_supplier) {
  if (!theme_supplier)
    return true;

  using Type = CustomThemeSupplier::ThemeType;

  switch (theme_supplier->get_theme_type()) {
    case Type::INCREASED_CONTRAST:
      return true;
    case Type::EXTENSION: {
      const std::string& id = theme_supplier->extension_id();
      return id == kDefaultThemeID || id == kDefaultThemeGalleryID;
    }
    case Type::NATIVE_X11:
    case Type::AUTOGENERATED:
      return false;
  }
}

// static
bool ThemeHelper::IsCustomTheme(const CustomThemeSupplier* theme_supplier) {
  return IsExtensionTheme(theme_supplier) ||
         IsAutogeneratedTheme(theme_supplier);
}

// static
bool ThemeHelper::HasCustomImage(int id,
                                 const CustomThemeSupplier* theme_supplier) {
  return BrowserThemePack::IsPersistentImageID(id) && theme_supplier &&
         theme_supplier->HasCustomImage(id);
}

// static
int ThemeHelper::GetDisplayProperty(int id,
                                    const CustomThemeSupplier* theme_supplier) {
  int result = 0;
  if (theme_supplier && theme_supplier->GetDisplayProperty(id, &result)) {
    return result;
  }

  switch (id) {
    case TP::NTP_BACKGROUND_ALIGNMENT:
      return TP::ALIGN_CENTER;

    case TP::NTP_BACKGROUND_TILING:
      return TP::NO_REPEAT;

    case TP::NTP_LOGO_ALTERNATE:
      return 0;

    case TP::SHOULD_FILL_BACKGROUND_TAB_COLOR:
      return 1;

    default:
      return -1;
  }
}

// static
base::RefCountedMemory* ThemeHelper::GetRawData(
    int id,
    const CustomThemeSupplier* theme_supplier,
    ui::ScaleFactor scale_factor) {
  // Check to see whether we should substitute some images.
  int ntp_alternate =
      GetDisplayProperty(TP::NTP_LOGO_ALTERNATE, theme_supplier);
  if (id == IDR_PRODUCT_LOGO && ntp_alternate != 0)
    id = IDR_PRODUCT_LOGO_WHITE;

  base::RefCountedMemory* data = nullptr;
  if (theme_supplier)
    data = theme_supplier->GetRawData(id, scale_factor);
  if (!data) {
    data =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
            id, ui::SCALE_FACTOR_100P);
  }

  return data;
}

ThemeHelper::ThemeHelper() = default;

ThemeHelper::~ThemeHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SkColor ThemeHelper::GetColor(int id,
                              bool incognito,
                              const CustomThemeSupplier* theme_supplier,
                              bool* has_custom_color) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (has_custom_color)
    *has_custom_color = false;

  const base::Optional<SkColor> omnibox_color =
      GetOmniboxColor(id, incognito, theme_supplier, has_custom_color);
  if (omnibox_color.has_value())
    return omnibox_color.value();

  if (theme_supplier &&
      !ShouldIgnoreThemeSupplier(id, incognito, theme_supplier)) {
    SkColor color;
    const int theme_supplier_id = incognito ? GetIncognitoId(id) : id;
    if (theme_supplier->GetColor(theme_supplier_id, &color)) {
      if (has_custom_color)
        *has_custom_color = true;
      return color;
    }
  }

  return GetDefaultColor(id, incognito, theme_supplier);
}

color_utils::HSL ThemeHelper::GetTint(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  color_utils::HSL hsl;
  if (theme_supplier && theme_supplier->GetTint(id, &hsl))
    return hsl;

  // Incognito tints are ignored for custom themes so they apply atop a
  // predictable state.
  return TP::GetDefaultTint(id,
                            incognito && UseIncognitoColor(id, theme_supplier),
                            UseDarkModeColors(theme_supplier));
}

gfx::ImageSkia* ThemeHelper::GetImageSkiaNamed(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  gfx::Image image = GetImageNamed(id, incognito, theme_supplier);
  if (image.IsEmpty())
    return nullptr;
  // TODO(pkotwicz): Remove this const cast.  The gfx::Image interface returns
  // its images const. GetImageSkiaNamed() also should but has many callsites.
  return const_cast<gfx::ImageSkia*>(image.ToImageSkia());
}

bool ThemeHelper::ShouldUseNativeFrame(
    const CustomThemeSupplier* theme_supplier) const {
  return false;
}

bool ThemeHelper::ShouldUseIncreasedContrastThemeSupplier(
    ui::NativeTheme* native_theme) const {
  return native_theme && native_theme->UsesHighContrastColors();
}

SkColor ThemeHelper::GetDefaultColor(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  if (TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREY <= id &&
      id <= TP::COLOR_TAB_GROUP_CONTEXT_MENU_CYAN)
    return GetTabGroupColor(id, incognito, theme_supplier);

  // For backward compat with older themes, some newer colors are generated from
  // older ones if they are missing.
  const auto get_frame_color = [this, incognito, theme_supplier](bool active) {
    return GetColor(active ? TP::COLOR_FRAME_ACTIVE : TP::COLOR_FRAME_INACTIVE,
                    incognito, theme_supplier);
  };
  switch (id) {
    case TP::COLOR_OMNIBOX_BACKGROUND: {
      // TODO(http://crbug.com/878664): Enable for all cases.
      if (!IsCustomTheme(theme_supplier))
        break;
      constexpr float kMinOmniboxToolbarContrast = 1.3f;
      const SkColor toolbar_color =
          GetColor(TP::COLOR_TOOLBAR, incognito, theme_supplier);
      const SkColor endpoint_color =
          color_utils::GetEndpointColorWithMinContrast(toolbar_color);
      const SkColor blend_target =
          (color_utils::GetContrastRatio(toolbar_color, endpoint_color) >=
           kMinOmniboxToolbarContrast)
              ? endpoint_color
              : color_utils::GetColorWithMaxContrast(endpoint_color);
      return color_utils::BlendForMinContrast(toolbar_color, toolbar_color,
                                              blend_target,
                                              kMinOmniboxToolbarContrast)
          .color;
    }
    case TP::COLOR_OMNIBOX_TEXT:
      // TODO(http://crbug.com/878664): Enable for all cases.
      if (!IsCustomTheme(theme_supplier))
        break;
      return color_utils::GetColorWithMaxContrast(
          GetColor(TP::COLOR_OMNIBOX_BACKGROUND, incognito, theme_supplier));
    case TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE:
      return color_utils::HSLShift(get_frame_color(/*active=*/true),
                                   GetTint(ThemeProperties::TINT_BACKGROUND_TAB,
                                           incognito, theme_supplier));
    case TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE:
      return color_utils::HSLShift(get_frame_color(/*active=*/false),
                                   GetTint(ThemeProperties::TINT_BACKGROUND_TAB,
                                           incognito, theme_supplier));
    case TP::COLOR_TOOLBAR_BUTTON_ICON:
    case TP::COLOR_TOOLBAR_BUTTON_ICON_HOVERED:
    case TP::COLOR_TOOLBAR_BUTTON_ICON_PRESSED:
      return color_utils::HSLShift(
          gfx::kChromeIconGrey,
          GetTint(TP::TINT_BUTTONS, incognito, theme_supplier));
    case TP::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE:
      // The active color is overridden in GtkUi.
      return SkColorSetA(
          GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, incognito, theme_supplier),
          0x6E);
    case TP::COLOR_LOCATION_BAR_BORDER:
      return SkColorSetA(SK_ColorBLACK, 0x4D);
    case TP::COLOR_TOOLBAR_TOP_SEPARATOR:
    case TP::COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE: {
      const SkColor tab_color =
          GetColor(TP::COLOR_TOOLBAR, incognito, theme_supplier);
      const SkColor frame_color =
          get_frame_color(/*active=*/id == TP::COLOR_TOOLBAR_TOP_SEPARATOR);
      const SeparatorColorKey key(tab_color, frame_color);
      auto i = GetSeparatorColorCache().find(key);
      if (i != GetSeparatorColorCache().end())
        return i->second;
      const SkColor separator_color = GetSeparatorColor(tab_color, frame_color);
      GetSeparatorColorCache()[key] = separator_color;
      return separator_color;
    }
    case TP::COLOR_TOOLBAR_VERTICAL_SEPARATOR: {
      return SkColorSetA(
          GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, incognito, theme_supplier),
          0x4D);
    }
    case TP::COLOR_TOOLBAR_INK_DROP: {
      return color_utils::GetColorWithMaxContrast(
          GetColor(TP::COLOR_TOOLBAR, incognito, theme_supplier));
    }
    case TP::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR:
      if (IsDefaultTheme(theme_supplier))
        break;
      return GetColor(TP::COLOR_LOCATION_BAR_BORDER, incognito, theme_supplier);
    case TP::COLOR_NTP_TEXT_LIGHT:
      return IncreaseLightness(
          GetColor(TP::COLOR_NTP_TEXT, incognito, theme_supplier), 0.40);
    case TP::COLOR_TAB_THROBBER_SPINNING:
    case TP::COLOR_TAB_THROBBER_WAITING: {
      SkColor base_color =
          ui::GetAuraColor(id == TP::COLOR_TAB_THROBBER_SPINNING
                               ? ui::NativeTheme::kColorId_ThrobberSpinningColor
                               : ui::NativeTheme::kColorId_ThrobberWaitingColor,
                           ui::NativeTheme::GetInstanceForNativeUi());
      color_utils::HSL hsl =
          GetTint(TP::TINT_BUTTONS, incognito, theme_supplier);
      return color_utils::HSLShift(base_color, hsl);
    }
  }

  return TP::GetDefaultColor(id,
                             incognito && UseIncognitoColor(id, theme_supplier),
                             UseDarkModeColors(theme_supplier));
}

// static
SkColor ThemeHelper::GetSeparatorColor(SkColor tab_color, SkColor frame_color) {
  const float kContrastRatio = 2.f;

  // In most cases, if the tab is lighter than the frame, we darken the
  // frame; if the tab is darker than the frame, we lighten the frame.
  // However, if the frame is already very dark or very light, respectively,
  // this won't contrast sufficiently with the frame color, so we'll need to
  // reverse when we're lightening and darkening.
  SkColor separator_color = SK_ColorWHITE;
  if (color_utils::GetRelativeLuminance(tab_color) >=
      color_utils::GetRelativeLuminance(frame_color)) {
    separator_color = color_utils::GetColorWithMaxContrast(separator_color);
  }

  {
    const auto result = color_utils::BlendForMinContrast(
        frame_color, frame_color, separator_color, kContrastRatio);
    if (color_utils::GetContrastRatio(result.color, frame_color) >=
        kContrastRatio) {
      return SkColorSetA(separator_color, result.alpha);
    }
  }

  separator_color = color_utils::GetColorWithMaxContrast(separator_color);

  // If the above call failed to create sufficient contrast, the frame color is
  // already very dark or very light.  Since separators are only used when the
  // tab has low contrast against the frame, the tab color is similarly very
  // dark or very light, just not quite as much so as the frame color.  Blend
  // towards the opposite separator color, and compute the contrast against the
  // tab instead of the frame to ensure both contrasts hit the desired minimum.
  const auto result = color_utils::BlendForMinContrast(
      frame_color, tab_color, separator_color, kContrastRatio);
  return SkColorSetA(separator_color, result.alpha);
}

// static
bool ThemeHelper::UseIncognitoColor(int id,
                                    const CustomThemeSupplier* theme_supplier) {
  // Incognito is disabled for any non-ignored custom theme colors so they apply
  // atop a predictable state.
  return ShouldIgnoreThemeSupplier(id, true, theme_supplier) ||
         (!IsCustomTheme(theme_supplier) &&
          (!theme_supplier || theme_supplier->CanUseIncognitoColors()));
}

// static
bool ThemeHelper::UseDarkModeColors(const CustomThemeSupplier* theme_supplier) {
  // Dark mode is disabled for custom themes so they apply atop a predictable
  // state.
  return !IsCustomTheme(theme_supplier) &&
         ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors();
}

// static
bool ThemeHelper::ShouldIgnoreThemeSupplier(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) {
  // The incognito NTP uses the default background color instead of any theme
  // background color, unless the theme also sets a custom background image.
  return incognito && (id == TP::COLOR_NTP_BACKGROUND) &&
         !HasCustomImage(IDR_THEME_NTP_BACKGROUND, theme_supplier);
}

gfx::Image ThemeHelper::GetImageNamed(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int adjusted_id = id;
  if (incognito) {
    if (id == IDR_THEME_FRAME)
      adjusted_id = IDR_THEME_FRAME_INCOGNITO;
    else if (id == IDR_THEME_FRAME_INACTIVE)
      adjusted_id = IDR_THEME_FRAME_INCOGNITO_INACTIVE;
  }

  gfx::Image image;
  if (theme_supplier)
    image = theme_supplier->GetImageNamed(adjusted_id);

  if (image.IsEmpty()) {
    image = ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
        adjusted_id);
  }

  return image;
}

base::Optional<SkColor> ThemeHelper::GetOmniboxColor(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier,
    bool* has_custom_color) const {
  // Avoid infinite loop caused by GetColor(TP::COLOR_TOOLBAR) call in
  // GetOmniboxColorImpl().
  if (id == TP::COLOR_TOOLBAR)
    return base::nullopt;

  const auto color = GetOmniboxColorImpl(id, incognito, theme_supplier);
  if (!color)
    return base::nullopt;
  if (has_custom_color)
    *has_custom_color = color.value().custom;
  return color.value().value;
}

base::Optional<ThemeHelper::OmniboxColor> ThemeHelper::GetOmniboxColorImpl(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  // Some utilities from color_utils are reimplemented throughout this function
  // to plumb the custom bit through.
  const auto get_resulting_paint_color = [&](OmniboxColor fg, OmniboxColor bg) {
    return OmniboxColor{color_utils::GetResultingPaintColor(fg.value, bg.value),
                        fg.custom || bg.custom};
  };
  const auto get_base_color = [&](int id) -> OmniboxColor {
    SkColor color;
    if (theme_supplier && theme_supplier->GetColor(id, &color))
      return {color, true};
    return {GetDefaultColor(id, incognito, theme_supplier), false};
  };

  // Compute the two base colors, |bg| and |fg|.
  OmniboxColor bg = get_resulting_paint_color(
      get_base_color(TP::COLOR_OMNIBOX_BACKGROUND),
      {GetColor(TP::COLOR_TOOLBAR, incognito, nullptr), false});
  // Returning early here avoids an infinite loop in computing |fg|, caused by
  // the default color for COLOR_OMNIBOX_TEXT being based on
  // COLOR_OMNIBOX_BACKGROUND.
  if (id == TP::COLOR_OMNIBOX_BACKGROUND)
    return bg;
  OmniboxColor fg =
      get_resulting_paint_color(get_base_color(TP::COLOR_OMNIBOX_TEXT), bg);

  // Certain output cases are based on inverted bg/fg.
  const bool high_contrast =
      theme_supplier && theme_supplier->get_theme_type() ==
                            CustomThemeSupplier::ThemeType::INCREASED_CONTRAST;
  const bool invert =
      high_contrast && (id == TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED ||
                        id == TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED ||
                        id == TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED ||
                        id == TP::COLOR_OMNIBOX_RESULTS_ICON_SELECTED ||
                        id == TP::COLOR_OMNIBOX_RESULTS_URL_SELECTED);
  const auto get_color_with_max_contrast = [](OmniboxColor color) {
    return OmniboxColor{color_utils::GetColorWithMaxContrast(color.value),
                        color.custom};
  };
  const auto blend_for_min_contrast =
      [&](OmniboxColor fg, OmniboxColor bg,
          base::Optional<OmniboxColor> hc_fg = base::nullopt,
          base::Optional<float> contrast_ratio = base::nullopt) {
        base::Optional<SkColor> hc_fg_arg;
        bool custom = fg.custom || bg.custom;
        if (hc_fg) {
          hc_fg_arg = hc_fg.value().value;
          custom |= hc_fg.value().custom;
        }
        // If high contrast is on, increase the minimum contrast ratio.
        // TODO(pkasting): Ideally we could do this in the base
        // BlendForMinContrast() function.
        const float ratio = contrast_ratio.value_or(
            high_contrast ? 6.0f : color_utils::kMinimumReadableContrastRatio);
        return OmniboxColor{color_utils::BlendForMinContrast(fg.value, bg.value,
                                                             hc_fg_arg, ratio)
                                .color,
                            custom};
      };
  if (invert) {
    // Given a color with some contrast against the opposite endpoint, returns a
    // color with that same contrast against the nearby endpoint.
    auto invert_color = [&](OmniboxColor fg) {
      const auto bg = get_color_with_max_contrast(fg);
      const auto inverted_bg = get_color_with_max_contrast(bg);
      const float contrast = color_utils::GetContrastRatio(fg.value, bg.value);
      return blend_for_min_contrast(fg, inverted_bg, base::nullopt, contrast);
    };
    fg = invert_color(fg);
    bg = invert_color(bg);
  }
  const bool dark = color_utils::IsDark(bg.value);

  // All remaining colors can be built atop the two base colors.
  const auto derive_default_icon_color = [](OmniboxColor color) {
    return OmniboxColor{color_utils::DeriveDefaultIconColor(color.value),
                        color.custom};
  };
  const auto blend_toward_max_contrast = [](OmniboxColor color, SkAlpha alpha) {
    return OmniboxColor{color_utils::BlendTowardMaxContrast(color.value, alpha),
                        color.custom};
  };
  const auto results_bg_color = [&]() {
    return get_color_with_max_contrast(fg);
  };
  const auto bg_hovered_color = [&]() {
    return blend_toward_max_contrast(bg, 0x0A);
  };
  const auto results_bg_hovered_color = [&]() {
    return blend_toward_max_contrast(results_bg_color(), 0x1A);
  };
  const auto url_color = [&](OmniboxColor bg) {
    return blend_for_min_contrast(
        {gfx::kGoogleBlue500, false}, bg,
        {{dark ? gfx::kGoogleBlue050 : gfx::kGoogleBlue900, false}});
  };
  const auto results_bg_selected_color = [&]() {
    return blend_toward_max_contrast(results_bg_color(), 0x29);
  };
  const auto blend_with_clamped_contrast = [&](OmniboxColor bg) {
    return blend_for_min_contrast(fg, fg, blend_for_min_contrast(bg, bg));
  };
  switch (id) {
    case TP::COLOR_OMNIBOX_TEXT:
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED:
      return fg;
    case TP::COLOR_OMNIBOX_BACKGROUND_HOVERED:
      return bg_hovered_color();
    case TP::COLOR_OMNIBOX_RESULTS_BG:
      return results_bg_color();
    case TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED:
      return results_bg_selected_color();
    case TP::COLOR_OMNIBOX_BUBBLE_OUTLINE:
      return {
          {dark ? gfx::kGoogleGrey100 : SkColorSetA(gfx::kGoogleGrey900, 0x24),
           false}};
    case TP::COLOR_OMNIBOX_TEXT_DIMMED:
      return blend_with_clamped_contrast(bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED:
      return blend_with_clamped_contrast(results_bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED:
      return blend_with_clamped_contrast(results_bg_selected_color());
    case TP::COLOR_OMNIBOX_RESULTS_ICON:
      return blend_for_min_contrast(derive_default_icon_color(fg),
                                    results_bg_color());
    case TP::COLOR_OMNIBOX_RESULTS_ICON_SELECTED:
      return blend_for_min_contrast(derive_default_icon_color(fg),
                                    results_bg_selected_color());
    case TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED:
      return results_bg_hovered_color();
    case TP::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE:
    case TP::COLOR_OMNIBOX_SELECTED_KEYWORD:
      if (dark)
        return {{gfx::kGoogleGrey100, false}};
      FALLTHROUGH;
    case TP::COLOR_OMNIBOX_RESULTS_URL:
      return url_color(results_bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_URL_SELECTED:
      return url_color(results_bg_selected_color());
    case TP::COLOR_OMNIBOX_RESULTS_BUTTON_BORDER:
      return blend_toward_max_contrast(bg, gfx::kGoogleGreyAlpha400);
    case TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT:
    case TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE:
    case TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS: {
      return {
          {ui::GetSecurityChipColor(GetSecurityChipColorId(id), fg.value,
                                    bg_hovered_color().value, high_contrast),
           fg.custom || (!dark && bg.custom)}};
    }
    default:
      return base::nullopt;
  }
}

SkColor ThemeHelper::GetTabGroupColor(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  // Deal with tab group colors in the tabstrip.
  if (id <= TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_CYAN) {
    int tab_color_id = id < TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREY
                           ? TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE
                           : TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE;

    // TODO(tluk) Change to ensure consistent color ids between WebUI and views
    // tabstrip groups. Currently WebUI tabstrip groups always check active
    // frame colors (https://crbug.com/1060398).
    if (base::FeatureList::IsEnabled(features::kWebUITabStrip))
      tab_color_id = TP::COLOR_FRAME_ACTIVE;

    return GetTabGroupColors(id)[color_utils::IsDark(
        GetColor(tab_color_id, incognito, theme_supplier))];
  }

  // Deal with the rest of the tab group colors.
  return GetTabGroupColors(id)[UseDarkModeColors(theme_supplier)];
}
