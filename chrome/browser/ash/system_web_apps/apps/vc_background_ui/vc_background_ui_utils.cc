// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_utils.h"

#include <memory>

#include "ash/webui/vc_background_ui/vc_background_ui.h"
#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_sea_pen_provider_impl.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "url/gurl.h"

namespace ash::vc_background_ui {

std::unique_ptr<content::WebUIController> CreateVcBackgroundUI(
    content::WebUI* web_ui,
    const GURL& url) {
  auto sea_pen_provider = std::make_unique<VcBackgroundUISeaPenProviderImpl>(
      web_ui,
      std::make_unique<wallpaper_handlers::WallpaperFetcherDelegateImpl>());
  return std::make_unique<ash::vc_background_ui::VcBackgroundUI>(
      web_ui, std::move(sea_pen_provider));
}

}  // namespace ash::vc_background_ui
