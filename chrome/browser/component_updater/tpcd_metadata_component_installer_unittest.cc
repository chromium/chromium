// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/tpcd_metadata_component_installer.h"

#include <memory>

#include "components/component_updater/installer_policies/tpcd_metadata_component_installer_policy.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
using ::testing::_;
}  // namespace

class TpcdMetadataComponentInstallerTest : public ::testing::Test {
 public:
  TpcdMetadataComponentInstallerTest() = default;

  ~TpcdMetadataComponentInstallerTest() override = default;

  content::BrowserTaskEnvironment& task_env() { return task_env_; }

 private:
  content::BrowserTaskEnvironment task_env_;
};

TEST_F(TpcdMetadataComponentInstallerTest, ComponentRegistered) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(_)).Times(1);
  RegisterTpcdMetadataComponent(service.get());

  task_env().RunUntilIdle();
}

}  // namespace component_updater
