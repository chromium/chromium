// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TEST_EXTENSION_EVENT_OBSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TEST_EXTENSION_EVENT_OBSERVER_H_

#include "base/values.h"
#include "extensions/browser/test_event_router.h"

class GURL;

namespace safe_browsing {

class TestExtensionEventObserver
    : public extensions::TestEventRouter::EventObserver {
 public:
  explicit TestExtensionEventObserver(
      extensions::TestEventRouter* event_router);

  TestExtensionEventObserver(const TestExtensionEventObserver&) = delete;
  TestExtensionEventObserver& operator=(const TestExtensionEventObserver&) =
      delete;

  ~TestExtensionEventObserver() override = default;

  // Removes |event_args_| from |*this| and returns them.
  base::Value PassEventArgs();

  // extensions::TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override;

  void VerifyLatestSecurityInterstitialEvent(
      const std::string& expected_event_name,
      const GURL& expected_page_url,
      const std::string& expected_reason,
      const std::string& expected_username = "",
      int expected_net_error_code = 0);

 private:
  // The arguments passed for the last observed event.
  base::Value::List latest_event_args_;
  std::string latest_event_name_;
};

std::unique_ptr<KeyedService> BuildSafeBrowsingPrivateEventRouter(
    content::BrowserContext* context);

std::unique_ptr<KeyedService> BuildRealtimeReportingClient(
    content::BrowserContext* context);
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TEST_EXTENSION_EVENT_OBSERVER_H_
