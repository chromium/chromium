// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_keyboard_backlight_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/rgb_keyboard/rgb_keyboard_util.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {
namespace personalization_app {

PersonalizationAppKeyboardBacklightProviderImpl::
    PersonalizationAppKeyboardBacklightProviderImpl(content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {}

PersonalizationAppKeyboardBacklightProviderImpl::
    ~PersonalizationAppKeyboardBacklightProviderImpl() = default;

void PersonalizationAppKeyboardBacklightProviderImpl::BindInterface(
    mojo::PendingReceiver<
        ash::personalization_app::mojom::KeyboardBacklightProvider> receiver) {
  keyboard_backlight_receiver_.reset();
  keyboard_backlight_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppKeyboardBacklightProviderImpl::SetBacklightColor(
    mojom::BacklightColor backlight_color) {
  if (!ash::features::IsRgbKeyboardEnabled()) {
    mojo::ReportBadMessage(
        "Cannot call `SetBacklightColor()` without rgb keyboard enabled");
    return;
  }
  DVLOG(4) << __func__ << " backlight_color=" << backlight_color;
  auto* rgb_keyboard_manager = ash::Shell::Get()->rgb_keyboard_manager();
  DCHECK(rgb_keyboard_manager);
  SkColor color = kInvalidColor;
  switch (backlight_color) {
    case mojom::BacklightColor::kWallpaper:
      // TODO(b/224871280): Add support to set keyboard color to wallpaper
      // extracted color.
      break;
    case mojom::BacklightColor::kWhite:
    case mojom::BacklightColor::kRed:
    case mojom::BacklightColor::kYellow:
    case mojom::BacklightColor::kGreen:
    case mojom::BacklightColor::kBlue:
    case mojom::BacklightColor::kIndigo:
    case mojom::BacklightColor::kPurple: {
      color = ConvertBacklightColorToSkColor(backlight_color);
      rgb_keyboard_manager->SetStaticBackgroundColor(
          SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
      break;
    }
    case mojom::BacklightColor::kRainbow:
      rgb_keyboard_manager->SetRainbowMode();
      break;
  }

  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  pref_service->SetInteger(ash::prefs::kPersonalizationKeyboardBacklightColor,
                           static_cast<int>(backlight_color));
}

}  // namespace personalization_app
}  // namespace ash
