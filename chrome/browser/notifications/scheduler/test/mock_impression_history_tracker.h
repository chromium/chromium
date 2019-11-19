// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_IMPRESSION_HISTORY_TRACKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_IMPRESSION_HISTORY_TRACKER_H_

#include <map>
#include <string>

#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace notifications {
namespace test {

class MockImpressionHistoryTracker : public ImpressionHistoryTracker {
 public:
  MockImpressionHistoryTracker();
  ~MockImpressionHistoryTracker() override;

  MOCK_METHOD1(Init, void(ImpressionHistoryTracker::InitCallback));
  MOCK_METHOD5(AddImpression,
               void(SchedulerClientType,
                    const std::string&,
                    const Impression::ImpressionResultMap&,
                    const Impression::CustomData&,
                    const base::Optional<base::TimeDelta>&));
  MOCK_METHOD0(AnalyzeImpressionHistory, void());
  MOCK_CONST_METHOD1(GetClientStates,
                     void(std::map<SchedulerClientType, const ClientState*>*));
  MOCK_CONST_METHOD1(GetImpression, Impression*(const std::string&));
  MOCK_METHOD2(GetImpressionDetail,
               void(SchedulerClientType,
                    ImpressionDetail::ImpressionDetailCallback));
  MOCK_METHOD1(OnUserAction, void(const UserActionData&));
};

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_IMPRESSION_HISTORY_TRACKER_H_
