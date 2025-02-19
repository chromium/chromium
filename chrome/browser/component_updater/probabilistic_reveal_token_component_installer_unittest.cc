// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/probabilistic_reveal_token_component_installer.h"

#include "base/test/task_environment.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class ProbabilisticRevealTokenComponentInstallerTest : public ::testing::Test {
 public:
  ProbabilisticRevealTokenComponentInstallerTest() = default;

  void RunUntilIdle() { env_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment env_;
  MockComponentUpdateService cus_;
};

TEST_F(ProbabilisticRevealTokenComponentInstallerTest, ComponentRegistration) {
  EXPECT_CALL(cus_, RegisterComponent(testing::_)).Times(1);
  RegisterProbabilisticRevealTokenComponent(&cus_);
  RunUntilIdle();
}

}  // namespace component_updater
