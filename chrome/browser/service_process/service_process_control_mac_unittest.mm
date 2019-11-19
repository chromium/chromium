// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <launch.h>

#include "base/mac/foundation_util.h"
#include "base/mac/launchd.h"
#include "base/mac/scoped_nsobject.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/common/mac/service_management.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ServiceProcessControlMac, TestJobSubmitRemove) {
  NSString* label_ns = @"com.chromium.ServiceProcessStateFileManipulationTest";
  std::string label(label_ns.UTF8String);

  // If the job is loaded or running, remove it.
  pid_t pid = base::mac::PIDForJob(label);
  if (pid >= 0)
    ASSERT_TRUE(mac::services::RemoveJob(label));

  // The job should not be loaded or running.
  pid = base::mac::PIDForJob(label);
  EXPECT_LT(pid, 0);

  // Submit a new job.
  mac::services::JobOptions options;
  options.label = label;
  options.executable_path = "/bin/sh";
  options.arguments = {"sh", "-c", "sleep 10; echo TestJobSubmitRemove"};
  options.run_at_load = true;
  options.auto_launch = false;
  ASSERT_TRUE(mac::services::SubmitJob(options));

  // The new job should be running.
  pid = base::mac::PIDForJob(label);
  EXPECT_GT(pid, 0);

  // Remove the job.
  ASSERT_TRUE(mac::services::RemoveJob(label));

  // Wait for the job to be killed.
  base::TimeDelta timeout_in_ms = TestTimeouts::action_timeout();
  base::Time start_time = base::Time::Now();
  while (1) {
    pid = base::mac::PIDForJob(label);
    if (pid < 0)
      break;

    base::Time current_time = base::Time::Now();
    if (current_time - start_time > timeout_in_ms)
      break;
  }

  EXPECT_LT(pid, 0);

  // Attempting to remove the job again should fail.
  EXPECT_FALSE(mac::services::RemoveJob(label));
}
