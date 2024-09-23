// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/android/favicon_fetcher.h"

#include <cstddef>
#include <memory>

#include "base/android/callback_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/gurl.h"

FaviconFetcher::FaviconFetcher(
    raw_ptr<favicon::LargeIconService> large_icon_service)
    : large_icon_service_(large_icon_service) {}

FaviconFetcher::~FaviconFetcher() {}

void FaviconFetcher::Destroy() {
  delete this;
}

void FaviconFetcher::OnFaviconDownloaded(
    const GURL& url,
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    FaviconDimensions faviconDimensions,
    favicon_base::GoogleFaviconServerRequestStatus status) {
  if (status == favicon_base::GoogleFaviconServerRequestStatus::SUCCESS) {
    FetchFavicon(url, false, faviconDimensions.min_source_size_in_pixel,
                 faviconDimensions.desired_size_in_pixel, std::move(callback));
  } else {
    LOG(WARNING)
        << "Unable to obtain a favicon image with the required specs for "
        << url.host();
    Destroy();
  }
}

void FaviconFetcher::ExecuteFaviconCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    SkBitmap bitmap) {
  base::android::RunObjectCallbackAndroid(callback,
                                          gfx::ConvertToJavaBitmap(bitmap));
}

void FaviconFetcher::OnGetFaviconFromCacheFinished(
    const GURL& url,
    bool continue_to_server,
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    FaviconDimensions faviconDimensions,
    const favicon_base::LargeIconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    SkBitmap faviconBitmap =
        image_result.image.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
    // Return the image to the caller by executing the callback and destroy this
    // instance.
    ExecuteFaviconCallback(callback, faviconBitmap);
    Destroy();
    return;
  }

  // Try to fetch the favicon from a Google favicon server.
  if (continue_to_server) {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("favicon_fetcher_get_favicon", R"(
          semantics {
            sender: "Favicon"
            description:
              "Sends a request to a Google server to retrieve a favicon bitmap "
              "for a URL that a supervised user has requested approval to "
              "access from their parents."
            trigger:
              "A request can be sent if Chrome does not have a favicon for a "
              "particular page that supervised users are requesting approval to "
              "access from their parents."
            data: "Page URL and desired icon size."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting: "This feature cannot be disabled by settings."
            policy_exception_justification: "Not implemented."
          }
          comments: "No policy is necessary as the request cannot be turned off via settings."
            "A request for a favicon will always be sent for supervised users."
          )");
    large_icon_service_
        ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
            url,
            /*should_trim_page_url_path=*/false, traffic_annotation,
            base::BindOnce(&FaviconFetcher::OnFaviconDownloaded,
                           base::Unretained(this), url, std::move(callback),
                           faviconDimensions));
  } else {
    Destroy();
  }
}

base::WeakPtr<FaviconFetcher> FaviconFetcher::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FaviconFetcher::FetchFavicon(
    const GURL& url,
    bool continue_to_server,
    int min_source_side_size_in_pixel,
    int desired_side_size_in_pixel,
    const base::android::ScopedJavaGlobalRef<jobject>& callback) {
  FaviconDimensions faviconDimensions;
  faviconDimensions.min_source_size_in_pixel = min_source_side_size_in_pixel;
  faviconDimensions.desired_size_in_pixel = desired_side_size_in_pixel;

  large_icon_service_->GetLargeIconImageOrFallbackStyleForPageUrl(
      url, faviconDimensions.min_source_size_in_pixel,
      faviconDimensions.desired_size_in_pixel,
      base::BindOnce(&FaviconFetcher::OnGetFaviconFromCacheFinished,
                     base::Unretained(this), url, continue_to_server,
                     std::move(callback), faviconDimensions),
      &task_tracker_);
}
