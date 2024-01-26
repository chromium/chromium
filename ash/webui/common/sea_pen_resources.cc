// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/sea_pen_resources.h"

#include "ash/constants/ash_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::common {

void AddSeaPenStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"seaPenExclusiveWallpapersHeading",
       IDS_PERSONALIZATION_APP_EXCLUSIVE_WALLPAPERS_HEADING},
      {"seaPenChooseAWallpaperHeading",
       IDS_PERSONALIZATION_APP_CHOOSE_A_WALLPAPER_HEADING},
      {"seaPenLabel", IDS_SEA_PEN_LABEL},
      {"seaPenPoweredByGoogle", IDS_SEA_PEN_POWERED_BY_GOOGLE},
      {"seaPenTemplateHeading", IDS_SEA_PEN_TEMPLATE_HEADING},
      {"seaPenRecentWallpapersHeading", IDS_SEA_PEN_RECENT_WALLPAPERS_HEADING},
      {"seaPenInspireMeButton", IDS_SEA_PEN_INSPIRE_ME_BUTTON},
      {"seaPenCreateButton", IDS_SEA_PEN_CREATE_BUTTON},
      {"seaPenRecreateButton", IDS_SEA_PEN_RECREATE_BUTTON},
      {"seaPenWallpaperPoweredByGoogle",
       IDS_SEA_PEN_WALLPAPER_POWERED_BY_GOOGLE},
      {"seaPenDeleteWallpaper", IDS_SEA_PEN_DELETE_WALLPAPER},
      {"seaPenCreateMore", IDS_SEA_PEN_CREATE_MORE},
      {"seaPenAbout", IDS_SEA_PEN_ABOUT},
      {"seaPenErrorNoInternet", IDS_SEA_PEN_ERROR_NO_INTERNET},
      {"seaPenErrorResourceExhausted", IDS_SEA_PEN_ERROR_RESOURCE_EXHAUSTED},
      {"seaPenErrorGeneric", IDS_SEA_PEN_ERROR_GENERIC},
      {"seaPenExperimentLabel", IDS_SEA_PEN_EXPERIMENT_LABEL},
      {"seaPenThumbnailsLoading", IDS_SEA_PEN_THUMBNAILS_LOADING},
      {"seaPenWallpaperTermsDialogTitle",
       IDS_SEA_PEN_WALLPAPER_TERMS_DIALOG_TITLE},
      {"seaPenWallpaperTermsAcceptButton",
       IDS_SEA_PEN_WALLPAPER_TERMS_ACCEPT_BUTTON},
      {"seaPenWallpaperTermsRefuseButton",
       IDS_SEA_PEN_WALLPAPER_TERMS_REFUSE_BUTTON},
      {"seaPenWallpaperTermsOfServiceDesc",
       IDS_SEA_PEN_WALLPAPER_TERMS_OF_SERVICE_DESC},
  };
  source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace ash::common
