// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon_source.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace apps {

namespace {

void LoadDefaultImage(content::URLDataSource::GotDataCallback callback) {
  std::string_view contents =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          IDR_APP_DEFAULT_ICON, apps_util::GetPrimaryDisplayUIScaleFactor());

  base::RefCountedBytes* image_bytes = new base::RefCountedBytes();
  image_bytes->as_vector().assign(contents.begin(), contents.end());
  std::move(callback).Run(image_bytes);
}

void RunCallback(content::URLDataSource::GotDataCallback callback,
                 apps::IconValuePtr iv) {
  if (!iv || iv->compressed.empty()) {
    LoadDefaultImage(std::move(callback));
    return;
  }
  base::RefCountedBytes* image_bytes =
      new base::RefCountedBytes(iv->compressed);
  std::move(callback).Run(image_bytes);
}

}  // namespace

AppIconSource::AppIconSource(Profile* profile) : profile_(profile) {}

AppIconSource::~AppIconSource() = default;

// static
GURL AppIconSource::GetIconURL(const std::string& app_id, int icon_size) {
  GURL icon_url(base::StringPrintf("%s%s/%d", chrome::kChromeUIAppIconURL,
                                   app_id.c_str(), icon_size));
  CHECK(icon_url.is_valid());
  return icon_url;
}

std::string AppIconSource::GetSource() {
  return chrome::kChromeUIAppIconHost;
}

void AppIconSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  std::string request_path = content::URLDataSource::URLToRequestPath(url);
  std::vector<std::string> path_parts = base::SplitString(
      request_path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Check whether data exists, load default image if not.
  if (request_path.empty() || path_parts.size() < 2) {
    LoadDefaultImage(std::move(callback));
    return;
  }

  // Check whether data is of correct type, load default image if not.
  std::string size_param = base::ToLowerASCII(path_parts[1]);
  size_t query_position = size_param.find("?");
  if (query_position != std::string::npos) {
    size_param = size_param.substr(0, query_position);
  }
  int size_in_dip = 0;
  if (!base::StringToInt(size_param, &size_in_dip) || size_in_dip < 1) {
    LoadDefaultImage(std::move(callback));
    return;
  }

  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  const std::string app_id = path_parts[0];
  app_service_proxy->LoadIcon(
      app_id, IconType::kCompressed, size_in_dip,
      /*allow_placeholder_icon=*/false,
      base::BindOnce(&RunCallback, std::move(callback)));
}

std::string AppIconSource::GetMimeType(const GURL&) {
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
