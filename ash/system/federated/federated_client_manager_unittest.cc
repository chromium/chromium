// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/federated/federated_client_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/federated/federated_service_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/federated/federated_client.h"
#include "chromeos/ash/services/federated/public/cpp/fake_service_connection.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::federated {

// Fake federated service connection is prepared by AshTestHelper.
class FederatedClientManagerTest : public NoSessionAshTestBase {
 public:
  FederatedClientManagerTest()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kFederatedService,
                              features::kFederatedStringsService},
        /*disabled_features=*/{});
  }

  FederatedClientManagerTest(const FederatedClientManagerTest&) = delete;
  FederatedClientManagerTest& operator=(const FederatedClientManagerTest&) =
      delete;

  ~FederatedClientManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    manager_ = std::make_unique<FederatedClientManager>();
  }

  void TearDown() override {
    // This ordering is important.
    manager_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<FederatedClientManager> manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(b/289140140): Add more tests when implementation is completed.

TEST_F(FederatedClientManagerTest, ServicesAvailableAfterLogin) {
  // Before login.
  EXPECT_FALSE(manager_->IsFederatedServiceAvailable());
  EXPECT_FALSE(manager_->IsFederatedStringsServiceAvailable());

  // After login.
  SimulateUserLogin("user@gmail.com");
  EXPECT_TRUE(manager_->IsFederatedServiceAvailable());
  EXPECT_TRUE(manager_->IsFederatedStringsServiceAvailable());
}

// Demonstration of using a faked FederatedServiceController in a non-ash
// test environment, i.e. a low-level unit test which does not inherit from
// AshTestBase.
// TODO(b/289140140): Refactor FederatedClientManager such that the testing
// here can be simpler, considering that this will be an examplar for
// prospective clients.
class FederatedClientManagerFakeAshInteractionTest : public testing::Test {
 public:
  FederatedClientManagerFakeAshInteractionTest()
      : scoped_fake_for_test_(&fake_service_connection_) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kFederatedService,
                              features::kFederatedStringsService},
        /*disabled_features=*/{});
  }
  FederatedClientManagerFakeAshInteractionTest(
      const FederatedClientManagerFakeAshInteractionTest&) = delete;
  FederatedClientManagerFakeAshInteractionTest& operator=(
      const FederatedClientManagerFakeAshInteractionTest&) = delete;

  ~FederatedClientManagerFakeAshInteractionTest() override = default;

  void SetUp() override {
    FederatedClientManager::UseFakeAshInteractionForTest();
    manager_ = std::make_unique<FederatedClientManager>();
  }

  void TearDown() override { manager_.reset(); }

 protected:
  std::unique_ptr<FederatedClientManager> manager_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  ash::federated::FakeServiceConnectionImpl fake_service_connection_;
  ash::federated::ScopedFakeServiceConnectionForTest scoped_fake_for_test_;
};

TEST_F(FederatedClientManagerFakeAshInteractionTest, ServicesAvailable) {
  EXPECT_TRUE(manager_->IsFederatedServiceAvailable());
  EXPECT_TRUE(manager_->IsFederatedStringsServiceAvailable());
}

TEST_F(FederatedClientManagerFakeAshInteractionTest, ReportExample) {
  EXPECT_TRUE(manager_->IsFederatedServiceAvailable());
  EXPECT_EQ(0, manager_->get_num_successful_reports_for_test());

  manager_->ReportSingleString(
      chromeos::federated::mojom::FederatedExampleTableId::UNKNOWN,
      "example_feature_name", "example_string");
  EXPECT_EQ(1, manager_->get_num_successful_reports_for_test());
}

}  // namespace ash::federated
