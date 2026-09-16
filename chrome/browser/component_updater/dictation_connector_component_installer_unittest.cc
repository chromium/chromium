// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/dictation_connector_component_installer.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class DictationConnectorComponentInstallerTest : public ::testing::Test {
 public:
  DictationConnectorComponentInstallerTest() {
    scoped_feature_list_.InitAndEnableFeature(dictation::kDictation);
  }

 protected:
  void SetUp() override {
    DictationConnectorComponentInstallerPolicy::ResetForTesting();
  }

  void TearDown() override {
    DictationConnectorComponentInstallerPolicy::ResetForTesting();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DictationConnectorComponentInstallerTest,
       GetExtensionDirectoryAlwaysAsync) {
  DictationConnectorComponentInstallerPolicy policy;
  base::FilePath expected_dir(
      FILE_PATH_LITERAL("/path/to/dictation/connector"));

  // This sets the directory.
  policy.ComponentReady(base::Version("1.0.0.0"), expected_dir,
                        base::DictValue());

  base::test::TestFuture<const base::FilePath&> future;
  base::CallbackListSubscription subscription =
      DictationConnectorComponentInstallerPolicy::GetExtensionDirectory(
          future.GetCallback());

  // The callback should not be run synchronously.
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), expected_dir);
}

TEST_F(DictationConnectorComponentInstallerTest, GetExtensionDirectory) {
  DictationConnectorComponentInstallerPolicy policy;
  base::FilePath expected_dir(
      FILE_PATH_LITERAL("/path/to/dictation/connector"));

  base::test::TestFuture<const base::FilePath&> future;
  base::CallbackListSubscription subscription =
      DictationConnectorComponentInstallerPolicy::GetExtensionDirectory(
          future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  policy.ComponentReady(base::Version("1.0.0.0"), expected_dir,
                        base::DictValue());

  // The callback should still not be run synchronously after ComponentReady.
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), expected_dir);
}

TEST_F(DictationConnectorComponentInstallerTest,
       HashMatchesExpectedExtensionId) {
  std::vector<uint8_t> hash;
  DictationConnectorComponentInstallerPolicy().GetHash(&hash);
  EXPECT_EQ(crx_file::id_util::GenerateIdFromHash(hash),
            extension_misc::kDictationConnectorExtensionId);
}

TEST_F(DictationConnectorComponentInstallerTest,
       GetInstallerAttributesDefaultEmpty) {
  DictationConnectorComponentInstallerPolicy policy;
  EXPECT_TRUE(policy.GetInstallerAttributes().empty());
}

TEST_F(DictationConnectorComponentInstallerTest,
       GetInstallerAttributesFromFeatureParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      dictation::kDictation,
      {{dictation::kDictationConnectorTag.name, "canary"}});
  DictationConnectorComponentInstallerPolicy policy;
  update_client::InstallerAttributes expected = {{"connector_tag", "canary"}};
  EXPECT_EQ(policy.GetInstallerAttributes(), expected);
}

}  // namespace component_updater
