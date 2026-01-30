// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_NAVIGATION_HANDLE_DATA_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_NAVIGATION_HANDLE_DATA_DELEGATE_H_

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"
#include "content/public/browser/navigation_handle.h"

namespace enterprise_reporting {

class NavigationHandleDataDelegate
    : public SaasUsageReportingController::NavigationDataDelegate {
 public:
  explicit NavigationHandleDataDelegate(
      content::NavigationHandle& navigation_handle);
  NavigationHandleDataDelegate(const NavigationHandleDataDelegate&) = delete;
  NavigationHandleDataDelegate& operator=(const NavigationHandleDataDelegate&) =
      delete;

  ~NavigationHandleDataDelegate() override = default;

  // SaasUsageReportingController::NavigationDataDelegate
  GURL GetUrl() const override;
  std::string GetEncryptionProtocol() const override;

 private:
  const raw_ref<content::NavigationHandle> navigation_handle_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_NAVIGATION_HANDLE_DATA_DELEGATE_H_
