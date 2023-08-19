// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/rootfs_lacros_loader.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

// Copied from rootfs_lacros_loader.cc
constexpr char kLacrosMounterUpstartJob[] = "lacros_2dmounter";

class RootfsLacrosLoaderTest : public testing::Test {
 public:
  RootfsLacrosLoaderTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    metadata_path_ = temp_dir_.GetPath().Append("metadata");
    base::WriteFile(metadata_path_,
                    "{\"content\":{\"version\":\"" + version_str + "\"}}");
    rootfs_lacros_loader_ = std::make_unique<RootfsLacrosLoader>(
        &fake_upstart_client_, metadata_path_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  const std::string version_str = "1.0.0";
  base::ScopedTempDir temp_dir_;
  base::FilePath metadata_path_;

  user_manager::ScopedUserManager scoped_user_manager_{
      std::make_unique<user_manager::FakeUserManager>()};
  ash::FakeUpstartClient fake_upstart_client_;
  std::unique_ptr<RootfsLacrosLoader> rootfs_lacros_loader_;
};

TEST_F(RootfsLacrosLoaderTest, LoadRootfsLacros) {
  bool callback_called = false;
  fake_upstart_client_.set_start_job_cb(base::BindRepeating(
      [](bool* callback_called, const std::string& job,
         const std::vector<std::string>& upstart_env) {
        EXPECT_EQ(job, kLacrosMounterUpstartJob);
        *callback_called = true;
        return ash::FakeUpstartClient::StartJobResult(true /* success */);
      },
      &callback_called));

  base::test::TestFuture<base::Version, const base::FilePath&> future;
  rootfs_lacros_loader_->Load(
      future.GetCallback<base::Version, const base::FilePath&>());
  EXPECT_EQ(base::Version(version_str), future.Get<0>());
  EXPECT_TRUE(callback_called);
}

}  // namespace
}  // namespace crosapi
