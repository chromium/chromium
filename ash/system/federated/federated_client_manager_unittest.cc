// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/federated/federated_client_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/federated/public/cpp/fake_service_connection.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::federated {

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
    manager_ = new FederatedClientManager;
  }

 protected:
  raw_ptr<FederatedClientManager> manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(b/289140140): Add tests after implementation is written.
TEST_F(FederatedClientManagerTest, PlaceholderTest) {
  EXPECT_FALSE(manager_->IsServiceAvailable());
}

}  // namespace ash::federated
