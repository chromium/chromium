// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/buildinfo/cpp/fidl.h>
#include <fuchsia/buildinfo/cpp/fidl_test_base.h>

#include "base/bind.h"
#include "base/fuchsia/build_info.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/location.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class FakeBuildInfoProvider
    : public fuchsia::buildinfo::testing::Provider_TestBase {
 public:
  FakeBuildInfoProvider(const std::string& version,
                        TestComponentContextForProcess& component_context)
      : version_(version),
        binding_(component_context.additional_services(), this) {}
  FakeBuildInfoProvider(const FakeBuildInfoProvider&) = delete;
  FakeBuildInfoProvider& operator=(const FakeBuildInfoProvider&) = delete;
  ~FakeBuildInfoProvider() override = default;

  // fuchsia::buildinfo::testing::Provider_TestBase implementation
  void GetBuildInfo(GetBuildInfoCallback callback) override {
    fuchsia::buildinfo::BuildInfo build_info;
    build_info.set_version(version_);
    callback(std::move(build_info));
  }
  void NotImplemented_(const std::string& name) final {
    ADD_FAILURE() << "Unexpected call: " << name;
  }

 private:
  std::string version_;
  ScopedServiceBinding<fuchsia::buildinfo::Provider> binding_;
};

}  // namespace

// Uses a fake "fuchsia.buildinfo.Provider" implementation.
// clears the cached BuildInfo to ensure that each test starts with no cached
// BuildInfo and that subsequent tests runs do not use fake values.
class BuildInfoTest : public testing::Test {
 protected:
  BuildInfoTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO),
        thread_("Helper thread") {
    thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
  }

  void SetUp() override { ClearCachedBuildInfoForTesting(); }
  void TearDown() override { ClearCachedBuildInfoForTesting(); }

  void FetchBuildInfoAndWaitUntilCached() {
    base::RunLoop run_loop;
    thread_.task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                      FetchAndCacheSystemBuildInfo();
                                      run_loop.Quit();
                                    }));
    run_loop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestComponentContextForProcess component_context_;
  base::Thread thread_;
};

TEST_F(BuildInfoTest, GetBuildInfoVersion) {
  FakeBuildInfoProvider build_info_provider("test.version string",
                                            component_context_);
  FetchBuildInfoAndWaitUntilCached();

  EXPECT_EQ(GetCachedBuildInfo().version(), "test.version string");
  EXPECT_EQ(GetBuildInfoVersion(), "test.version string");
}

// Ensures that when FetchAndCacheSystemBuildInfo() has not been called in the
// process that a  DCHECK fires to alert the developer.
TEST_F(BuildInfoTest, FetchAndCacheSystemBuildInfoNotCalled) {
  bool has_version = false;  // False because EXPECT_FALSE is always executed.
  EXPECT_DCHECK_DEATH_WITH(
      { has_version = GetCachedBuildInfo().has_version(); },
      "FetchAndCacheSystemBuildInfo\\(\\) has not been called in this process");
  EXPECT_FALSE(has_version);

  has_version = false;  // False because EXPECT_FALSE is always executed.
  EXPECT_DCHECK_DEATH_WITH(
      { has_version = !GetBuildInfoVersion().empty(); },
      "FetchAndCacheSystemBuildInfo\\(\\) has not been called in this process");
  EXPECT_FALSE(has_version);
}

TEST(BuildInfoFromSystemServiceTest, ValidValues) {
  ClearCachedBuildInfoForTesting();

  FetchAndCacheSystemBuildInfo();

  EXPECT_TRUE(GetCachedBuildInfo().has_version());
  EXPECT_FALSE(GetCachedBuildInfo().version().empty());

  EXPECT_FALSE(GetBuildInfoVersion().empty());

  ClearCachedBuildInfoForTesting();
}

}  // namespace base
