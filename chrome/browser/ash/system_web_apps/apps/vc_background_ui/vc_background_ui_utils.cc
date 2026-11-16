// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_utils.h"

#include <memory>
#include <utility>

#include "ash/webui/vc_background_ui/vc_background_ui.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_sea_pen_provider_impl.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash::vc_background_ui {

std::unique_ptr<content::WebUIController> CreateVcBackgroundUI(
    const ApplicationLocaleStorage* application_locale_storage,
    const variations::VariationsService& variations_service,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    content::WebUI* web_ui,
    const GURL& url,
    manta::MantaService* manta_service) {
  auto sea_pen_provider = std::make_unique<VcBackgroundUISeaPenProviderImpl>(
      web_ui,
      std::make_unique<wallpaper_handlers::WallpaperFetcherDelegateImpl>(
          application_locale_storage, std::move(shared_url_loader_factory)),
      manta_service);
  return std::make_unique<VcBackgroundUI>(
      CHECK_DEREF(application_locale_storage).GetTag(), variations_service,
      web_ui, std::move(sea_pen_provider));
}

}  // namespace ash::vc_background_ui
