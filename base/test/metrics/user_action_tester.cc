// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/user_action_tester.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/test_simple_task_runner.h"

namespace base {

UserActionTester::UserActionTester()
    : task_runner_(new base::TestSimpleTaskRunner),
      action_callback_(base::BindRepeating(&UserActionTester::OnUserAction,
                                           base::Unretained(this))) {
  base::SetRecordActionTaskRunner(task_runner_);
  base::AddActionCallback(action_callback_);
}

UserActionTester::~UserActionTester() {
  base::RemoveActionCallback(action_callback_);
}

int UserActionTester::GetActionCount(const std::string& user_action) const {
  return times_map_.count(user_action);
}

std::vector<TimeTicks> UserActionTester::GetActionTimes(
    const std::string& user_action) const {
  std::vector<TimeTicks> result;
  auto range = times_map_.equal_range(user_action);
  for (auto it = range.first; it != range.second; it++) {
    result.push_back(it->second);
  }
  return result;
}

void UserActionTester::ResetCounts() {
  times_map_.clear();
}

void UserActionTester::OnUserAction(const std::string& user_action,
                                    TimeTicks action_time) {
  times_map_.insert({user_action, action_time});
}

}  // namespace base
