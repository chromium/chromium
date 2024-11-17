// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/open_cookie_database_component_installer.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/common/content_features.h"

namespace component_updater {

class OpenCookieDatabaseComponentInstallerTest : public ::testing::Test {
 public:
  OpenCookieDatabaseComponentInstallerTest() = default;

  void RunUntilIdle() { env_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment env_;
  MockComponentUpdateService cus_;
};

TEST_F(OpenCookieDatabaseComponentInstallerTest,
       ComponentRegistrationWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kDevToolsPrivacyUI);

  EXPECT_CALL(cus_, RegisterComponent(testing::_)).Times(0);
  RegisterOpenCookieDatabaseComponent(&cus_);
  RunUntilIdle();
}

TEST_F(OpenCookieDatabaseComponentInstallerTest,
       ComponentRegistrationWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kDevToolsPrivacyUI);

  EXPECT_CALL(cus_, RegisterComponent(testing::_)).Times(1);
  RegisterOpenCookieDatabaseComponent(&cus_);
  RunUntilIdle();
}

}  // namespace component_updater
                                 