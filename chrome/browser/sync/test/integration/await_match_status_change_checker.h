// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_AWAIT_MATCH_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_AWAIT_MATCH_STATUS_CHANGE_CHECKER_H_

#include <iosfwd>

#include "base/functional/callback.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

// Helper class used in the datatype specific AwaitAllModelsMatch
// implementations. |conditon| should be a datatype specific condition that
// verifies the state of all the profiles involved in the test.
class AwaitMatchStatusChangeChecker : public MultiClientStatusChangeChecker {
 public:
  // The std::ostream allows the callback to output debug messages.
  using ExitConditionCallback = base::RepeatingCallback<bool(std::ostream*)>;

  explicit AwaitMatchStatusChangeChecker(
      const ExitConditionCallback& condition);
  ~AwaitMatchStatusChangeChecker() override;

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const ExitConditionCallback condition_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_AWAIT_MATCH_STATUS_CHANGE_CHECKER_H_
