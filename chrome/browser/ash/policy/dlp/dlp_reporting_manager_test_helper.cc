// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager_test_helper.h"

#include <memory>

#include "chrome/browser/ash/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "components/reporting/client/mock_report_queue.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::Mock;

namespace policy {

class DlpPolicyEventMatcher : public MatcherInterface<const DlpPolicyEvent&> {
 public:
  explicit DlpPolicyEventMatcher(const DlpPolicyEvent& event)
      : source_url_(event.source().url()),
        destination_url_(event.destination().url()),
        destination_component_(event.destination().component()) {}

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

    return source_url_equals && destination_url_equals &&
           destination_component_equals;
  }

  void DescribeTo(::std::ostream* os) const override {}

 private:
  const std::string source_url_;
  const std::string destination_url_;
  const int destination_component_;
};

Matcher<const DlpPolicyEvent&> IsDlpPolicyEvent(const DlpPolicyEvent& event) {
  return Matcher<const DlpPolicyEvent&>(new DlpPolicyEventMatcher(event));
}

void SetReportQueueForReportingManager(policy::DlpReportingManager* manager,
                                       std::vector<DlpPolicyEvent>& events) {
  auto report_queue = std::make_unique<reporting::MockReportQueue>();
  EXPECT_CALL(*report_queue.get(), AddRecord)
      .WillRepeatedly(
          [&events](base::StringPiece record, reporting::Priority priority,
                    reporting::ReportQueue::EnqueueCallback callback) {
            DlpPolicyEvent event;
            event.ParseFromString(std::string(record));
            // Don't use this code in a multithreaded env as it can cause
            // concurrency issues with the events in the vector.
            events.push_back(event);
            std::move(callback).Run(reporting::Status::StatusOK());
          });
  manager->GetReportQueueSetter().Run(std::move(report_queue));
}

}  // namespace policy
