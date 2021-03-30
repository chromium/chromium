// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_key_pair.h"

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class DeviceTrustKeyPairTest : public testing::Test {
 public:
  DeviceTrustKeyPairTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  void SetUp() override {
    testing::Test::SetUp();
    OSCryptMocker::SetUp();
    key_.Init();
  }

  void TearDown() override {
    OSCryptMocker::TearDown();
    testing::Test::TearDown();
  }

  DeviceTrustKeyPair key_;

 private:
  ScopedTestingLocalState local_state_;
};

TEST_F(DeviceTrustKeyPairTest, ExportPrivateKey) {
  std::string private_key = key_.ExportPEMPrivateKey();
  EXPECT_TRUE(base::StartsWith(private_key, "-----BEGIN PRIVATE KEY-----\n"));
  EXPECT_TRUE(base::EndsWith(private_key, "-----END PRIVATE KEY-----\n"));
}

TEST_F(DeviceTrustKeyPairTest, ExportPublicKey) {
  std::string public_key = key_.ExportPEMPublicKey();
  EXPECT_TRUE(base::StartsWith(public_key, "-----BEGIN PUBLIC KEY-----\n"));
  EXPECT_TRUE(base::EndsWith(public_key, "-----END PUBLIC KEY-----\n"));
}

}  // namespace enterprise_connectors
