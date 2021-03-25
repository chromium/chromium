// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_REPORTING_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_REPORTING_MANAGER_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/util/statusor.h"

namespace content {
class WebContents;
}

class DlpPolicyEvent;
namespace policy {
// helper function to create DlpPolicyEvents to be enqueued or used to test
// against.
DlpPolicyEvent* CreateDlpPolicyEvent(content::WebContents* source,
                                     DlpRulesManager::Level level,
                                     DlpRulesManager::Restriction restriction);

// DlpReportingManger controls the coordination and setup towards the reporting
// pipeline so that other areas of the DLP functionality don't need to know
// about reporting but just trigger some functionality (e.g.
// ReportPrintingEvent) that will take over the work to queue extract relevant
// data, mask if necessary and much more.
class DlpReportingManager {
 public:
  static void Init();
  static DlpReportingManager* Get();
  static void SetDlpReportingManagerForTesting(DlpReportingManager* manager);

  // The different methods that cause report events from the specific
  // restrictions.
  void ReportPrintingEvent(content::WebContents* contents,
                           DlpRulesManager::Level level) const;

 private:
  // Result of trying to build a |ReportQueueConfiguration|.
  using ReportQueueConfigResult = ::reporting::StatusOr<
      std::unique_ptr<::reporting::ReportQueueConfiguration>>;

  bool ReportingEnabled() const { return true; }

  // methods to setup the reporting environment
  void BuildReportQueue(
      reporting::ReportingClient::CreateReportQueueCallback callback);
  void BuildReportQueueConfiguration(
      const policy::DMToken& dm_token,
      reporting::ReportQueueConfiguration::PolicyCheckCallback callback);

  void OnReportQueueResult(
      reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>
          report_queue_result);

  void OnEventEnqueued(reporting::Status status) const;

  ReportQueueConfigResult report_queue_config_;
  std::unique_ptr<reporting::ReportQueue> report_queue_;

  friend class DlpReportingManagerTest;
  friend class DlpContentManagerTest;

  DlpReportingManager();
  DlpReportingManager(const DlpReportingManager&) = delete;
  ~DlpReportingManager();
  DlpReportingManager& operator=(const DlpReportingManager&) = delete;
};
}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_REPORTING_MANAGER_H_
