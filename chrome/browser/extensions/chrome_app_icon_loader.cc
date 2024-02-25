// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_app_icon_loader.h"

#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"

namespace extensions {

namespace {

const Extension* GetExtensionByID(Profile* profile, const std::string& id) {
  return ExtensionRegistry::Get(profile)->GetInstalledExtension(id);
}

}  // namespace

ChromeAppIconLoader::ChromeAppIconLoader(Profile* profile,
                                         int icon_size_in_dip,
                                         const ResizeFunction& resize_function,
                                         AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, icon_size_in_dip, delegate),
      resize_function_(resize_function) {}

ChromeAppIconLoader::ChromeAppIconLoader(Profile* profile,
                                         int icon_size_in_dip,
                                         AppIconLoaderDelegate* delegate)
    : ChromeAppIconLoader(profile,
                          icon_size_in_dip,
                          ResizeFunction(),
                          delegate) {}

ChromeAppIconLoader::~ChromeAppIconLoader() {}

bool ChromeAppIconLoader::CanLoadImageForApp(const std::string& id) {
  if (map_.find(id) != map_.end())
    return true;

  const Extension* extension = GetExtensionByID(profile(), id);
  if (!extension || (extensions_only_ && !extension->is_extension()))
    return false;

  return true;
}

void ChromeAppIconLoader::FetchImage(const std::string& id) {
  auto it = map_.find(id);
  if (it != map_.end()) {
    if (it->second && !it->second->image_skia().isNull())
      OnIconUpdated(it->second.get());
    return;  // Already loaded the image.
  }

  const Extension* extension = GetExtensionByID(profile(), id);
  if (!extension)
    return;

  std::unique_ptr<ChromeAppIcon> icon =
      ChromeAppIconService::Get(profile())->CreateIcon(
          this, id, icon_size_in_dip(), resize_function_);
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

void ChromeAppIconLoader::SetExtensionsOnly() {
  extensions_only_ = true;
}

void ChromeAppIconLoader::OnIconUpdated(ChromeAppIcon* icon) {
  delegate()->OnAppImageUpdated(icon->app_id(), icon->image_skia(),
                                /*is_placeholder_icon=*/false,
                                /*badge_image=*/std::nullopt);
}

}  // namespace extensions
