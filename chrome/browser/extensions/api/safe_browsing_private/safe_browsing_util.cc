// Copyright 2019 The Chromium Authors. All rights reserved.
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
    entry.main_frame_url =
        std::make_unique<std::string>(referrer.main_frame_url());
  }
  // This url type value is deprecated and should not be used.
  DCHECK_NE(
      referrer.type(),
      safe_browsing::ReferrerChainEntry_URLType_DEPRECATED_SERVER_REDIRECT);
  switch (referrer.type()) {
    case safe_browsing::ReferrerChainEntry_URLType_EVENT_URL:
      entry.url_type = safe_browsing_private::URLType::URL_TYPE_EVENT_URL;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_LANDING_PAGE:
      entry.url_type = safe_browsing_private::URLType::URL_TYPE_LANDING_PAGE;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_LANDING_REFERRER:
      entry.url_type =
          safe_browsing_private::URLType::URL_TYPE_LANDING_REFERRER;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_CLIENT_REDIRECT:
      entry.url_type = safe_browsing_private::URLType::URL_TYPE_CLIENT_REDIRECT;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_RECENT_NAVIGATION:
      entry.url_type =
          safe_browsing_private::URLType::URL_TYPE_RECENT_NAVIGATION;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_REFERRER:
      entry.url_type = safe_browsing_private::URLType::URL_TYPE_REFERRER;
      break;
    case safe_browsing::ReferrerChainEntry_URLType_DEPRECATED_SERVER_REDIRECT:
      NOTREACHED();
  }
  if (referrer.ip_addresses_size() > 0) {
    entry.ip_addresses = std::make_unique<std::vector<std::string>>();
    entry.ip_addresses->reserve(referrer.ip_addresses_size());
    for (const std::string& ip_address : referrer.ip_addresses())
      entry.ip_addresses->emplace_back(ip_address);
  }
  if (referrer.has_referrer_url()) {
    entry.referrer_url = std::make_unique<std::string>(referrer.referrer_url());
  }
  if (referrer.has_referrer_main_frame_url()) {
    entry.referrer_main_frame_url =
        std::make_unique<std::string>(referrer.referrer_main_frame_url());
  }
  if (referrer.has_is_retargeting())
    entry.is_retargeting = std::make_unique<bool>(referrer.is_retargeting());
  if (referrer.has_navigation_time_msec()) {
    entry.navigation_time_ms =
        std::make_unique<double>(referrer.navigation_time_msec());
  }
  if (referrer.server_redirect_chain_size() > 0) {
    entry.server_redirect_chain =
        std::make_unique<std::vector<safe_browsing_private::ServerRedirect>>();
    entry.server_redirect_chain->reserve(referrer.server_redirect_chain_size());
    for (const auto& server_redirect : referrer.server_redirect_chain()) {
      safe_browsing_private::ServerRedirect result;
      result.url = std::make_unique<std::string>(server_redirect.url());
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
            safe_browsing_private::NAVIGATION_INITIATION_BROWSER_INITIATED;
        break;
      case safe_browsing::
          ReferrerChainEntry_NavigationInitiation_RENDERER_INITIATED_WITHOUT_USER_GESTURE:
        entry.navigation_initiation = safe_browsing_private::
            NAVIGATION_INITIATION_RENDERER_INITIATED_WITHOUT_USER_GESTURE;
        break;
      case safe_browsing::
          ReferrerChainEntry_NavigationInitiation_RENDERER_INITIATED_WITH_USER_GESTURE:
        entry.navigation_initiation = safe_browsing_private::
            NAVIGATION_INITIATION_RENDERER_INITIATED_WITH_USER_GESTURE;
        break;
      case safe_browsing::ReferrerChainEntry_NavigationInitiation_UNDEFINED:
        NOTREACHED();
    }
  }
  if (referrer.has_maybe_launched_by_external_application()) {
    entry.maybe_launched_by_external_app = std::make_unique<bool>(
        referrer.maybe_launched_by_external_application());
  }
  if (referrer.has_is_subframe_url_removed()) {
    entry.is_subframe_url_removed =
        std::make_unique<bool>(referrer.is_subframe_url_removed());
  }
  if (referrer.has_is_subframe_referrer_url_removed()) {
    entry.is_subframe_referrer_url_removed =
        std::make_unique<bool>(referrer.is_subframe_referrer_url_removed());
  }

  return entry;
}

}  // namespace safe_browsing_util

}  // namespace extensions
