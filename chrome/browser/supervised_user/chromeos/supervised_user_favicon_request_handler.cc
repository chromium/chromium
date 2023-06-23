// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/supervised_user_favicon_request_handler.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace {
constexpr int kMinIconSize = 16;
constexpr int kDesiredIconSize = 24;
constexpr int kMonogramSize = 20;

const char kFaviconAvailabilityHistogramName[] =
    "ChromeOS.FamilyLinkUser.FaviconAvailability";
}  // namespace

// static
const char* SupervisedUserFaviconRequestHandler::
    GetFaviconAvailabilityHistogramForTesting() {
  return kFaviconAvailabilityHistogramName;
}

SupervisedUserFaviconRequestHandler::SupervisedUserFaviconRequestHandler(
    const GURL& url,
    favicon::LargeIconService* large_icon_service)
    : page_url_(url), large_icon_service_(large_icon_service) {}

SupervisedUserFaviconRequestHandler::~SupervisedUserFaviconRequestHandler() =
    default;

void SupervisedUserFaviconRequestHandler::StartFaviconFetch(
    base::OnceClosure on_fetched_callback) {
  DCHECK(!network_request_completed_);
  on_fetched_callback_ = std::move(on_fetched_callback);
  FetchFaviconFromCache();
}

void SupervisedUserFaviconRequestHandler::FetchFaviconFromCache() {
  large_icon_service_->GetLargeIconImageOrFallbackStyleForPageUrl(
      page_url_, kMinIconSize, kDesiredIconSize,
      base::BindOnce(
          &SupervisedUserFaviconRequestHandler::OnGetFaviconFromCacheFinished,
          weak_ptr_factory_.GetWeakPtr()),
      &favicon_task_tracker_);
}

gfx::ImageSkia SupervisedUserFaviconRequestHandler::GetFaviconOrFallback() {
  if (favicon_.isNull()) {
    base::UmaHistogramEnumeration(kFaviconAvailabilityHistogramName,
                                  FaviconAvailability::kUnavailable);
    return gfx::ImageSkia::CreateFrom1xBitmap(favicon::GenerateMonogramFavicon(
        page_url_, kMonogramSize, kDesiredIconSize));
  }
  base::UmaHistogramEnumeration(kFaviconAvailabilityHistogramName,
                                FaviconAvailability::kAvailable);
  return favicon_;
}

void SupervisedUserFaviconRequestHandler::OnGetFaviconFromCacheFinished(
    const favicon_base::LargeIconImageResult& result) {
  // Check if fetching the favicon from the cache was successful.
  if (!result.image.IsEmpty()) {
    large_icon_service_->TouchIconFromGoogleServer(result.icon_url);
    favicon_ = result.image.AsImageSkia();
    std::move(on_fetched_callback_).Run();
    return;
  }

  // Do not make another network request if one has already been made.
  if (network_request_completed_) {
    std::move(on_fetched_callback_).Run();
    return;
  }

  // Try to fetch the favicon from a Google favicon server.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("supervised_user_favicon_request", R"(
        semantics {
          sender: "SupervisedUserFaviconRequest"
          description:
            "Sends a request to a Google server to retrieve the favicon bitmap "
            "for a website that is blocked for a supervised user."
          trigger:
            "The user visits a website that has been blocked by their parent "
            "or guardian."
          data: "Page URL and desired icon size."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "This request cannot be disabled by "
          "policy because it is required to provide a favicon cue about a "
          "blocked website to a parent that is considering approving the site "
          "for a supervised user."
        })");
  large_icon_service_
      ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          page_url_,
          /*may_page_url_be_private=*/false,
          /*should_trim_page_url_path=*/false, traffic_annotation,
          base::BindOnce(&SupervisedUserFaviconRequestHandler::
                             OnGetFaviconFromGoogleServerFinished,
                         weak_ptr_factory_.GetWeakPtr()));
}

void SupervisedUserFaviconRequestHandler::OnGetFaviconFromGoogleServerFinished(
    favicon_base::GoogleFaviconServerRequestStatus status) {
  network_request_completed_ = true;
  if (status != favicon_base::GoogleFaviconServerRequestStatus::SUCCESS) {
    LOG(WARNING) << "Favicon fetch failed, using fallback favicon.";
    std::move(on_fetched_callback_).Run();
    return;
  }
  FetchFaviconFromCache();
}
