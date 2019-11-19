// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon_source.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace apps {

namespace {

void LoadDefaultImage(const content::URLDataSource::GotDataCallback& callback) {
  base::StringPiece contents =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          IDR_APP_DEFAULT_ICON, apps_util::GetPrimaryDisplayUIScaleFactor());

  base::RefCountedBytes* image_bytes = new base::RefCountedBytes();
  image_bytes->data().assign(contents.data(),
                             contents.data() + contents.size());
  callback.Run(image_bytes);
}

void RunCallback(const content::URLDataSource::GotDataCallback& callback,
                 apps::mojom::IconValuePtr iv) {
  if (!iv->compressed.has_value() || iv->compressed.value().empty()) {
    LoadDefaultImage(callback);
    return;
  }
  base::RefCountedBytes* image_bytes =
      new base::RefCountedBytes(iv->compressed.value());
  callback.Run(image_bytes);
}

}  // namespace

AppIconSource::AppIconSource(Profile* profile) : profile_(profile) {}

AppIconSource::~AppIconSource() = default;

std::string AppIconSource::GetSource() {
  return chrome::kChromeUIAppIconHost;
}

void AppIconSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  const std::string path_lower =
      base::ToLowerASCII(content::URLDataSource::URLToRequestPath(url));
  std::vector<std::string> path_parts = base::SplitString(
      path_lower, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Check data exists, load default image if it doesn't.
  if (path_lower.empty() || path_parts.size() < 2) {
    LoadDefaultImage(callback);
    return;
  }

  // Check data is correct type, load default image if not.
  const std::string app_id = path_parts[0];
  std::string size_param = path_parts[1];
  int size = 0;
  if (!base::StringToInt(size_param, &size)) {
    LoadDefaultImage(callback);
    return;
  }
  constexpr bool quantize_to_supported_scale_factor = true;
  int size_in_dip =
      apps_util::ConvertPxToDip(size, quantize_to_supported_scale_factor);

  apps::AppServiceProxy* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  if (!app_service_proxy) {
    LoadDefaultImage(callback);
    return;
  }

  const apps::mojom::AppType app_type =
      app_service_proxy->AppRegistryCache().GetAppType(app_id);
  constexpr bool allow_placeholder_icon = false;
  app_service_proxy->LoadIcon(
      app_type, app_id, apps::mojom::IconCompression::kCompressed, size_in_dip,
      allow_placeholder_icon, base::BindOnce(&RunCallback, callback));
}

std::string AppIconSource::GetMimeType(const std::string&) {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

bool AppIconSource::AllowCaching() {
  // Should not be cached as caching is performed by proxy.
  return false;
}

bool AppIconSource::ShouldReplaceExistingSource() {
  // The source doesn't maintain its own state so there's no need to replace it.
  return false;
}

}  // namespace apps
