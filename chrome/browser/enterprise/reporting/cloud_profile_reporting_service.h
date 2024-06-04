// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"

class Profile;

namespace enterprise_reporting {

class CloudProfileReportingService : public KeyedService,
                                     public policy::CloudPolicyCore::Observer {
 public:
  explicit CloudProfileReportingService(Profile* profile);
  CloudProfileReportingService(const CloudProfileReportingService&) = delete;
  CloudProfileReportingService& operator=(const CloudProfileReportingService&) =
      delete;
  ~CloudProfileReportingService() override;

  ReportScheduler* report_scheduler() { return report_scheduler_.get(); }

  void CreateReportScheduler();

  // policy::CloudPolicyCore::Observer
  void OnCoreConnected(policy::CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(policy::CloudPolicyCore* core) override;
  void OnCoreDisconnecting(policy::CloudPolicyCore* core) override;

  void InitForTesting();

 private:
  void Init();

  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;
  std::unique_ptr<ReportScheduler> report_scheduler_;
  raw_ptr<Profile> profile_;

  base::ScopedObservation<policy::CloudPolicyCore,
                          policy::CloudPolicyCore::Observer>
      core_observation_{this};

  base::WeakPtrFactory<CloudProfileReportingService> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_H_
