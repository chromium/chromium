// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_delegate.h"

#include <stdint.h>
#include <iterator>
#include <memory>
#include <utility>

#include "android_webview/nonembedded/component_updater/aw_component_update_service.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/update_client/update_client.h"
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

constexpr char kTestVersion[] = "123.456.789";

std::unique_ptr<base::DictionaryValue> test_manifest() {
  auto manifest = std::make_unique<base::DictionaryValue>();
  manifest->SetString("version", kTestVersion);
  return manifest;
}

void CreateTestFiles(const base::FilePath& install_dir) {
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

class MockInstallerPolicy : public component_updater::ComponentInstallerPolicy {
 public:
  explicit MockInstallerPolicy(
      std::unique_ptr<AwComponentInstallerPolicyDelegate> delegate)
      : delegate_(std::move(delegate)) {}
  ~MockInstallerPolicy() override = default;

  MockInstallerPolicy(const MockInstallerPolicy&) = delete;
  MockInstallerPolicy& operator=(const MockInstallerPolicy&) = delete;

  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override {
    std::vector<uint8_t> hash;
    GetHash(&hash);
    return delegate_->OnCustomInstall(manifest, install_dir, hash);
  }

  void ComponentReady(
      const base::Version& version,
      const base::FilePath& install_dir,
      std::unique_ptr<base::DictionaryValue> manifest) override {
    delegate_->ComponentReady(version, install_dir, std::move(manifest));
  }

  void OnCustomUninstall() override { delegate_->OnCustomUninstall(); }

  MOCK_CONST_METHOD2(VerifyInstallation,
                     bool(const base::DictionaryValue& manifest,
                          const base::FilePath& dir));
  MOCK_CONST_METHOD0(SupportsGroupPolicyEnabledComponentUpdates, bool());
  MOCK_CONST_METHOD0(RequiresNetworkEncryption, bool());
  MOCK_CONST_METHOD0(GetRelativeInstallDir, base::FilePath());
  MOCK_CONST_METHOD0(GetName, std::string());
  MOCK_CONST_METHOD0(GetInstallerAttributes,
                     update_client::InstallerAttributes());

  void GetHash(std::vector<uint8_t>* hash) const override {
    hash->assign(std::begin(kSha256Hash), std::end(kSha256Hash));
  }

 private:
  std::unique_ptr<AwComponentInstallerPolicyDelegate> delegate_;
};

}  // namespace

class MockAwComponentUpdateService : public AwComponentUpdateService {
 public:
  explicit MockAwComponentUpdateService(bool notify_result)
      : notify_result_(notify_result) {}
  ~MockAwComponentUpdateService() override = default;

  bool NotifyNewVersion(const std::string& component_id,
                        const base::FilePath& install_dir,
                        const base::Version& version) override {
    component_id_ = component_id;
    install_dir_ = install_dir;
    version_ = version;

    // Check that all files are copied to the new install_dir.
    AssertTestFiles(install_dir_);

    return notify_result_;
  }

  std::string GetComponentId() const { return component_id_; }
  base::FilePath GetNewInstallDir() const { return install_dir_; }
  base::Version GetVersion() const { return version_; }

 private:
  const bool notify_result_;

  std::string component_id_;
  base::FilePath install_dir_;
  base::Version version_;
};

class AwComponentInstallerPolicyDelegateTest : public testing::Test {
 public:
  AwComponentInstallerPolicyDelegateTest() = default;
  ~AwComponentInstallerPolicyDelegateTest() override = default;

  AwComponentInstallerPolicyDelegateTest(
      const AwComponentInstallerPolicyDelegateTest&) = delete;
  AwComponentInstallerPolicyDelegateTest& operator=(
      const AwComponentInstallerPolicyDelegateTest&) = delete;

  // Override from testing::Test
  void SetUp() override {
    mock_policy_ = std::make_unique<MockInstallerPolicy>(
        std::make_unique<AwComponentInstallerPolicyDelegate>());
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath GetTestInstallPath() const {
    return scoped_temp_dir_.GetPath();
  }

 protected:
  std::unique_ptr<MockInstallerPolicy> mock_policy_;

 private:
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(AwComponentInstallerPolicyDelegateTest,
       TestSuccessfullNotifyNewVersion) {
  MockAwComponentUpdateService mock_service(/* notify_result= */ true);
  SetAwComponentUpdateServiceForTesting(&mock_service);
  CreateTestFiles(GetTestInstallPath());

  std::unique_ptr<base::DictionaryValue> manifest = test_manifest();
  const int result =
      mock_policy_->OnCustomInstall(*manifest, GetTestInstallPath()).error;
  EXPECT_EQ(result, static_cast<int>(update_client::InstallError::NONE));

  EXPECT_EQ(mock_service.GetVersion().GetString(), kTestVersion);
  EXPECT_EQ(mock_service.GetComponentId(), kComponentId);

  // Check that the original install path still has files.
  AssertTestFiles(GetTestInstallPath());

  // The incoming `install_dir` has to be different from the original one.
  EXPECT_NE(mock_service.GetNewInstallDir(), GetTestInstallPath());
}

TEST_F(AwComponentInstallerPolicyDelegateTest, TestFailedNotifyNewVersion) {
  MockAwComponentUpdateService mock_service(/* notify_result= */ false);
  SetAwComponentUpdateServiceForTesting(&mock_service);
  CreateTestFiles(GetTestInstallPath());

  std::unique_ptr<base::DictionaryValue> manifest = test_manifest();
  const int result =
      mock_policy_->OnCustomInstall(*manifest, GetTestInstallPath()).error;
  EXPECT_EQ(result,
            static_cast<int>(update_client::InstallError::GENERIC_ERROR));
}

}  // namespace android_webview
