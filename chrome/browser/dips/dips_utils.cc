// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_utils.h"

#include <algorithm>
#include <string_view>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"
#include "url/origin.h"

base::FilePath GetDIPSFilePath(content::BrowserContext* context) {
  return context->GetPath().Append(kDIPSFilename);
}

bool UpdateTimestampRange(TimestampRange& range, base::Time time) {
  if (!range.has_value()) {
    range = {time, time};
    return true;
  }

  if (time < range->first) {
    range->first = time;
    return true;
  }

  if (time > range->second) {
    range->second = time;
    return true;
  }

  return false;
}

bool IsNullOrWithin(const TimestampRange& inner, const TimestampRange& outer) {
  if (!inner.has_value()) {
    return true;
  }

  if (!outer.has_value()) {
    return false;
  }

  return outer->first <= inner->first && inner->second <= outer->second;
}

std::ostream& operator<<(std::ostream& os, TimestampRange range) {
  if (!range.has_value()) {
    return os << "[NULL, NULL]";
  }
  return os << "[" << range->first << ", " << range->second << "]";
}

// SiteDataAccessType:

std::string_view SiteDataAccessTypeToString(SiteDataAccessType type) {
  switch (type) {
    case SiteDataAccessType::kUnknown:
      return "Unknown";
    case SiteDataAccessType::kNone:
      return "None";
    case SiteDataAccessType::kRead:
      return "Read";
    case SiteDataAccessType::kWrite:
      return "Write";
    case SiteDataAccessType::kReadWrite:
      return "ReadWrite";
  }
}

std::ostream& operator<<(std::ostream& os, SiteDataAccessType access_type) {
  return os << SiteDataAccessTypeToString(access_type);
}

// DIPSCookieMode:
DIPSCookieMode GetDIPSCookieMode(bool is_otr) {
  return is_otr ? DIPSCookieMode::kOffTheRecord_Block3PC
                : DIPSCookieMode::kBlock3PC;
}

std::string_view GetHistogramSuffix(DIPSCookieMode mode) {
  // Any changes here need to be reflected in DIPSCookieMode in
  // tools/metrics/histograms/metadata/others/histograms.xml
  switch (mode) {
    case DIPSCookieMode::kBlock3PC:
      return ".Block3PC";
    case DIPSCookieMode::kOffTheRecord_Block3PC:
      return ".OffTheRecord_Block3PC";
  }
  DCHECK(false) << "Invalid DIPSCookieMode";
  return std::string_view();
}

const char* DIPSCookieModeToString(DIPSCookieMode mode) {
  switch (mode) {
    case DIPSCookieMode::kBlock3PC:
      return "Block3PC";
    case DIPSCookieMode::kOffTheRecord_Block3PC:
      return "OffTheRecord_Block3PC";
  }
}

std::ostream& operator<<(std::ostream& os, DIPSCookieMode mode) {
  return os << DIPSCookieModeToString(mode);
}

// DIPSRedirectType:
std::string_view GetHistogramPiece(DIPSRedirectType type) {
  // Any changes here need to be reflected in
  // tools/metrics/histograms/metadata/privacy/histograms.xml
  switch (type) {
    case DIPSRedirectType::kClient:
      return "Client";
    case DIPSRedirectType::kServer:
      return "Server";
  }
  DCHECK(false) << "Invalid DIPSRedirectType";
  return std::string_view();
}

const char* DIPSRedirectTypeToString(DIPSRedirectType type) {
  switch (type) {
    case DIPSRedirectType::kClient:
      return "Client";
    case DIPSRedirectType::kServer:
      return "Server";
  }
}

std::ostream& operator<<(std::ostream& os, DIPSRedirectType type) {
  return os << DIPSRedirectTypeToString(type);
}

int64_t BucketizeBounceDelay(base::TimeDelta delta) {
  return std::clamp(delta.InSeconds(), INT64_C(0), INT64_C(10));
}

std::string GetSiteForDIPS(const GURL& url) {
  const auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? url.host() : domain;
}

std::string GetSiteForDIPS(const url::Origin& origin) {
  const auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? origin.host() : domain;
}

bool HasSameSiteIframe(content::WebContents* web_contents, const GURL& url) {
  const auto popup_site = net::SiteForCookies::FromUrl(url);
  bool found = false;

  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
      [&](content::RenderFrameHost* frame) {
        if (frame->IsInPrimaryMainFrame()) {
          // Continue to look at children of the main frame.
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        }

        // Note: For future first-party checks, consider using schemeful site
        // comparisons. More specs are moving to schemeful, although this is
        // different from how cookie access is currently classified.
        if (popup_site.IsFirstPartyWithSchemefulMode(
                frame->GetLastCommittedURL(), /*compute_schemefully=*/false)) {
          // We found a same-site iframe -- break out of the ForEach loop.
          found = true;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }

        // Not same-site, so skip children and go to the next sibling iframe.
        return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
      });

  return found;
}

const base::TimeDelta kDIPSTimestampUpdateInterval = base::Minutes(1);

bool UpdateTimestamp(std::optional<base::Time>& last_time, base::Time now) {
  if (!last_time.has_value() ||
      (now - last_time.value()) >= kDIPSTimestampUpdateInterval) {
    last_time = now;
    return true;
  }

  return false;
}

OptionalBool IsAdTaggedCookieForHeuristics(
    const content::CookieAccessDetails& details) {
  if (!base::FeatureList::IsEnabled(
          network::features::kSkipTpcdMitigationsForAds) ||
      !network::features::kSkipTpcdMitigationsForAdsHeuristics.Get()) {
    return OptionalBool::kUnknown;
  }
  return ToOptionalBool(details.cookie_setting_overrides.Has(
      net::CookieSettingOverride::kSkipTPCDHeuristicsGrant));
}

bool HasCHIPS(const net::CookieAccessResultList& cookie_access_result_list) {
  for (const auto& cookie_with_access_result : cookie_access_result_list) {
    if (cookie_with_access_result.cookie.IsPartitioned()) {
      return true;
    }
  }
  return false;
}
