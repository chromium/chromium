// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_REPORTING_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_REPORTING_MANAGER_H_

#include <memory>

#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/util/status.h"

class DlpPolicyEvent;

namespace policy {
// helper function to create DlpPolicyEvents to be enqueued or used to test
// against.
DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_pattern,
                                    DlpRulesManager::Restriction restriction,
                                    DlpRulesManager::Level level);
DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_pattern,
                                    const std::string& dst_pattern,
                                    DlpRulesManager::Restriction restriction,
                                    DlpRulesManager::Level level);
DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_pattern,
                                    DlpRulesManager::Component dst_component,
                                    DlpRulesManager::Restriction restriction,
                                    DlpRulesManager::Level level);

// DlpReportingManger controls the coordination and setup towards the reporting
// pipeline so that other areas of the DLP functionality don't need to know
// about reporting but just trigger some functionality (e.g.
// ReportPrintingEvent) that will take over the work to queue extract relevant
// data, mask if necessary and much more.
class DlpReportingManager {
 public:
  using ReportQueueSetterCallback =
      base::OnceCallback<void(std::unique_ptr<reporting::ReportQueue>)>;

  DlpReportingManager();
  DlpReportingManager(const DlpReportingManager&) = delete;
  ~DlpReportingManager();
  DlpReportingManager& operator=(const DlpReportingManager&) = delete;

  // The different methods that cause report events from the specific
  // restrictions.
  void ReportEvent(const std::string& src_pattern,
                   DlpRulesManager::Restriction restriction,
                   DlpRulesManager::Level level);
  void ReportEvent(const std::string& src_pattern,
                   const std::string& dst_pattern,
                   DlpRulesManager::Restriction restriction,
                   DlpRulesManager::Level level);
  void ReportEvent(const std::string& src_pattern,
                   DlpRulesManager::Component dst_component,
                   DlpRulesManager::Restriction restriction,
                   DlpRulesManager::Level level);

  ReportQueueSetterCallback GetReportQueueSetter();

  size_t events_reported() const { return events_reported_; }

 private:
  friend class DlpReportingManagerTestHelper;

  void SetReportQueue(std::unique_ptr<reporting::ReportQueue> report_queue);

  void OnEventEnqueued(reporting::Status status);

  void ReportEvent(DlpPolicyEvent event);

  // Counter for the number of events reported from login.
  size_t events_reported_ = 0;

  std::unique_ptr<reporting::ReportQueue> report_queue_;

  base::WeakPtrFactory<DlpReportingManager> weak_factory_{this};
};
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_REPORTING_MANAGER_H_
