// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager_test_helper.h"

#include <memory>
#include <string_view>

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::Mock;

namespace data_controls {

class DlpPolicyEventMatcher : public MatcherInterface<const DlpPolicyEvent&> {
 public:
  explicit DlpPolicyEventMatcher(const DlpPolicyEvent& event)
      : source_url_(event.source().url()),
        destination_url_(event.destination().url()),
        destination_component_(event.destination().component()),
        restriction_(event.restriction()),
        mode_(event.mode()),
        content_name_(event.content_name()),
        triggered_rule_name(event.triggered_rule_name()),
        triggered_rule_id(event.triggered_rule_id()) {}

  bool MatchAndExplain(const DlpPolicyEvent& event,
                       MatchResultListener* listener) const override {
    bool source_url_equals = event.source().url() == source_url_;
    if (!source_url_equals) {
      *listener << " |source_url| is " << event.source().url();
    }
    bool destination_url_equals = event.destination().url() == destination_url_;
    if (!destination_url_equals) {
      *listener << " |destination_url| is " << event.destination().url();
    }
    bool destination_component_equals =
        event.destination().component() == destination_component_;
    if (!destination_component_equals) {
      *listener << " |destination_component| is "
                << event.destination().component();
    }
    bool restriction_equals = event.restriction() == restriction_;
    if (!restriction_equals) {
      *listener << " |restriction| is " << event.restriction();
    }
    bool mode_equals = event.mode() == mode_;
    if (!mode_equals) {
      *listener << " |mode| is " << event.mode();
    }
    bool content_name_equals = event.content_name() == content_name_;
    if (!content_name_equals) {
      *listener << " |content_name| is " << event.content_name();
    }
    bool rule_name_equals = event.triggered_rule_name() == triggered_rule_name;
    if (!rule_name_equals) {
      *listener << " |triggered_rule_name| is " << event.triggered_rule_name();
    }
    bool rule_id_equals = event.triggered_rule_id() == triggered_rule_id;
    if (!rule_id_equals) {
      *listener << " |triggered_rule_id| is " << event.triggered_rule_id();
    }

    return source_url_equals && destination_url_equals &&
           destination_component_equals && restriction_equals && mode_equals &&
           content_name_equals && rule_name_equals && rule_id_equals;
  }

  void DescribeTo(::std::ostream* os) const override {}

 private:
  const std::string source_url_;
  const std::string destination_url_;
  const int destination_component_;
  const int restriction_;
  const int mode_;
  const std::string content_name_;
  const std::string triggered_rule_name;
  const std::string triggered_rule_id;
};

Matcher<const DlpPolicyEvent&> IsDlpPolicyEvent(const DlpPolicyEvent& event) {
  return Matcher<const DlpPolicyEvent&>(new DlpPolicyEventMatcher(event));
}

void SetReportQueueForReportingManager(
    DlpReportingManager* manager,
    std::vector<DlpPolicyEvent>& events,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  auto report_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new ::reporting::MockReportQueue(),
          base::OnTaskRunnerDeleter(std::move(task_runner)));
  EXPECT_CALL(*report_queue, AddRecord)
      .WillRepeatedly(
          [&events](std::string_view record, reporting::Priority priority,
                    reporting::ReportQueue::EnqueueCallback callback) {
            DlpPolicyEvent event;
            event.ParseFromString(std::string(record));
            // Don't use this code in a multithreaded env as it can cause
            // concurrency issues with the events in the vector.
            events.push_back(event);
            std::move(callback).Run(reporting::Status::StatusOK());
          });
  manager->SetReportQueueForTest(std::move(report_queue));
}

}  // namespace data_controls
