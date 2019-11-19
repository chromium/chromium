// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/await_match_status_change_checker.h"

#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

AwaitMatchStatusChangeChecker::AwaitMatchStatusChangeChecker(
    const ExitConditionCallback& condition,
    const std::string& debug_message)
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()),
      condition_(condition),
      debug_message_(debug_message) {
}

AwaitMatchStatusChangeChecker::~AwaitMatchStatusChangeChecker() {
}

bool AwaitMatchStatusChangeChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for: " + debug_message_;
  return condition_.Run();
}
