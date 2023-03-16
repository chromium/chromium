// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_BROWSER_CRASH_EVENT_ROUTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_BROWSER_CRASH_EVENT_ROUTER_H_

#include "content/public/browser/browser_context.h"

namespace enterprise_connectors {

// An instance of class is owned by the ConnectorsManager, we use its lifetime
// to manage which profiles are observed for the purposes of crash reporting.
// Its constructor and destructor add and remove profiles to the
// CrashReportingContext, respectively, if they are valid for crash reporting.
class BrowserCrashEventRouter {
 public:
  explicit BrowserCrashEventRouter(content::BrowserContext* context);

  BrowserCrashEventRouter(const BrowserCrashEventRouter&) = delete;
  BrowserCrashEventRouter& operator=(const BrowserCrashEventRouter&) = delete;
  BrowserCrashEventRouter(BrowserCrashEventRouter&&) = delete;
  BrowserCrashEventRouter& operator=(BrowserCrashEventRouter&&) = delete;
  ~BrowserCrashEventRouter();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_BROWSER_CRASH_EVENT_ROUTER_H_
