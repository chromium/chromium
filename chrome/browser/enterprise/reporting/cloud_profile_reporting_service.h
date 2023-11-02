// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

class Profile;

namespace policy {
class DeviceManagementService;
}

namespace enterprise_reporting {

class CloudProfileReportingService : public KeyedService {
 public:
  CloudProfileReportingService(
      Profile* profile,
      policy::DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  CloudProfileReportingService(const CloudProfileReportingService&) = delete;
  CloudProfileReportingService& operator=(const CloudProfileReportingService&) =
      delete;
  ~CloudProfileReportingService() override;

  ReportScheduler* report_scheduler() { return report_scheduler_.get(); }

 private:
  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;
  std::unique_ptr<ReportScheduler> report_scheduler_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_H_
