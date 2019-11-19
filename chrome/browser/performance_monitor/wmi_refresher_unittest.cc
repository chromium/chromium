// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/wmi_refresher.h"

#include <cmath>
#include <limits>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {
namespace win {

namespace {

using WMIRefresherTest = testing::Test;

}  // namespace

// This test assume that the WMI service is available in all supported version
// of Windows.
//
// TODO(https://crbug.com/956638): Investigate why the initialization of WMI
// might fail in some situations and reenable this test.
TEST_F(WMIRefresherTest, DISABLED_EndToEnd) {
  base::test::TaskEnvironment env;
  PostTask(FROM_HERE, base::BindOnce([] {
             // The WMIRefresher objects have to live on a sequence with the
             // MayBlock trait.
             base::ScopedAllowBlockingForTesting allow_blocking;

             win::WMIRefresher wmi_refresher;

             // NOTE: This call can take a few seconds to run as it has to wait
             // for the WMI service to be initialized. In theory this is
             // something that could take a long time as it's depending on a
             // Windows service, in practice this takes between 5 and 10 seconds
             // at most. If this test fails then the system call will probably
             // have to be mocked, which isn't ideal as this'll mean that the
             // real system call won't be tested.
             EXPECT_TRUE(wmi_refresher.InitializeDiskIdleTimeConfig());

             base::Optional<float> disk_idle_time;
             while (!disk_idle_time)
               disk_idle_time =
                   wmi_refresher.RefreshAndGetDiskIdleTimeInPercent();

             // Sanity check the data.
             EXPECT_GE(disk_idle_time.value(), 0.0f);
             EXPECT_LE(disk_idle_time.value(), 1.0f);
           }));
  env.RunUntilIdle();
}

}  // namespace win
}  // namespace performance_monitor
