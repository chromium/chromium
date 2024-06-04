// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_theme_metrics_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/metrics/histogram_functions.h"
#include "ui/gfx/color_palette.h"

PersonalizationAppThemeMetricsProvider::
    PersonalizationAppThemeMetricsProvider() = default;
PersonalizationAppThemeMetricsProvider::
    ~PersonalizationAppThemeMetricsProvider() = default;

namespace {
constexpr SkColor kLightPink =
    SkColorSetA(SkColor(ash::personalization_app::mojom::kStaticColorLightPink),
                SK_AlphaOPAQUE);

constexpr SkColor kDarkGreen =
    SkColorSetA(SkColor(ash::personalization_app::mojom::kStaticColorDarkGreen),
                SK_AlphaOPAQUE);

constexpr SkColor kLightPurple = SkColorSetA(
    SkColor(ash::personalization_app::mojom::kStaticColorLightPurple),
    SK_AlphaOPAQUE);

constexpr auto kSkColorToStaticColor =
    base::MakeFixedFlatMap<SkColor,
                           ash::personalization_app::mojom::StaticColor>({
        {gfx::kGoogleBlue500,
         ash::personalization_app::mojom::StaticColor::kGoogleBlue},
        {kLightPink, ash::personalization_app::mojom::StaticColor::kLightPink},
        {kDarkGreen, ash::personalization_app::mojom::StaticColor::kDarkGreen},
        {kLightPurple,
         ash::personalization_app::mojom::StaticColor::kLightPurple},
    });
}  // namespace

bool PersonalizationAppThemeMetricsProvider::ProvideHistograms() {
  if (!ash::Shell::HasInstance()) {
    return false;
  }

  const std::optional<ash::ColorPaletteSeed> optional_color_palette_seed =
      ash::Shell::Get()->color_palette_controller()->GetCurrentSeed();
  if (!optional_color_palette_seed.has_value()) {
    return false;
  }

  const ash::ColorPaletteSeed color_palette_seed =
      optional_color_palette_seed.value();
  if (color_palette_seed.scheme == ash::style::mojom::ColorScheme::kStatic) {
    const SkColor seed_color = color_palette_seed.seed_color;
    const ash::personalization_app::mojom::StaticColor static_color =
        kSkColorToStaticColor.contains(seed_color)
            ? kSkColorToStaticColor.at(seed_color)
            : ash::personalization_app::mojom::StaticColor::kUnknown;
    base::UmaHistogramEnumeration(
        "Ash.Personalization.DynamicColor.StaticColor.Settled", static_color);
  } else {
    base::UmaHistogramEnumeration(
        "Ash.Personalization.DynamicColor.ColorScheme.Settled",
        color_palette_seed.scheme);
  }
  const auto* session =
      ash::Shell::Get()->session_controller()->GetUserSession(/*index=*/0);
  if (!session) {
    return false;
  }
  const AccountId& account_id = session->user_info.account_id;
  const bool use_k_means =
      ash::Shell::Get()->color_palette_controller()->GetUseKMeansPref(
          account_id);
  base::UmaHistogramBoolean("Ash.Style.ColorPalette.KMeansAlgorithm",
                            use_k_means);
  return true;
}
