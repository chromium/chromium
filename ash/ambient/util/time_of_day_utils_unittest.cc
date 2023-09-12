// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/time_of_day_utils.h"

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::personalization_app {
namespace {

constexpr char kTestRootPath[] = "/test/time_of_day";

class TimeOfDayUtilsTest : public ::testing::Test {
 protected:
  TimeOfDayUtilsTest() {
    dlcservice_client_.set_install_root_path(kTestRootPath);
  }

  base::test::TaskEnvironment task_environment_;
  FakeDlcserviceClient dlcservice_client_;
};

TEST_F(TimeOfDayUtilsTest, InstallSuccess) {
  base::test::TestFuture<base::FilePath> future;
  InstallTimeOfDayDlc(future.GetCallback());
  EXPECT_EQ(future.Get(), base::FilePath(kTestRootPath));
}

TEST_F(TimeOfDayUtilsTest, InstallError) {
  dlcservice_client_.set_install_error(
      "org.chromium.DlcServiceInterface.INTERNAL");
  base::test::TestFuture<base::FilePath> future;
  InstallTimeOfDayDlc(future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

}  // namespace
}  // namespace ash::personalization_app
