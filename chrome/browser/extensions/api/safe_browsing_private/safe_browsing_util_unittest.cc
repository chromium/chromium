// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_util.h"

#include <string>

#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing_private = extensions::api::safe_browsing_private;

namespace extensions {

namespace {

const char kMainFrameUrl[] = "https://www.example.com/info";
const char kIpAddress[] = "localhost";
const char kReferrerUrl[] = "https://www.example.com";
const bool kIsRetargeting = false;
const double kNavTime = 12345;
const char kServerRedirectUrl[] = "https://example.com/redirect";
const bool kMaybeLaunched = false;
const bool kIsSubframeUrlRemoved = false;
const bool kIsSubframeReferrerUrlRemoved = false;
const bool kIsUrlRemovedByPolicy = false;

void InitializeFakeReferrerChainEntry(
    std::string url,
    safe_browsing::ReferrerChainEntry* referrer) {
  referrer->set_url(url);
  referrer->set_main_frame_url(kMainFrameUrl);
  referrer->set_type(safe_browsing::ReferrerChainEntry_URLType_LANDING_PAGE);
  referrer->add_ip_addresses(kIpAddress);
  referrer->set_referrer_url(kReferrerUrl);
  referrer->set_is_retargeting(kIsRetargeting);
  referrer->set_navigation_time_msec(kNavTime);
  safe_browsing::ReferrerChainEntry_ServerRedirect* server_redirect =
      referrer->add_server_redirect_chain();
  server_redirect->set_url(kServerRedirectUrl);
  referrer->set_navigation_initiation(
      safe_browsing::ReferrerChainEntry_NavigationInitiation_BROWSER_INITIATED);
  referrer->set_maybe_launched_by_external_application(kMaybeLaunched);
  referrer->set_is_subframe_url_removed(kIsSubframeUrlRemoved);
  referrer->set_is_subframe_referrer_url_removed(kIsSubframeReferrerUrlRemoved);
  referrer->set_is_url_removed_by_policy(kIsUrlRemovedByPolicy);
}

void ValidateReferrerChain(
    const safe_browsing_private::ReferrerChainEntry& referrer) {
  ASSERT_FALSE(referrer.url.empty());
  EXPECT_EQ(*referrer.main_frame_url, kMainFrameUrl);
  EXPECT_EQ(referrer.url_type, safe_browsing_private::URLType::kLandingPage);
  EXPECT_EQ(referrer.ip_addresses->at(0), kIpAddress);
  EXPECT_EQ(*referrer.referrer_url, kReferrerUrl);
  EXPECT_EQ(*referrer.is_retargeting, kIsRetargeting);
  EXPECT_EQ(*referrer.navigation_time_ms, kNavTime);
  EXPECT_EQ(*referrer.server_redirect_chain->at(0).url, kServerRedirectUrl);
  EXPECT_EQ(referrer.navigation_initiation,
            safe_browsing_private::NavigationInitiation::kBrowserInitiated);
  EXPECT_EQ(*referrer.maybe_launched_by_external_app, kMaybeLaunched);
  EXPECT_EQ(*referrer.is_subframe_url_removed, kIsSubframeUrlRemoved);
  EXPECT_EQ(*referrer.is_subframe_referrer_url_removed,
            kIsSubframeReferrerUrlRemoved);
  EXPECT_EQ(referrer.is_url_removed_by_policy, kIsUrlRemovedByPolicy);
}

}  // namespace

// Tests that we correctly convert referrer chain entries from proto to idl.
TEST(SafeBrowsingUtilUnitTest, ReferrerToReferrerChainEntry) {
  safe_browsing::ReferrerChainEntry entry;
  InitializeFakeReferrerChainEntry("https://foo.com", &entry);
  safe_browsing_private::ReferrerChainEntry landing_referrer_result =
      safe_browsing_util::ReferrerToReferrerChainEntry(entry);
  ValidateReferrerChain(landing_referrer_result);
  EXPECT_EQ(landing_referrer_result.url, "https://foo.com");
}

}  // namespace extensions
