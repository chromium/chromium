// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/real_time_url_checks_allowlist_component_installer.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/safe_browsing/android/real_time_url_checks_allowlist.h"
#include "components/update_client/update_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace component_updater {

class MockRealTimeUrlChecksAllowlist
    : public safe_browsing::RealTimeUrlChecksAllowlist {
 public:
  MockRealTimeUrlChecksAllowlist() = default;
  MockRealTimeUrlChecksAllowlist(const MockRealTimeUrlChecksAllowlist&) =
      delete;
  MockRealTimeUrlChecksAllowlist& operator=(
      const MockRealTimeUrlChecksAllowlist&) = delete;

  MOCK_METHOD1(PopulateFromDynamicUpdate, void(const std::string& binary_pb));

  ~MockRealTimeUrlChecksAllowlist() override = default;
};

class RealTimeUrlChecksAllowlistComponentInstallerTest
    : public ::testing::Test {
 public:
  RealTimeUrlChecksAllowlistComponentInstallerTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
  }

  void LoadRealTimeUrlChecksAllowlist() {
    auto mock_realtime_allowlist = StrictMock<MockRealTimeUrlChecksAllowlist>();
    safe_browsing::RealTimeUrlChecksAllowlist::SetInstanceForTesting(
        &mock_realtime_allowlist);

    // Calling ComponentReady should trigger PopulateFromDynamicUpdate call
    EXPECT_CALL(mock_realtime_allowlist, PopulateFromDynamicUpdate(_)).Times(1);
    policy_->ComponentReady(base::Version(), component_install_dir_.GetPath(),
                            base::Value::Dict());

    env_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&mock_realtime_allowlist);
  }

 protected:
  base::test::TaskEnvironment env_;

 private:
  base::ScopedTempDir component_install_dir_;
  std::unique_ptr<RealTimeUrlChecksAllowlistComponentInstallerPolicy> policy_ =
      std::make_unique<RealTimeUrlChecksAllowlistComponentInstallerPolicy>();
};

TEST_F(RealTimeUrlChecksAllowlistComponentInstallerTest, VerifyRegistration) {
  // Calling RegisterRealTimeUrlChecksAllowlistComponent should trigger
  // RegisterComponent call
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  EXPECT_CALL(*service, RegisterComponent(_)).WillOnce(testing::Return(true));
  RegisterRealTimeUrlChecksAllowlistComponent(service.get());

  ASSERT_NO_FATAL_FAILURE(LoadRealTimeUrlChecksAllowlist());
}

}  // namespace component_updater
