// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/third_party_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace {

// The maximum number of subframes that we've recorded timings for that we can
// keep track of in memory.
const int kMaxRecordedFrames = 50;

bool IsSameSite(const url::Origin& origin1, const url::Origin& origin2) {
  return origin1.scheme() == origin2.scheme() &&
         net::registry_controlled_domains::SameDomainOrHost(
             origin1, origin2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsSameSite(const GURL& url1, const GURL& url2) {
  return url1.SchemeIs(url2.scheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

ThirdPartyMetricsObserver::AccessedTypes::AccessedTypes(
    AccessType access_type) {
  switch (access_type) {
    case AccessType::kCookieRead:
      cookie_read = true;
      break;
    case AccessType::kCookieWrite:
      cookie_write = true;
      break;
    case AccessType::kLocalStorage:
      local_storage = true;
      break;
    case AccessType::kSessionStorage:
      session_storage = true;
      break;
    // No extra metadata required for the following types as they only record
    // use counters.
    case AccessType::kFileSystem:
    case AccessType::kIndexedDb:
    case AccessType::kCacheStorage:
      break;
    case AccessType::kUnknown:
      NOTREACHED();
      break;
  }
}

ThirdPartyMetricsObserver::ThirdPartyMetricsObserver() = default;
ThirdPartyMetricsObserver::~ThirdPartyMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ThirdPartyMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // The browser may come back, but there is no guarantee. To be safe, record
  // what we have now and ignore future changes to this navigation.
  RecordMetrics(timing);
  return STOP_OBSERVING;
}

void ThirdPartyMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordMetrics(timing);
}

void ThirdPartyMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (third_party_font_loaded_ ||
      extra_request_complete_info.request_destination !=
          network::mojom::RequestDestination::kFont) {
    return;
  }

  third_party_font_loaded_ =
      !IsSameSite(GetDelegate().GetUrl(),
                  extra_request_complete_info.origin_of_final_url.GetURL());
}

void ThirdPartyMetricsObserver::OnCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    const net::CookieList& cookie_list,
    bool blocked_by_policy) {
  OnCookieOrStorageAccess(url, first_party_url, blocked_by_policy,
                          AccessType::kCookieRead);
}

void ThirdPartyMetricsObserver::OnCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy) {
  OnCookieOrStorageAccess(url, first_party_url, blocked_by_policy,
                          AccessType::kCookieWrite);
}

void ThirdPartyMetricsObserver::RecordStorageAccessUseCounter(
    AccessType access_type) {
  page_load_metrics::mojom::PageLoadFeatures third_party_storage_features;

  switch (access_type) {
    case AccessType::kCookieRead:
      third_party_storage_features.features.push_back(
          blink::mojom::WebFeature::kThirdPartyCookieRead);
      break;
    case AccessType::kCookieWrite:
      third_party_storage_features.features.push_back(
          blink::mojom::WebFeature::kThirdPartyCookieWrite);
      break;
    case AccessType::kLocalStorage:
      third_party_storage_features.features.push_back(
          blink::mojom::WebFeature::kThirdPartyLocalStorage);
      break;
    case AccessType::kSessionStorage:
      third_party_storage_features.features.push_back(
          blink::mojom::WebFeature::kThirdPartySessionStorage);
      break;
    case AccessType::kFileSystem:
      third_party_storage_features.features.push_back(
          blink::mojom::WebFeature::kThirdPartyFileSystem);
      break;
    case AccessType::kIndexedDb:
      third_party_storage_features.features.push_back(
          blink::mojom::WebFeature::kThirdPartyIndexedDb);
      break;
    case AccessType::kCacheStorage:
      third_party_storage_features.features.push_back(
          blink::mojom::WebFeature::kThirdPartyCacheStorage);
      break;
    default
        :  // No feature usage recorded for storage types without a use counter.
      return;
  }

  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      GetDelegate().GetWebContents()->GetMainFrame(),
      third_party_storage_features);
}

void ThirdPartyMetricsObserver::OnStorageAccessed(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    page_load_metrics::StorageType storage_type) {
  OnCookieOrStorageAccess(url, first_party_url, blocked_by_policy,
                          StorageTypeToAccessType(storage_type));
}

void ThirdPartyMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  // A RenderFrameHost is navigating. Since this is a new navigation we want to
  // capture its paint timing. Remove the RFH from the list of recorded frames.
  // This is guaranteed to be called before receiving the first paint update for
  // the navigation.
  recorded_frames_.erase(navigation_handle->GetRenderFrameHost());
}

void ThirdPartyMetricsObserver::OnFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  recorded_frames_.erase(render_frame_host);
}

void ThirdPartyMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.paint_timing->first_contentful_paint)
    return;

  // Filter out top-frames
  if (!subframe_rfh)
    return;

  // Filter out navigations that we've already recorded, or if we've reached our
  // frame limit.
  const auto it = recorded_frames_.find(subframe_rfh);
  if (it != recorded_frames_.end() ||
      recorded_frames_.size() >= kMaxRecordedFrames) {
    return;
  }

  // Filter out first-party frames.
  content::RenderFrameHost* top_frame =
      GetDelegate().GetWebContents()->GetMainFrame();
  if (!top_frame)
    return;

  const url::Origin& top_frame_origin = top_frame->GetLastCommittedOrigin();
  const url::Origin& subframe_origin = subframe_rfh->GetLastCommittedOrigin();
  if (IsSameSite(top_frame_origin, subframe_origin))
    return;

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.ThirdParty.Frames.NavigationToFirstContentfulPaint3",
        timing.paint_timing->first_contentful_paint.value());
    recorded_frames_.insert(subframe_rfh);
  }
}

void ThirdPartyMetricsObserver::OnCookieOrStorageAccess(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    AccessType access_type) {
  if (blocked_by_policy) {
    should_record_metrics_ = false;
    return;
  }

  if (!url.is_valid())
    return;

  // TODO(csharrison): Optimize the domain lookup.
  // Note: If either |url| or |first_party_url| is empty, SameDomainOrHost will
  // return false, and function execution will continue because it is considered
  // 3rd party. Since |first_party_url| is actually the |site_for_cookies|, this
  // will happen e.g. for a 3rd party iframe on document.cookie access.
  if (IsSameSite(url, first_party_url))
    return;

  std::string registrable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  // |registrable_domain| can be empty e.g. if |url| is on an IP address, or the
  // domain is itself a TLD, or it's a file URL (in which case it has no host),
  // etc. See comment for GetDomainAndRegistry() in
  // //net/base/registry_controlled_domains/registry_controlled_domains.h.
  if (registrable_domain.empty()) {
    if (url.has_host()) {
      registrable_domain = url.host();
    } else {
      return;
    }
  }

  RecordStorageAccessUseCounter(access_type);

  GURL representative_url(
      base::StrCat({url.scheme(), "://", registrable_domain, "/"}));

  auto it = third_party_accessed_types_.find(representative_url);

  if (it != third_party_accessed_types_.end()) {
    switch (access_type) {
      case AccessType::kCookieRead:
        it->second.cookie_read = true;
        break;
      case AccessType::kCookieWrite:
        it->second.cookie_write = true;
        break;
      case AccessType::kLocalStorage:
        it->second.local_storage = true;
        break;
      case AccessType::kSessionStorage:
        it->second.session_storage = true;
        break;
      // No metadata is tracked for the following types as they only record use
      // counters.
      case AccessType::kFileSystem:
      case AccessType::kIndexedDb:
      case AccessType::kCacheStorage:
        break;
      case AccessType::kUnknown:
        NOTREACHED();
        break;
    }
    return;
  }

  // Don't let the map grow unbounded.
  if (third_party_accessed_types_.size() >= 1000)
    return;

  third_party_accessed_types_.emplace(representative_url, access_type);
}

void ThirdPartyMetricsObserver::RecordMetrics(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  if (!should_record_metrics_)
    return;

  int cookie_origin_reads = 0;
  int cookie_origin_writes = 0;
  int local_storage_origin_access = 0;
  int session_storage_origin_access = 0;

  for (auto it : third_party_accessed_types_) {
    cookie_origin_reads += it.second.cookie_read;
    cookie_origin_writes += it.second.cookie_write;
    local_storage_origin_access += it.second.local_storage;
    session_storage_origin_access += it.second.session_storage;
  }

  UMA_HISTOGRAM_COUNTS_1000("PageLoad.Clients.ThirdParty.Origins.CookieRead2",
                            cookie_origin_reads);
  UMA_HISTOGRAM_COUNTS_1000("PageLoad.Clients.ThirdParty.Origins.CookieWrite2",
                            cookie_origin_writes);
  UMA_HISTOGRAM_COUNTS_1000(
      "PageLoad.Clients.ThirdParty.Origins.LocalStorageAccess2",
      local_storage_origin_access);
  UMA_HISTOGRAM_COUNTS_1000(
      "PageLoad.Clients.ThirdParty.Origins.SessionStorageAccess2",
      session_storage_origin_access);

  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (third_party_font_loaded_ &&
      all_frames_largest_contentful_paint.ContainsValidTime() &&
      all_frames_largest_contentful_paint.Type() ==
          page_load_metrics::ContentfulPaintTimingInfo::LargestContentType::
              kText &&
      WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.ThirdParty.PaintTiming."
        "NavigationToLargestContentfulPaint.HasThirdPartyFont",
        all_frames_largest_contentful_paint.Time().value());
  }
}

ThirdPartyMetricsObserver::AccessType
ThirdPartyMetricsObserver::StorageTypeToAccessType(
    page_load_metrics::StorageType storage_type) {
  switch (storage_type) {
    case page_load_metrics::StorageType::kLocalStorage:
      return AccessType::kLocalStorage;
    case page_load_metrics::StorageType::kSessionStorage:
      return AccessType::kSessionStorage;
    case page_load_metrics::StorageType::kFileSystem:
      return AccessType::kFileSystem;
    case page_load_metrics::StorageType::kIndexedDb:
      return AccessType::kIndexedDb;
    case page_load_metrics::StorageType::kCacheStorage:
      return AccessType::kCacheStorage;
    default:
      return AccessType::kUnknown;
  }
}
