// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy.h"

#include <stdint.h>

#include <iterator>
#include <memory>
#include <utility>

#include "android_webview/common/aw_paths.h"
#include "base/android/path_utils.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "base/version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {

constexpr char kComponentId[] = "jebgalgnebhfojomionfpkfelancnnkf";
// This hash corresponds to kComponentId.
constexpr uint8_t kSha256Hash[] = {
    0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec, 0x8e, 0xd5, 0xfa,
    0x54, 0xb0, 0xd2, 0xdd, 0xa5, 0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47,
    0xf6, 0xc4, 0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

base::Value::Dict test_manifest(const base::Version& version) {
  base::Value::Dict manifest;
  manifest.Set("version", version.GetString());
  return manifest;
}

void CreateTestFiles(const base::FilePath& install_dir) {
  base::CreateDirectory(install_dir);
  ASSERT_TRUE(base::WriteFile(install_dir.AppendASCII("file1.txt"), "1"));
  ASSERT_TRUE(base::WriteFile(install_dir.AppendASCII("file2.txt"), "2"));
  ASSERT_TRUE(base::CreateDirectory(install_dir.AppendASCII("sub_dir")));
  ASSERT_TRUE(base::WriteFile(
      install_dir.AppendASCII("sub_dir").AppendASCII("file3.txt"), "3"));
}

void AssertTestFiles(const base::FilePath& install_dir) {
  EXPECT_TRUE(base::PathExists(install_dir.AppendASCII("file1.txt")));
  EXPECT_TRUE(base::PathExists(install_dir.AppendASCII("file2.txt")));
  EXPECT_TRUE(base::DirectoryExists(install_dir.AppendASCII("sub_dir")));
  EXPECT_TRUE(base::PathExists(
      install_dir.AppendASCII("sub_dir").AppendASCII("file3.txt")));
}

}  // namespace

class TestAwComponentInstallerPolicy : public AwComponentInstallerPolicy {
 public:
  TestAwComponentInstallerPolicy() {
    ON_CALL(*this, GetHash).WillByDefault([](std::vector<uint8_t>* hash) {
      hash->assign(std::begin(kSha256Hash), std::end(kSha256Hash));
    });
  }
  ~TestAwComponentInstallerPolicy() override = default;

  TestAwComponentInstallerPolicy(const TestAwComponentInstallerPolicy&) =
      delete;
  TestAwComponentInstallerPolicy& operator=(
      const TestAwComponentInstallerPolicy&) = delete;

  MOCK_METHOD2(OnCustomInstall,
               update_client::CrxInstaller::Result(const base::Value::Dict&,
                                                   const base::FilePath&));
  MOCK_CONST_METHOD2(VerifyInstallation,
                     bool(const base::Value::Dict& manifest,
                          const base::FilePath& dir));
  MOCK_CONST_METHOD0(SupportsGroupPolicyEnabledComponentUpdates, bool());
  MOCK_CONST_METHOD0(RequiresNetworkEncryption, bool());
  MOCK_CONST_METHOD0(GetRelativeInstallDir, base::FilePath());
  MOCK_CONST_METHOD0(GetName, std::string());
  MOCK_CONST_METHOD0(GetInstallerAttributes,
                     update_client::InstallerAttributes());
  MOCK_CONST_METHOD1(GetHash, void(std::vector<uint8_t>*));

 private:
  void IncrementComponentsUpdatedCount() override { /* noop */
  }
};

class AwComponentInstallerPolicyTest : public testing::Test {
 public:
  AwComponentInstallerPolicyTest() = default;
  ~AwComponentInstallerPolicyTest() override = default;

  AwComponentInstallerPolicyTest(const AwComponentInstallerPolicyTest&) =
      delete;
  AwComponentInstallerPolicyTest& operator=(
      const AwComponentInstallerPolicyTest&) = delete;

  // Override from testing::Test
  void SetUp() override {
    scoped_path_override_ =
        std::make_unique<base::ScopedPathOverride>(DIR_COMPONENTS_TEMP);

    ASSERT_TRUE(base::android::GetDataDirectory(&cps_component_path_));
    cps_component_path_ = cps_component_path_.AppendASCII("components")
                              .AppendASCII("cps")
                              .AppendASCII(kComponentId);

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    CreateTestFiles(GetTestInstallPath());

    delegate_ = std::make_unique<TestAwComponentInstallerPolicy>();
  }

  void TearDown() override {
    ASSERT_TRUE(base::DeletePathRecursively(cps_component_path_));
  }

  base::FilePath GetTestInstallPath() const {
    return scoped_temp_dir_.GetPath();
  }

 protected:
  base::FilePath cps_component_path_;
  std::unique_ptr<TestAwComponentInstallerPolicy> delegate_;

 private:
  base::ScopedTempDir scoped_temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> scoped_path_override_;
};

TEST_F(AwComponentInstallerPolicyTest, TestNoExistingVersions) {
  const base::Version testVersion("1.2.3.4");

  delegate_->ComponentReady(testVersion, GetTestInstallPath(),
                            test_manifest(testVersion));

  // Check that the original install path still has files.
  AssertTestFiles(GetTestInstallPath());

  AssertTestFiles(
      cps_component_path_.AppendASCII("1_" + testVersion.GetString()));
}

TEST_F(AwComponentInstallerPolicyTest, TestExistingOtherVersions) {
  const base::Version testVersion("1.2.3.4");

  CreateTestFiles(cps_component_path_.AppendASCII("1_4.3.2.1"));
  CreateTestFiles(cps_component_path_.AppendASCII("10_2.3.4.1"));

  delegate_->ComponentReady(testVersion, GetTestInstallPath(),
                            test_manifest(testVersion));

  // Check that the original install path still has files.
  AssertTestFiles(GetTestInstallPath());

  AssertTestFiles(
      cps_component_path_.AppendASCII("11_" + testVersion.GetString()));
}

TEST_F(AwComponentInstallerPolicyTest, TestExistingSameVersion) {
  const base::Version testVersion("1.2.3.4");

  CreateTestFiles(
      cps_component_path_.AppendASCII("5_" + testVersion.GetString()));

  delegate_->ComponentReady(testVersion, GetTestInstallPath(),
                            test_manifest(testVersion));

  // Check that the original install path still has files.
  AssertTestFiles(GetTestInstallPath());

  // Directory should only contain "<component-id>/5/1.2.3.4/", no other files
  // or directories should exist.
  base::FileEnumerator file_enumerator(
      cps_component_path_, /* recursive= */ false,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    EXPECT_EQ(path.BaseName().MaybeAsASCII(), "5_" + testVersion.GetString());
    AssertTestFiles(path);
  }
}

}  // namespace android_webview
