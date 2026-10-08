// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {

using InstalledAsset =
    optimization_guide::ManifestAssetManagerDelegate::InstalledAsset;

TEST(OptimizationGuideOnDeviceModelInstallerTest, GetInstalledAssets) {
  content::BrowserTaskEnvironment task_environment;
  base::ScopedPathOverride path_override(DIR_COMPONENT_USER);
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_COMPONENT_USER, &install_dir));

  const std::string kManifestModelKey =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  const std::string kAmbiguousManifestModelKey =
      "1111111111111111111111111111111111111111111111111111111111111111";
  const std::string kEmptyManifestModelKey =
      "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
  const std::string kLegacyBaseModelKey =
      "5ab6799b9cd59e4f9cbe1f4a80f5529074ea873af991002643860336a6388663";

  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideOnDeviceModel"))
          .AppendASCII("2025.8.8.1141")));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII(kManifestModelKey)
          .AppendASCII("1.0.0.0")));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII(kAmbiguousManifestModelKey)
          .AppendASCII("1.0.0.0")));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII(kAmbiguousManifestModelKey)
          .AppendASCII("1.2.0.0")));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII(kEmptyManifestModelKey)));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII("invalid_public_key")
          .AppendASCII("1.0.0.0")));

  auto delegate = CreateManifestAssetManagerDelegate();
  base::test::TestFuture<std::vector<InstalledAsset>> future;
  delegate->GetInstalledAssets(future.GetCallback());
  EXPECT_THAT(
      future.Get(),
      testing::UnorderedElementsAre(
          InstalledAsset{kLegacyBaseModelKey, base::Version("2025.8.8.1141")},
          InstalledAsset{kManifestModelKey, base::Version("1.0.0.0")},
          InstalledAsset{kAmbiguousManifestModelKey, std::nullopt},
          InstalledAsset{kEmptyManifestModelKey, std::nullopt}));
}

}  // namespace
}  // namespace component_updater
