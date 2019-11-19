// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_AWAIT_MATCH_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_AWAIT_MATCH_STATUS_CHANGE_CHECKER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

// Helper class used in the datatype specific AwaitAllModelsMatch
// implementations. |conditon| should be a datatype specific condition that
// verifies the state of all the profiles involved in the test.
class AwaitMatchStatusChangeChecker : public MultiClientStatusChangeChecker {
 public:
  using ExitConditionCallback = base::Callback<bool(void)>;

  AwaitMatchStatusChangeChecker(const ExitConditionCallback& condition,
                                const std::string& debug_message);
  ~AwaitMatchStatusChangeChecker() override;

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  ExitConditionCallback condition_;
  std::string debug_message_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_AWAIT_MATCH_STATUS_CHANGE_CHECKER_H_
