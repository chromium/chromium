// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_shim.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace android_webview {
namespace {
constexpr uint8_t kSha256Hash[] = {
    0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec, 0x8e, 0xd5, 0xfa,
    0x54, 0xb0, 0xd2, 0xdd, 0xa5, 0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47,
    0xf6, 0xc4, 0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

class MockComponentInstallerPolicy
    : public component_updater::ComponentInstallerPolicy {
 public:
  MockComponentInstallerPolicy() {
    ON_CALL(*this, GetHash).WillByDefault([](std::vector<uint8_t>* hash) {
      hash->assign(std::begin(kSha256Hash), std::end(kSha256Hash));
    });
  }

  ~MockComponentInstallerPolicy() override = default;

  MockComponentInstallerPolicy(const MockComponentInstallerPolicy&) = delete;
  MockComponentInstallerPolicy& operator=(const MockComponentInstallerPolicy&) =
      delete;

  MOCK_METHOD2(OnCustomInstall,
               update_client::CrxInstaller::Result(const base::Value::Dict&,
                                                   const base::FilePath&));
  MOCK_METHOD0(OnCustomUninstall, void());
  MOCK_METHOD3(ComponentReady,
               void(const base::Version&,
                    const base::FilePath&,
                    base::Value::Dict));
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
};

}  // namespace

class AwComponentInstallerPolicyShimTest : public testing::Test {
 public:
  AwComponentInstallerPolicyShimTest() = default;
  ~AwComponentInstallerPolicyShimTest() override = default;

  AwComponentInstallerPolicyShimTest(
      const AwComponentInstallerPolicyShimTest&) = delete;
  void operator=(const AwComponentInstallerPolicyShimTest&) = delete;
};

TEST_F(AwComponentInstallerPolicyShimTest, TestDelegatedFunctions) {
  auto mock_policy = std::make_unique<MockComponentInstallerPolicy>();
  MockComponentInstallerPolicy* mock_policy_ptr = mock_policy.get();
  auto shim =
      std::make_unique<AwComponentInstallerPolicyShim>(std::move(mock_policy));
  std::vector<uint8_t> hash;

  EXPECT_CALL(*mock_policy_ptr, OnCustomUninstall()).Times(0);
  EXPECT_CALL(*mock_policy_ptr, ComponentReady(_, _, _)).Times(0);
  EXPECT_CALL(*mock_policy_ptr, OnCustomInstall(_, _)).Times(1);
  EXPECT_CALL(*mock_policy_ptr, VerifyInstallation(_, _)).Times(1);
  EXPECT_CALL(*mock_policy_ptr, SupportsGroupPolicyEnabledComponentUpdates())
      .Times(1);
  EXPECT_CALL(*mock_policy_ptr, RequiresNetworkEncryption()).Times(1);
  EXPECT_CALL(*mock_policy_ptr, GetRelativeInstallDir()).Times(1);
  EXPECT_CALL(*mock_policy_ptr, GetName()).Times(1);
  EXPECT_CALL(*mock_policy_ptr, GetInstallerAttributes()).Times(1);
  EXPECT_CALL(*mock_policy_ptr, GetHash).Times(testing::AtLeast(1));

  shim->OnCustomInstall(base::Value::Dict(), base::FilePath());
  shim->VerifyInstallation(base::Value::Dict(), base::FilePath());
  shim->SupportsGroupPolicyEnabledComponentUpdates();
  shim->RequiresNetworkEncryption();
  shim->GetRelativeInstallDir();
  shim->GetName();
  shim->GetInstallerAttributes();
  shim->GetHash(&hash);
}

}  // namespace android_webview
