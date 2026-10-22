// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/private_verification_tokens_installer.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class PrivateVerificationTokensInstallerTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment env_;
  base::test::ScopedFeatureList scoped_feature_list_;
  MockComponentUpdateService mock_update_service_;
};

TEST_F(PrivateVerificationTokensInstallerTest, RegisterIfFeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnablePrivateVerificationTokens);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_update_service_, RegisterComponent(testing::_))
      .WillOnce(testing::DoAll([&run_loop]() { run_loop.Quit(); },
                               testing::Return(true)));

  RegisterPrivateVerificationTokensComponentIfEnabled(&mock_update_service_);
  run_loop.Run();
}

TEST_F(PrivateVerificationTokensInstallerTest, DoNotRegisterIfFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kEnablePrivateVerificationTokens);

  EXPECT_CALL(mock_update_service_, RegisterComponent(testing::_)).Times(0);

  RegisterPrivateVerificationTokensComponentIfEnabled(&mock_update_service_);

  // Use a TestFuture as a sentinel to ensure any tasks posted to the UI thread
  // (which would be a bug in the disabled case) have a chance to run and
  // be caught by the EXPECT_CALL above before the test finishes.
  base::test::TestFuture<void> sentinel;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, sentinel.GetCallback());
  EXPECT_TRUE(sentinel.Wait());
}

}  // namespace component_updater
