// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DLP_REPORTING_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DLP_REPORTING_MANAGER_H_

#include <memory>

#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/enterprise/data_controls/core/browser/rule.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/util/status.h"

class DlpPolicyEvent;

namespace data_controls {

// Helper class used to build custom DLP policy events.
class DlpPolicyEventBuilder {
 public:
  // Possible event types.
  static std::unique_ptr<DlpPolicyEventBuilder> Event(
      const std::string& src_url,
      const std::string& rule_name,
      const std::string& rule_id,
      Rule::Restriction restriction,
      Rule::Level level);
  static std::unique_ptr<DlpPolicyEventBuilder> WarningProceededEvent(
      const std::string& src_url,
      const std::string& rule_name,
      const std::string& rule_id,
      Rule::Restriction restriction);

  // Setters used to define event properties.
  void SetDestinationUrl(const std::string& dst_url);
#if BUILDFLAG(IS_CHROMEOS)
  void SetDestinationComponent(Component dst_component);
#endif  // BUILDFLAG(IS_CHROMEOS)
  void SetContentName(const std::string& content_name);

  // Stops the creation and returns the created event.
  DlpPolicyEvent Create();

 private:
  DlpPolicyEventBuilder();

  // Private setters used to define mandatory event properties set up internally
  // when a DlpPolicyEventBuilder is built.
  void SetSourceUrl(const std::string& src_url);
  void SetRestriction(Rule::Restriction restriction);

  DlpPolicyEvent event;
};

// helper function to create DlpPolicyEvents to be enqueued or used to test
// against.
DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_url,
                                    Rule::Restriction restriction,
                                    const std::string& rule_name,
                                    const std::string& rule_id,
                                    Rule::Level level);
DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_url,
                                    const std::string& dst_url,
                                    Rule::Restriction restriction,
                                    const std::string& rule_name,
                                    const std::string& rule_id,
                                    Rule::Level level);

#if BUILDFLAG(IS_CHROMEOS)
DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_url,
                                    Component dst_component,
                                    Rule::Restriction restriction,
                                    const std::string& rule_name,
                                    const std::string& rule_id,
                                    Rule::Level level);
#endif  // BUILDFLAG(IS_CHROMEOS)

template <typename... Args>
DlpPolicyEvent CreateDlpPolicyWarningProceededEvent(Args... args) {
  auto event = CreateDlpPolicyEvent(args..., Rule::Level::kNotSet);
  // Override Rule::Level::kNotSet set above.
  event.set_mode(DlpPolicyEvent_Mode_WARN_PROCEED);
  return event;
}

// DlpReportingManger controls the coordination and setup towards the reporting
// pipeline so that other areas of the DLP functionality don't need to know
// about reporting but just trigger some functionality (e.g.
// ReportPrintingEvent) that will take over the work to queue extract relevant
// data, mask if necessary and much more.
class DlpReportingManager {
 public:
  using ReportQueueSetterCallback =
      base::OnceCallback<void(std::unique_ptr<reporting::ReportQueue>)>;

  // For callers who are interested in observing DLP reporting events.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked whenever a new event is being reported.
    virtual void OnReportEvent(DlpPolicyEvent event) = 0;
  };

  DlpReportingManager();
  DlpReportingManager(const DlpReportingManager&) = delete;
  ~DlpReportingManager();
  DlpReportingManager& operator=(const DlpReportingManager&) = delete;

  // The different methods that cause report events from the specific
  // restrictions.
  void ReportEvent(const std::string& src_url,
                   Rule::Restriction restriction,
                   Rule::Level level,
                   const std::string& rule_name,
                   const std::string& rule_id);
  void ReportEvent(const std::string& src_url,
                   const std::string& dst_url,
                   Rule::Restriction restriction,
                   Rule::Level level,
                   const std::string& rule_name,
                   const std::string& rule_id);
#if BUILDFLAG(IS_CHROMEOS)
  void ReportEvent(const std::string& src_url,
                   Component dst_component,
                   Rule::Restriction restriction,
                   Rule::Level level,
                   const std::string& rule_name,
                   const std::string& rule_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

  template <typename... Args>
  void ReportWarningProceededEvent(Args... args) {
    ReportEvent(CreateDlpPolicyWarningProceededEvent(args...));
  }
  void ReportEvent(DlpPolicyEvent event);

  size_t events_reported() const { return events_reported_; }

  // Test hook for overriding the default report queue used for reporting
  // purposes.
  //
  // TODO(b/202746926): Ideally, the report queue should be overridden at
  // |DlpReportingManager| instantiation, but since a lot of test scenarios
  // follow deferred mock setup we defer refactoring this part for later.
  void SetReportQueueForTest(
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
          report_queue);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void OnEventEnqueued(reporting::Status status);

  // Counter for the number of events reported from login.
  size_t events_reported_ = 0;

  std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
      report_queue_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<DlpReportingManager> weak_factory_{this};
};
}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DLP_REPORTING_MANAGER_H_
