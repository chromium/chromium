// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_USER_PERFORMANCE_TUNING_MANAGER_ENVIRONMENT_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_USER_PERFORMANCE_TUNING_MANAGER_ENVIRONMENT_H_

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

class PrefService;

namespace performance_manager::user_tuning {

class TestUserPerformanceTuningManagerEnvironment {
 public:
  TestUserPerformanceTuningManagerEnvironment();
  ~TestUserPerformanceTuningManagerEnvironment();

  void SetUp(PrefService* local_state);
  void TearDown();

 private:
  bool throttling_enabled_ = false;
  std::unique_ptr<UserPerformanceTuningManager> manager_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_USER_PERFORMANCE_TUNING_MANAGER_ENVIRONMENT_H_
