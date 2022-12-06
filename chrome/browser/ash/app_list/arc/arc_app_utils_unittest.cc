// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/components/arc/test/fake_app_instance.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void GetAndroidIdBlocking(bool* out_ok, int64_t* out_android_id) {
  base::RunLoop run_loop;
  arc::GetAndroidId(base::BindOnce(
      [](base::OnceClosure closure, bool* out_ok, int64_t* out_android_id,
         bool ok, int64_t result) {
        *out_ok = ok;
        *out_android_id = result;
        std::move(closure).Run();
      },
      run_loop.QuitClosure(), out_ok, out_android_id));
  run_loop.Run();
}

}  // namespace

using ArcAppUtilsTest = testing::Test;

// Tests that IsArcItem does not crash or DCHECK with invalid crx file ids.
TEST_F(ArcAppUtilsTest, IsArcItemDoesNotCrashWithInvalidCrxFileIds) {
  // TestingProfile checks CurrentlyOn(cotnent::BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment;
  TestingProfile testing_profile;
  EXPECT_FALSE(arc::IsArcItem(&testing_profile, std::string()));
  EXPECT_FALSE(arc::IsArcItem(&testing_profile, "ShelfWindowWatcher0"));
}

TEST_F(ArcAppUtilsTest, GetAndroidId) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile testing_profile;

  // ARC++ is not running.
  bool ok = true;
  int64_t android_id = -1;
  GetAndroidIdBlocking(&ok, &android_id);
  EXPECT_FALSE(ok);
  EXPECT_EQ(0, android_id);

  ArcAppTest arc_app_test_;
  arc_app_test_.SetUp(&testing_profile);

  constexpr int64_t kAndroidIdForTest = 1000;
  arc_app_test_.app_instance()->set_android_id(kAndroidIdForTest);
  GetAndroidIdBlocking(&ok, &android_id);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kAndroidIdForTest, android_id);

  arc_app_test_.TearDown();
}
