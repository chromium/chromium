// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_app_icon_loader.h"

#include "base/stl_util.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"

namespace extensions {

namespace {

const Extension* GetExtensionByID(Profile* profile, const std::string& id) {
  ExtensionService* service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return nullptr;
  return service->GetInstalledExtension(id);
}

}  // namespace

ChromeAppIconLoader::ChromeAppIconLoader(Profile* profile,
                                         int icon_size_in_dips,
                                         const ResizeFunction& resize_function,
                                         AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, icon_size_in_dips, delegate),
      resize_function_(resize_function) {}

ChromeAppIconLoader::ChromeAppIconLoader(Profile* profile,
                                         int icon_size_in_dips,
                                         AppIconLoaderDelegate* delegate)
    : ChromeAppIconLoader(profile,
                          icon_size_in_dips,
                          ResizeFunction(),
                          delegate) {}

ChromeAppIconLoader::~ChromeAppIconLoader() {}

bool ChromeAppIconLoader::CanLoadImageForApp(const std::string& id) {
  if (map_.find(id) != map_.end())
    return true;
  return GetExtensionByID(profile(), id) != nullptr;
}

void ChromeAppIconLoader::FetchImage(const std::string& id) {
  if (map_.find(id) != map_.end())
    return;  // Already loading the image.

  const Extension* extension = GetExtensionByID(profile(), id);
  if (!extension)
    return;

  std::unique_ptr<ChromeAppIcon> icon =
      ChromeAppIconService::Get(profile())->CreateIcon(this, id, icon_size(),
                                                       resize_function_);
  // Triggers image loading now instead of depending on paint message. This
  // makes the temp blank image be shown for shorter time and improves user
  // experience. See http://crbug.com/146114.
  icon->image_skia().EnsureRepsForSupportedScales();
  map_[id] = std::move(icon);
}

void ChromeAppIconLoader::ClearImage(const std::string& id) {
  map_.erase(id);
}

void ChromeAppIconLoader::UpdateImage(const std::string& id) {
  auto it = map_.find(id);
  if (it == map_.end())
    return;

  it->second->UpdateIcon();
}

void ChromeAppIconLoader::OnIconUpdated(ChromeAppIcon* icon) {
  delegate()->OnAppImageUpdated(icon->app_id(), icon->image_skia());
}

}  // namespace extensions
