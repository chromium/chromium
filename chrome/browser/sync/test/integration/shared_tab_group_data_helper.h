// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SHARED_TAB_GROUP_DATA_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SHARED_TAB_GROUP_DATA_HELPER_H_

#include <vector>

#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tab_groups {

MATCHER_P2(HasSpecificsSharedTabGroup, title, color, "") {
  return arg.tab_group().title() == title && arg.tab_group().color() == color;
}

MATCHER_P2(HasSpecificsSharedTab, title, url, "") {
  return arg.tab().title() == title && arg.tab().url() == url;
}

// A helper class that waits for the SAVED_TAB_GROUP entities on the FakeServer
// to match a given GMock matcher.
class ServerSharedTabGroupMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher =
      testing::Matcher<std::vector<sync_pb::SharedTabGroupDataSpecifics>>;

  explicit ServerSharedTabGroupMatchChecker(const Matcher& matcher);
  ~ServerSharedTabGroupMatchChecker() override;

  // fake_server::FakeServerMatchStatusChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SHARED_TAB_GROUP_DATA_HELPER_H_
