// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_util.h"

#include <string>
#include <utility>

#include "chrome/common/extensions/api/safe_browsing_private.h"

namespace safe_browsing_private = extensions::api::safe_browsing_private;

namespace extensions {

namespace safe_browsing_util {

safe_browsing_private::ReferrerChainEntry ReferrerToReferrerChainEntry(
    const safe_browsing::ReferrerChainEntry& referrer) {
  safe_browsing_private::ReferrerChainEntry entry;
  // Add all referrer chain entry fields to the entry.
  entry.url = referrer.url();
  if (referrer.has_main_frame_url()) {
    entry.main_frame_url = referrer.main_frame_url();
  }
  // This url type value is deprecated and should not be used.
  DCHECK_NE(
      referrer.type(),
      safe_browsing::ReferrerChainEntry_URLType_DEPRECATED_SERVER_REDIRECT);
  switch (referrer.type()) {
    case safe_browsing::ReferrerChainEntry_URLType_EVENT_URL:
      entry.url_type = safe_browsing_private::URLType::kEventUrl;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_LANDING_PAGE:
      entry.url_type = safe_browsing_private::URLType::kLandingPage;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_LANDING_REFERRER:
      entry.url_type = safe_browsing_private::URLType::kLandingReferrer;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_CLIENT_REDIRECT:
      entry.url_type = safe_browsing_private::URLType::kClientRedirect;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_RECENT_NAVIGATION:
      entry.url_type = safe_browsing_private::URLType::kRecentNavigation;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_REFERRER:
      entry.url_type = safe_browsing_private::URLType::kReferrer;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_DEPRECATED_SERVER_REDIRECT:
      NOTREACHED_IN_MIGRATION();
  }
  if (referrer.ip_addresses_size() > 0) {
    entry.ip_addresses.emplace();
    entry.ip_addresses->reserve(referrer.ip_addresses_size());
    for (const std::string& ip_address : referrer.ip_addresses())
      entry.ip_addresses->emplace_back(ip_address);
  }
  if (referrer.has_referrer_url()) {
    entry.referrer_url = referrer.referrer_url();
  }
  if (referrer.has_referrer_main_frame_url()) {
    entry.referrer_main_frame_url = referrer.referrer_main_frame_url();
  }
  if (referrer.has_is_retargeting())
    entry.is_retargeting = referrer.is_retargeting();
  if (referrer.has_navigation_time_msec()) {
    entry.navigation_time_ms = referrer.navigation_time_msec();
  }
  if (referrer.server_redirect_chain_size() > 0) {
    entry.server_redirect_chain.emplace();
    entry.server_redirect_chain->reserve(referrer.server_redirect_chain_size());
    for (const auto& server_redirect : referrer.server_redirect_chain()) {
      safe_browsing_private::ServerRedirect result;
      result.url = server_redirect.url();
      entry.server_redirect_chain->emplace_back(std::move(result));
    }
  }
  if (referrer.has_navigation_initiation()) {
    DCHECK_NE(referrer.navigation_initiation(),
              safe_browsing::ReferrerChainEntry_NavigationInitiation_UNDEFINED);
    switch (referrer.navigation_initiation()) {
      case safe_browsing::
          ReferrerChainEntry_NavigationInitiation_BROWSER_INITIATED:
        entry.navigation_initiation =
            safe_browsing_private::NavigationInitiation::kBrowserInitiated;
        break;
      case safe_browsing::
          ReferrerChainEntry_NavigationInitiation_RENDERER_INITIATED_WITHOUT_USER_GESTURE:
        entry.navigation_initiation = safe_browsing_private::
            NavigationInitiation::kRendererInitiatedWithoutUserGesture;
        break;
      case safe_browsing::
          ReferrerChainEntry_NavigationInitiation_RENDERER_INITIATED_WITH_USER_GESTURE:
        entry.navigation_initiation = safe_browsing_private::
            NavigationInitiation::kRendererInitiatedWithUserGesture;
        break;
      case safe_browsing::
          ReferrerChainEntry_NavigationInitiation_COPY_PASTE_USER_INITIATED:
        entry.navigation_initiation = safe_browsing_private::
            NavigationInitiation::kCopyPasteUserInitiated;
        break;
      case safe_browsing::
          ReferrerChainEntry_NavigationInitiation_NOTIFICATION_INITIATED:
        entry.navigation_initiation =
            safe_browsing_private::NavigationInitiation::kNotificationInitiated;
        break;
      case safe_browsing::ReferrerChainEntry_NavigationInitiation_UNDEFINED:
        NOTREACHED_IN_MIGRATION();
    }
  }
  if (referrer.has_maybe_launched_by_external_application()) {
    entry.maybe_launched_by_external_app =
        referrer.maybe_launched_by_external_application();
  }
  if (referrer.has_is_subframe_url_removed()) {
    entry.is_subframe_url_removed = referrer.is_subframe_url_removed();
  }
  if (referrer.has_is_subframe_referrer_url_removed()) {
    entry.is_subframe_referrer_url_removed =
        referrer.is_subframe_referrer_url_removed();
  }
  entry.is_url_removed_by_policy = referrer.is_url_removed_by_policy();

  return entry;
}

}  // namespace safe_browsing_util

}  // namespace extensions
