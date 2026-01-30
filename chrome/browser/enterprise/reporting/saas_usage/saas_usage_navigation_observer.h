// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_NAVIGATION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"
#include "content/public/browser/web_contents_observer.h"

namespace enterprise_reporting {

// Observes navigations in a web contents and reports them to the controller
// if navigation is reportable:
// - it's a primary frame navigation
// - it's not a same document navigation
// - it's not a download
// - it's not an error page
// - it's not a redirect
class SaasUsageNavigationObserver : public content::WebContentsObserver {
 public:
  explicit SaasUsageNavigationObserver(content::WebContents* web_contents);
  SaasUsageNavigationObserver(const SaasUsageNavigationObserver&) = delete;
  SaasUsageNavigationObserver& operator=(const SaasUsageNavigationObserver&) =
      delete;

  ~SaasUsageNavigationObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  raw_ptr<SaasUsageReportingController> controller_ = nullptr;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_NAVIGATION_OBSERVER_H_
