// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/file_type_policies_component_installer.h"
#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

TEST(FileTypePoliciesComponentInstallerTest, VerifyAttributes) {
  FileTypePoliciesComponentInstallerPolicy installer_policy;
  update_client::InstallerAttributes attributes;
  // Feature disabled
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(safe_browsing::kFileTypePoliciesTag);

    attributes = installer_policy.GetInstallerAttributes();
    EXPECT_EQ(attributes["tag"], "default");
  }

  // Feature enabled
  {
    base::test::ScopedFeatureList feature_list;
    base::test::FeatureRefAndParams feature_params(
        safe_browsing::kFileTypePoliciesTag, {{"policy_omaha_tag", "46"}});
    feature_list.InitWithFeaturesAndParameters({feature_params}, {});

    attributes = installer_policy.GetInstallerAttributes();
    EXPECT_EQ(attributes["tag"], "46");
  }
}

}  // namespace component_updater
