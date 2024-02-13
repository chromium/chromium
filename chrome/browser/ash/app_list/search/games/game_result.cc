// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/games/game_result.h"

#include <cmath>
#include <string>
#include <string_view>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/common/search_result_util.h"
#include "chrome/grit/app_icon_resources.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace app_list {
namespace {

constexpr char16_t kA11yDelimiter[] = u", ";

constexpr auto kAllowedLaunchAppIds = base::MakeFixedFlatSet<std::string_view>(
    {"egmafekfmcnknbdlbfbhafbllplmjlhn", "pnkcfpnngfokcnnijgkllghjlhkailce"});

void LogIconLoadStatus(apps::DiscoveryError status) {
  base::UmaHistogramEnumeration("Apps.AppList.GameResult.IconLoadStatus",
                                status);
}

// Calculates the side length of the largest square that will fit in a circle of
// the given diameter.
int MaxSquareLengthForRadius(const int radius) {
  const double hypotenuse = sqrt(2.0 * radius * radius);
  return floor(hypotenuse);
}

// Loads the app placeholder icon and treats it as unmaskable.
ui::ImageModel GetAppPlaceholderIcon(int dimension) {
  // TODO(b/306561938): Use 20x20 version of the icon instead.
  const gfx::ImageSkia& image =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_APP_ICON_PLACEHOLDER_CUBE);
  const int radius = dimension / 2;
  const int size = MaxSquareLengthForRadius(radius);
  const gfx::ImageSkia resized_image =
      gfx::ImageSkiaOperations::CreateResizedImage(
          image, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
          gfx::Size(size, size));
  // Sets the background to white to be consistent with the way the game icons
  // are displayed.
  // TODO(b/306561938): Use cros_tokens::kCrosSysSystemOnBase for background
  // here and below instead.
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          radius, SK_ColorWHITE, resized_image));
}
}  // namespace

GameResult::GameResult(Profile* profile,
                       AppListControllerDelegate* list_controller,
                       apps::AppDiscoveryService* app_discovery_service,
                       const apps::Result& game,
                       double relevance,
                       const std::u16string& query)
    : profile_(profile),
      list_controller_(list_controller),
      dimension_(kAppIconDimension) {
  DCHECK(profile);
  DCHECK(list_controller);
  DCHECK(app_discovery_service);
  // GameResult requires that apps::Result has GameExtras populated.
  DCHECK(game.GetSourceExtras());
  DCHECK(game.GetSourceExtras()->AsGameExtras());

  const auto* extras = game.GetSourceExtras()->AsGameExtras();
  launch_url_ = extras->GetDeeplinkUrl();
  is_icon_masking_allowed_ = extras->GetIsIconMaskingAllowed();

  set_id(launch_url_.spec());
  set_relevance(relevance);

  SetMetricsType(ash::GAME_SEARCH);
  SetResultType(ResultType::kGames);
  SetDisplayType(DisplayType::kList);
  SetCategory(Category::kGames);

  UpdateText(game, query);
  SetIcon(IconInfo(GetAppPlaceholderIcon(dimension_), kAppIconDimension,
                   IconShape::kCircle));
  app_discovery_service->GetIcon(
      game.GetIconId(), dimension_, apps::ResultType::kGameSearchCatalog,
      base::BindOnce(&GameResult::OnIconLoaded, weak_factory_.GetWeakPtr()));
  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->AddObserver(this);
}

GameResult::~GameResult() {
  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->RemoveObserver(this);
}

void GameResult::Open(int event_flags) {
  // Launch the app directly if possible.
  std::vector<std::string> app_ids =
      list_controller_->GetAppIdsForUrl(profile_, launch_url_,
                                        /*exclude_browsers=*/true,
                                        /*exclude_browser_tab_apps=*/true);
  for (const auto& app_id : app_ids) {
    if (kAllowedLaunchAppIds.contains(app_id)) {
      list_controller_->LaunchAppWithUrl(profile_, app_id, event_flags,
                                         launch_url_,
                                         apps::LaunchSource::kFromAppListQuery);
      return;
    }
  }

  // If no suitable app was found, launch the URL in the browser.
  list_controller_->OpenURL(profile_, launch_url_, ui::PAGE_TRANSITION_TYPED,
                            ui::DispositionFromEventFlags(event_flags));
}

void GameResult::UpdateText(const apps::Result& game,
                            const std::u16string& query) {
  SetTitle(game.GetAppTitle());

  std::u16string source = game.GetSourceExtras()->AsGameExtras()->GetSource();
  ash::SearchResultTextItem details_text =
      CreateStringTextItem(source).SetOverflowBehavior(
          ash::SearchResultTextItem::OverflowBehavior::kNoElide);
  SetDetailsTextVector({details_text});

  SetAccessibleName(
      base::JoinString({game.GetAppTitle(), source}, kA11yDelimiter));
}

void GameResult::OnIconLoaded(const gfx::ImageSkia& image,
                              apps::DiscoveryError error) {
  LogIconLoadStatus(error);
  if (error != apps::DiscoveryError::kSuccess) {
    // Don't display results that have no icon.
    scoring().set_filtered(true);
    return;
  }

  // All icons must be circles and set on a white background. The white
  // background will only affect images with transparent backgrounds.
  gfx::ImageSkia icon;
  if (is_icon_masking_allowed_) {
    // Create a circle that is large enough to cover the image. Images are
    // expected to be squares of an even dimension.
    const int radius = std::max(image.height(), image.width()) / 2;
    icon = gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
        radius, SK_ColorWHITE, image);
  } else {
    // If icon masking is not allowed, resize the image to fit.
    const int radius = dimension_ / 2;
    const int size = MaxSquareLengthForRadius(radius);
    const gfx::ImageSkia resized_image =
        gfx::ImageSkiaOperations::CreateResizedImage(
            image, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
            gfx::Size(size, size));

    icon = gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
        radius, SK_ColorWHITE, resized_image);
  }
  SetIcon(IconInfo(ui::ImageModel::FromImageSkia(icon), kAppIconDimension,
                   IconShape::kCircle));
}

}  // namespace app_list
