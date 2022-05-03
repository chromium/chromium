// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/views/linux_ui/linux_ui.h"
#endif

namespace {

using TP = ThemeProperties;

// The default theme if we've gone to the theme gallery and installed the
// "Default" theme. We have to detect this case specifically. (By the time we
// realize we've installed the default theme, we already have an extension
// unpacked on the filesystem.)
constexpr char kDefaultThemeGalleryID[] = "hkacjpbfdknhflllbcmjibkdeoafencn";

// Returns an array of light and dark mode versions of the given color id
// Ex: [light mode, dark mode]
const std::array<SkColor, 2> GetTabGroupColors(int color_id) {
  // Depending on UI variation enabled, dark mode saved group chip colors are
  // calculated by blending the default dark mode toolbar color with the tab
  // strip group colors at 24% or 48% alpha.
  const SkColor default_dark_toolbar_color =
      TP::GetDefaultColor(TP::COLOR_TOOLBAR, false, true);
  float tab_group_chip_alpha = 0.24f;

  // Flat version of dark mode colors used in bookmarks bar to fill
  // the buttons.
  const SkColor kFlatGrey = SkColorSetRGB(0x5D, 0x5E, 0x62);
  const SkColor kFlatBlue = SkColorSetRGB(0x49, 0x54, 0x68);
  const SkColor kFlatRed = SkColorSetRGB(0x62, 0x4A, 0x4B);
  const SkColor kFlatGreen = SkColorSetRGB(0x47, 0x59, 0x50);
  const SkColor kFlatYellow = SkColorSetRGB(0x65, 0x5C, 0x44);
  const SkColor kFlatCyan = SkColorSetRGB(0x45, 0x5D, 0x65);
  const SkColor kFlatPurple = SkColorSetRGB(0x58, 0x4A, 0x68);
  const SkColor kFlatPink = SkColorSetRGB(0x65, 0x4A, 0x5D);
  const SkColor kFlatOrange = color_utils::AlphaBlend(
      gfx::kGoogleOrange300, default_dark_toolbar_color, tab_group_chip_alpha);

  switch (color_id) {
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_BLUE:
    case TP::COLOR_TAB_GROUP_DIALOG_BLUE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_BLUE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_BLUE:
      return {gfx::kGoogleBlue600, gfx::kGoogleBlue300};
    case TP::COLOR_TAB_GROUP_BOOKMARK_BAR_BLUE:
      return {gfx::kGoogleBlue050, kFlatBlue};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_RED:
    case TP::COLOR_TAB_GROUP_DIALOG_RED:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_RED:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_RED:
      return {gfx::kGoogleRed600, gfx::kGoogleRed300};
    case TP::COLOR_TAB_GROUP_BOOKMARK_BAR_RED:
      return {gfx::kGoogleRed050, kFlatRed};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_YELLOW:
    case TP::COLOR_TAB_GROUP_DIALOG_YELLOW:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_YELLOW:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_YELLOW:
      return {gfx::kGoogleYellow600, gfx::kGoogleYellow300};
    case TP::COLOR_TAB_GROUP_BOOKMARK_BAR_YELLOW:
      return {gfx::kGoogleYellow050, kFlatYellow};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_GREEN:
    case TP::COLOR_TAB_GROUP_DIALOG_GREEN:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREEN:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREEN:
      return {gfx::kGoogleGreen700, gfx::kGoogleGreen300};
    case TP::COLOR_TAB_GROUP_BOOKMARK_BAR_GREEN:
      return {gfx::kGoogleGreen050, kFlatGreen};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_PINK:
    case TP::COLOR_TAB_GROUP_DIALOG_PINK:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PINK:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PINK:
      return {gfx::kGooglePink700, gfx::kGooglePink300};
    case TP::COLOR_TAB_GROUP_BOOKMARK_BAR_PINK:
      return {gfx::kGooglePink050, kFlatPink};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_PURPLE:
    case TP::COLOR_TAB_GROUP_DIALOG_PURPLE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PURPLE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PURPLE:
      return {gfx::kGooglePurple500, gfx::kGooglePurple300};
    case TP::COLOR_TAB_GROUP_BOOKMARK_BAR_PURPLE:
      return {gfx::kGooglePurple050, kFlatPurple};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_CYAN:
    case TP::COLOR_TAB_GROUP_DIALOG_CYAN:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_CYAN:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_CYAN:
      return {gfx::kGoogleCyan900, gfx::kGoogleCyan300};
    case TP::COLOR_TAB_GROUP_BOOKMARK_BAR_CYAN:
      return {gfx::kGoogleCyan050, kFlatCyan};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_ORANGE:
    case TP::COLOR_TAB_GROUP_DIALOG_ORANGE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_ORANGE:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_ORANGE:
      return {gfx::kGoogleOrange400, gfx::kGoogleOrange300};
    case TP::COLOR_TAB_GROUP_BOOKMARK_BAR_ORANGE:
      return {gfx::kGoogleOrange050, kFlatOrange};
    case TP::COLOR_TAB_GROUP_BOOKMARK_BAR_GREY:
      return {gfx::kGoogleGrey100, kFlatGrey};
    case TP::COLOR_TAB_GROUP_CONTEXT_MENU_GREY:
    case TP::COLOR_TAB_GROUP_DIALOG_GREY:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREY:
    case TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREY:
    default:
      return {gfx::kGoogleGrey700, gfx::kGoogleGrey300};
  }
}

SkColor IncreaseLightness(SkColor color, double percent) {
  color_utils::HSL result;
  color_utils::SkColorToHSL(color, &result);
  result.l += (1 - result.l) * percent;
  return color_utils::HSLToSkColor(result, SkColorGetA(color));
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
    ui::ResourceScaleFactor scale_factor) {
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
            id, ui::k100Percent);
  }

  return data;
}

ThemeHelper::ThemeHelper() = default;

ThemeHelper::~ThemeHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SkColor ThemeHelper::GetColor(int id,
                              bool incognito,
                              const CustomThemeSupplier* theme_supplier) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (theme_supplier && !incognito) {
    SkColor color;
    if (theme_supplier->GetColor(id, &color))
      return color;
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

  return TP::GetDefaultTint(id, incognito, UseDarkModeColors(theme_supplier));
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
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(USE_GTK) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Linux the GTK system theme provides the high contrast colors,
  // so don't use the IncreasedContrastThemeSupplier.
  return false;
#else
  return native_theme && native_theme->UserHasContrastPreference();
#endif
}

SkColor ThemeHelper::GetDefaultColor(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  if (TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREY <= id &&
      id <= TP::MAX_COLOR_BOOKMARK_BAR)
    return GetTabGroupColor(id, incognito, theme_supplier);

  const absl::optional<SkColor> omnibox_color =
      GetOmniboxColor(id, incognito, theme_supplier);
  if (omnibox_color.has_value())
    return omnibox_color.value();

  // For backward compat with older themes, some newer colors are generated from
  // older ones if they are missing.
  const auto get_frame_color = [this, incognito, theme_supplier](bool active) {
    return GetColor(active ? TP::COLOR_FRAME_ACTIVE : TP::COLOR_FRAME_INACTIVE,
                    incognito, theme_supplier);
  };
  switch (id) {
    case TP::COLOR_BOOKMARK_BAR_BACKGROUND:
      return GetColor(TP::COLOR_TOOLBAR, incognito, theme_supplier);
    case TP::COLOR_BOOKMARK_FAVICON: {
      SkColor color;
      return (theme_supplier &&
              theme_supplier->GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, &color))
                 ? color
                 : SK_ColorTRANSPARENT;
    }
    case TP::COLOR_FLYING_INDICATOR_BACKGROUND:
      return GetColor(TP::COLOR_TOOLBAR, incognito, theme_supplier);
    case TP::COLOR_FLYING_INDICATOR_FOREGROUND:
      return GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, incognito, theme_supplier);
    case TP::COLOR_FRAME_CAPTION_ACTIVE:
    case TP::COLOR_FRAME_CAPTION_INACTIVE:
      return color_utils::GetColorWithMaxContrast(GetColor(
          id == TP::COLOR_FRAME_CAPTION_ACTIVE ? TP::COLOR_FRAME_ACTIVE
                                               : TP::COLOR_FRAME_INACTIVE,
          incognito, theme_supplier));
    case TP::COLOR_BOOKMARK_TEXT:
    case TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE:
    case TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE:
      return GetColor(TP::COLOR_TOOLBAR_TEXT, incognito, theme_supplier);
    case TP::COLOR_TAB_STROKE_FRAME_ACTIVE:
      return GetColor(TP::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE, incognito,
                      theme_supplier);
    case TP::COLOR_TAB_STROKE_FRAME_INACTIVE:
      return GetColor(TP::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE, incognito,
                      theme_supplier);
    case TP::COLOR_DOWNLOAD_SHELF_CONTENT_AREA_SEPARATOR:
      return color_utils::AlphaBlend(
          GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, incognito, theme_supplier),
          GetColor(TP::COLOR_DOWNLOAD_SHELF, incognito, theme_supplier),
          SkAlpha{0x3A});
    case TP::COLOR_STATUS_BUBBLE_ACTIVE:
      return GetColor(TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE, incognito,
                      theme_supplier);
    case TP::COLOR_STATUS_BUBBLE_INACTIVE:
      return GetColor(TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE,
                      incognito, theme_supplier);
    case TP::COLOR_STATUS_BUBBLE_TEXT_ACTIVE:
      return GetColor(TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE, incognito,
                      theme_supplier);
    case TP::COLOR_STATUS_BUBBLE_TEXT_INACTIVE:
      return GetColor(TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE,
                      incognito, theme_supplier);
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
          gfx::kGoogleGrey700,
          GetTint(TP::TINT_BUTTONS, incognito, theme_supplier));
    case TP::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE:
      // The active color is overridden in GtkUi.
      return SkColorSetA(
          GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, incognito, theme_supplier),
          gfx::kGoogleGreyAlpha500);
    case TP::COLOR_LOCATION_BAR_BORDER:
      return SkColorSetA(SK_ColorBLACK, 0x4D);
    case TP::COLOR_LOCATION_BAR_BORDER_OPAQUE:
      return color_utils::GetResultingPaintColor(
          GetColor(TP::COLOR_LOCATION_BAR_BORDER, incognito, theme_supplier),
          GetColor(TP::COLOR_TOOLBAR, incognito, theme_supplier));
    case TP::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE:
    case TP::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE: {
      const SkColor toolbar_color =
          GetColor(TP::COLOR_TOOLBAR, incognito, theme_supplier);
      const SkColor frame_color = get_frame_color(
          /*active=*/id == TP::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE);
      const SeparatorColorKey key(toolbar_color, frame_color);
      auto i = GetSeparatorColorCache().find(key);
      if (i != GetSeparatorColorCache().end())
        return i->second;
      const SkColor separator_color =
          GetToolbarTopSeparatorColor(toolbar_color, frame_color);
      GetSeparatorColorCache()[key] = separator_color;
      return separator_color;
    }
    case TP::COLOR_BOOKMARK_SEPARATOR:
    case TP::COLOR_TOOLBAR_VERTICAL_SEPARATOR:
      return SkColorSetA(
          GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, incognito, theme_supplier),
          0x4D);
    case TP::COLOR_TOOLBAR_BUTTON_TEXT:
      // TODO(crbug.com/967317): Update to match mocks, i.e. return
      // gfx::kGoogleGrey900, if needed.
      [[fallthrough]];
    case TP::COLOR_TOOLBAR_INK_DROP:
      return color_utils::GetColorWithMaxContrast(
          GetColor(TP::COLOR_TOOLBAR, incognito, theme_supplier));
    case TP::COLOR_TOOLBAR_BUTTON_BORDER:
      return SkColorSetA(
          GetColor(TP::COLOR_TOOLBAR_INK_DROP, incognito, theme_supplier),
          0x20);
    case TP::COLOR_INFOBAR_CONTENT_AREA_SEPARATOR:
      return color_utils::AlphaBlend(
          GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, incognito, theme_supplier),
          GetColor(TP::COLOR_INFOBAR, incognito, theme_supplier),
          SkAlpha{0x3A});
    case TP::COLOR_SIDE_PANEL_CONTENT_AREA_SEPARATOR:
    case TP::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR:
      return color_utils::AlphaBlend(
          GetColor(TP::COLOR_TOOLBAR_BUTTON_ICON, incognito, theme_supplier),
          GetColor(TP::COLOR_TOOLBAR, incognito, theme_supplier),
          SkAlpha{0x3A});
    case TP::COLOR_NTP_SECTION_BORDER:
      return SkColorSetA(
          GetColor(TP::COLOR_NTP_HEADER, incognito, theme_supplier), 0x50);
    case TP::COLOR_NTP_TEXT_LIGHT:
      return IncreaseLightness(
          GetColor(TP::COLOR_NTP_TEXT, incognito, theme_supplier), 0.40);
    case TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE:
    case TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE:
      return GetColor(id == TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE
                          ? TP::COLOR_FRAME_ACTIVE
                          : TP::COLOR_FRAME_INACTIVE,
                      incognito, theme_supplier);
  }

  return TP::GetDefaultColor(id, incognito, UseDarkModeColors(theme_supplier));
}

// static
bool ThemeHelper::UseDarkModeColors(const CustomThemeSupplier* theme_supplier) {
  // Dark mode is disabled for custom themes so they apply atop a predictable
  // state.
  if (IsCustomTheme(theme_supplier))
    return false;

  ui::NativeTheme const* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
#if BUILDFLAG(IS_LINUX)
  if (const auto* linux_ui = views::LinuxUI::instance()) {
    // We rely on the fact that the system theme is in use iff `theme_supplier`
    // is non-null, but this is cheating. In the future this might not hold
    // after we fully migrate to the color provider and remove SystemThemeLinux.
    native_theme = linux_ui->GetNativeTheme(
        theme_supplier &&
        theme_supplier->get_theme_type() == CustomThemeSupplier::NATIVE_X11);
  }
#endif
  return native_theme->ShouldUseDarkColors();
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

absl::optional<SkColor> ThemeHelper::GetOmniboxColor(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  // The base colors are not computed; avoid infinite recursion.  COLOR_TOOLBAR
  // must also be excluded since it's used in the base definition of
  // COLOR_OMNIBOX_BACKGROUND.
  if (id == TP::COLOR_OMNIBOX_BACKGROUND || id == TP::COLOR_OMNIBOX_TEXT ||
      id == TP::COLOR_TOOLBAR) {
    return absl::nullopt;
  }

  // Compute the two base colors, |bg| and |fg|.
  SkColor bg =
      GetColor(TP::COLOR_OMNIBOX_BACKGROUND, incognito, theme_supplier);
  SkColor fg = GetColor(TP::COLOR_OMNIBOX_TEXT, incognito, theme_supplier);

  // Certain output cases are based on inverted bg/fg.
  const bool high_contrast =
      theme_supplier && theme_supplier->get_theme_type() ==
                            CustomThemeSupplier::ThemeType::INCREASED_CONTRAST;
  const bool invert =
      high_contrast &&
      (id == TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED ||
       id == TP::COLOR_OMNIBOX_RESULTS_BUTTON_INK_DROP_SELECTED ||
       id == TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED ||
       id == TP::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE_SELECTED ||
       id == TP::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE_SELECTED ||
       id == TP::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY_SELECTED ||
       id == TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED ||
       id == TP::COLOR_OMNIBOX_RESULTS_ICON_SELECTED ||
       id == TP::COLOR_OMNIBOX_RESULTS_URL_SELECTED);
  const auto blend_for_min_contrast =
      [&](SkColor fg, SkColor bg, absl::optional<SkColor> hc_fg = absl::nullopt,
          absl::optional<float> contrast_ratio = absl::nullopt) {
        // If high contrast is on, increase the minimum contrast ratio.
        // TODO(pkasting): Ideally we could do this in the base
        // BlendForMinContrast() function.
        const float ratio = contrast_ratio.value_or(
            high_contrast ? 6.0f : color_utils::kMinimumReadableContrastRatio);
        return color_utils::BlendForMinContrast(fg, bg, hc_fg, ratio).color;
      };
  if (invert) {
    // Given a color with some contrast against the opposite endpoint, returns a
    // color with that same contrast against the nearby endpoint.
    auto invert_color = [&](SkColor fg) {
      const auto bg = color_utils::GetColorWithMaxContrast(fg);
      const auto inverted_bg = color_utils::GetColorWithMaxContrast(bg);
      const float contrast = color_utils::GetContrastRatio(fg, bg);
      return blend_for_min_contrast(fg, inverted_bg, absl::nullopt, contrast);
    };
    fg = invert_color(fg);
    bg = invert_color(bg);
  }
  const bool dark = color_utils::IsDark(bg);

  // All remaining colors can be built atop the two base colors.
  const auto results_bg_color = [&]() {
    return color_utils::GetColorWithMaxContrast(fg);
  };
  const auto bg_hovered_color = [&]() {
    return color_utils::BlendTowardMaxContrast(bg, 0x0A);
  };
  const auto results_bg_hovered_color = [&]() {
    return color_utils::BlendTowardMaxContrast(results_bg_color(), 0x1A);
  };
  const auto negative_text_color = [&](SkColor bg) {
    return blend_for_min_contrast(
        dark ? gfx::kGoogleRed300 : gfx::kGoogleRed600, bg);
  };
  const auto positive_text_color = [&](SkColor bg) {
    return blend_for_min_contrast(
        dark ? gfx::kGoogleGreen300 : gfx::kGoogleGreen700, bg);
  };
  const auto secondary_text_color = [&](SkColor bg) {
    return blend_for_min_contrast(
        // In the color pipeline world, this color is kColorDisabledForeground.
        blend_for_min_contrast(
            gfx::kGoogleGrey600,
            dark ? SkColorSetRGB(0x29, 0x2A, 0x2D) : SK_ColorWHITE,
            dark ? gfx::kGoogleGrey200 : gfx::kGoogleGrey900),
        bg);
  };
  const auto url_color = [&](SkColor bg) {
    return blend_for_min_contrast(
        gfx::kGoogleBlue500, bg,
        dark ? gfx::kGoogleBlue050 : gfx::kGoogleBlue900);
  };
  const auto results_bg_selected_color = [&]() {
    return color_utils::BlendTowardMaxContrast(results_bg_color(), 0x1A);
  };
  const auto blend_with_clamped_contrast = [&](SkColor bg) {
    return blend_for_min_contrast(fg, fg, blend_for_min_contrast(bg, bg));
  };
  switch (id) {
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED:
      return fg;
    case TP::COLOR_OMNIBOX_BACKGROUND_HOVERED:
      return bg_hovered_color();
    case TP::COLOR_OMNIBOX_RESULTS_BG:
      return results_bg_color();
    case TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED:
      return results_bg_selected_color();
    case TP::COLOR_OMNIBOX_BUBBLE_OUTLINE:
      return dark ? gfx::kGoogleGrey100
                  : SkColorSetA(gfx::kGoogleGrey900, 0x24);
    case TP::COLOR_OMNIBOX_TEXT_DIMMED:
      return blend_with_clamped_contrast(bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED:
      return blend_with_clamped_contrast(results_bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED:
      return blend_with_clamped_contrast(results_bg_selected_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE:
      return negative_text_color(results_bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE_SELECTED:
      return negative_text_color(results_bg_selected_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE:
      return positive_text_color(results_bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE_SELECTED:
      return positive_text_color(results_bg_selected_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY:
      return secondary_text_color(results_bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY_SELECTED:
      return secondary_text_color(results_bg_selected_color());
    case TP::COLOR_OMNIBOX_RESULTS_ICON:
      return blend_for_min_contrast(color_utils::DeriveDefaultIconColor(fg),
                                    results_bg_color());
    case TP::COLOR_OMNIBOX_RESULTS_ICON_SELECTED:
      return blend_for_min_contrast(color_utils::DeriveDefaultIconColor(fg),
                                    results_bg_selected_color());
    case TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED:
      return results_bg_hovered_color();
    case TP::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE:
    case TP::COLOR_OMNIBOX_SELECTED_KEYWORD:
      if (dark)
        return gfx::kGoogleGrey100;
      [[fallthrough]];
    case TP::COLOR_OMNIBOX_RESULTS_URL:
      return url_color(results_bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_URL_SELECTED:
      return url_color(results_bg_selected_color());
    case TP::COLOR_OMNIBOX_RESULTS_BUTTON_BORDER:
      return color_utils::BlendTowardMaxContrast(bg, gfx::kGoogleGreyAlpha400);
    case TP::COLOR_OMNIBOX_RESULTS_BUTTON_INK_DROP:
      return color_utils::GetColorWithMaxContrast(results_bg_hovered_color());
    case TP::COLOR_OMNIBOX_RESULTS_BUTTON_INK_DROP_SELECTED:
      return color_utils::GetColorWithMaxContrast(results_bg_selected_color());
    case TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT:
    case TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE:
      return blend_for_min_contrast(
          dark ? gfx::kGoogleGrey500 : gfx::kGoogleGrey700, bg_hovered_color());
    case TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS:
      return blend_for_min_contrast(
          dark ? gfx::kGoogleRed300 : gfx::kGoogleRed600, bg_hovered_color());
    default:
      return absl::nullopt;
  }
}

SkColor ThemeHelper::GetTabGroupColor(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  // Deal with tab group colors in the tabstrip.
  if (id <= TP::MAX_COLOR_TABSTRIP_INACTIVE) {
    int tab_color_id = id < TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREY
                           ? TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE
                           : TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE;

    return GetTabGroupColors(id)[color_utils::IsDark(
        GetColor(tab_color_id, incognito, theme_supplier))];
  }

  // Deal with the rest of the tab group colors.
  bool use_dark_mode_colors;
  if (id >= TP::COLOR_TAB_GROUP_DIALOG_GREY &&
      id <= TP::MAX_COLOR_BOOKMARK_BAR) {
    // To support custom themes, assume that the dark mode palette is more
    // appropriate for bookmark chips, tab group dialog bubble, and context sub
    // menu when the bookmark bar appears to be light text on dark bookmark bar.
    use_dark_mode_colors = !color_utils::IsDark(
        GetColor(TP::COLOR_BOOKMARK_TEXT, incognito, theme_supplier));
  } else {
    use_dark_mode_colors = UseDarkModeColors(theme_supplier);
  }
  return GetTabGroupColors(id)[incognito || use_dark_mode_colors];
}
