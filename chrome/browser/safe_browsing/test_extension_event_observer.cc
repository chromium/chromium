// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/test_extension_event_observer.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace OnSecurityInterstitialShown =
    extensions::api::safe_browsing_private::OnSecurityInterstitialShown;
namespace OnSecurityInterstitialProceeded =
    extensions::api::safe_browsing_private::OnSecurityInterstitialProceeded;
namespace OnPolicySpecifiedPasswordReuseDetected = extensions::api::
    safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected;
namespace OnPolicySpecifiedPasswordChanged =
    extensions::api::safe_browsing_private::OnPolicySpecifiedPasswordChanged;
namespace OnDangerousDownloadOpened =
    extensions::api::safe_browsing_private::OnDangerousDownloadOpened;

namespace safe_browsing {

TestExtensionEventObserver::TestExtensionEventObserver(
    extensions::TestEventRouter* event_router) {
  event_router->AddEventObserver(this);
}

base::Value TestExtensionEventObserver::PassEventArgs() {
  return base::Value(std::move(latest_event_args_));
}

void TestExtensionEventObserver::OnBroadcastEvent(
    const extensions::Event& event) {
  if (event.event_name == OnSecurityInterstitialProceeded::kEventName ||
      event.event_name == OnSecurityInterstitialShown::kEventName ||
      event.event_name == OnPolicySpecifiedPasswordReuseDetected::kEventName ||
      event.event_name == OnPolicySpecifiedPasswordChanged::kEventName ||
      event.event_name == OnDangerousDownloadOpened::kEventName) {
    latest_event_args_ = event.event_args.Clone();
    latest_event_name_ = event.event_name;
  }
}

void TestExtensionEventObserver::VerifyLatestSecurityInterstitialEvent(
    const std::string& expected_event_name,
    const GURL& expected_page_url,
    const std::string& expected_reason,
    const std::string& expected_username,
    int expected_net_error_code) {
  EXPECT_EQ(expected_event_name, latest_event_name_);
  const auto captured_args = std::move(PassEventArgs().GetList()[0].GetDict());
  EXPECT_EQ(expected_page_url.spec(), *captured_args.FindString("url"));
  EXPECT_EQ(expected_reason, *captured_args.FindString("reason"));
  if (!expected_username.empty())
    EXPECT_EQ(expected_username, *captured_args.FindString("userName"));
  if (expected_net_error_code == 0)
    EXPECT_FALSE(captured_args.Find("netErrorCode"));
  else
    EXPECT_EQ(base::NumberToString(expected_net_error_code),
              *captured_args.FindString("netErrorCode"));
}

std::unique_ptr<KeyedService> BuildSafeBrowsingPrivateEventRouter(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(
      new extensions::SafeBrowsingPrivateEventRouter(context));
}

std::unique_ptr<KeyedService> BuildRealtimeReportingClient(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(
      new enterprise_connectors::RealtimeReportingClient(context));
}

}  // namespace safe_browsing
