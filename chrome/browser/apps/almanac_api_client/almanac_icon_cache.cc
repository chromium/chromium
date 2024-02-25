// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_icon_cache.h"

#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace apps {
namespace {
// The UMA name for the Almanac Icon Cache client of the Image Fetcher service.
constexpr char kUmaClientName[] = "AlmanacIcons";

// Description of the network request.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("almanac_icon_cache", R"(
      semantics {
        sender: "Almanac Apps Icon Cache"
        description:
          "Sends a request to either a website or a Google-owned server to "
          "retrieve an app icon. The data is used by clients of the service "
          "for displaying app results."
        trigger:
          "A request is sent when an app is to be displayed e.g. installing "
          "or searching for an app."
        internal {
          contacts {
            email: "cros-apps-foundation-system@google.com"
          }
        }
        user_data: {
          type: NONE
        }
        data: "Icon URL"
        destination: WEBSITE
        last_reviewed: "2023-10-09"
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

void OnIconDownloaded(base::OnceCallback<void(const gfx::Image&)> callback,
                      const gfx::Image& icon,
                      const image_fetcher::RequestMetadata& metadata) {
  std::move(callback).Run(icon);
}
}  // namespace

AlmanacIconCache::AlmanacIconCache(ProfileKey* key) {
  image_fetcher::ImageFetcherService* image_fetcher_service =
      ImageFetcherServiceFactory::GetForKey(key);
  if (!image_fetcher_service) {
    return;
  }

  image_fetcher_ = image_fetcher_service->GetImageFetcher(
      image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
}

AlmanacIconCache::AlmanacIconCache() = default;
AlmanacIconCache::~AlmanacIconCache() = default;

image_fetcher::ImageFetcher* AlmanacIconCache::GetImageFetcher() {
  return image_fetcher_.get();
}

void AlmanacIconCache::GetIcon(
    const GURL& icon_url,
    base::OnceCallback<void(const gfx::Image&)> callback) {
  if (!GetImageFetcher()) {
    std::move(callback).Run(gfx::Image());
    return;
  }
  image_fetcher::ImageFetcherParams params(kTrafficAnnotation, kUmaClientName);
  GetImageFetcher()->FetchImage(
      icon_url, base::BindOnce(&OnIconDownloaded, std::move(callback)),
      std::move(params));
}

}  // namespace apps
