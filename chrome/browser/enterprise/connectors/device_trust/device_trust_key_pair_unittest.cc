// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_key_pair.h"

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::Value origins[]{base::Value("example1.example.com"),
                            base::Value("example2.example.com")};

}  // namespace

namespace policy {

class DeviceTrustKeyPairTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    OSCryptMocker::SetUp();

    prefs()->SetUserPref(
        enterprise_connectors::kContextAwareAccessSignalsAllowlistPref,
        std::make_unique<base::ListValue>(origins));
  }

  void TearDown() override {
    OSCryptMocker::TearDown();
    testing::Test::TearDown();
  }

  TestingPrefServiceSimple* GetTestingLocalState();
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }
  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
};

TEST_F(DeviceTrustKeyPairTest, ExportPrivateKey) {
  policy::DeviceTrustKeyPair key(profile());

  std::string private_key = key.ExportPEMPrivateKey();
  EXPECT_TRUE(base::StartsWith(private_key, "-----BEGIN PRIVATE KEY-----\n"));
  EXPECT_TRUE(base::EndsWith(private_key, "-----END PRIVATE KEY-----\n"));
}

TEST_F(DeviceTrustKeyPairTest, ExportPublicKey) {
  policy::DeviceTrustKeyPair key(profile());

  std::string public_key = key.ExportPEMPublicKey();
  EXPECT_TRUE(base::StartsWith(public_key, "-----BEGIN PUBLIC KEY-----\n"));
  EXPECT_TRUE(base::EndsWith(public_key, "-----END PUBLIC KEY-----\n"));
}

}  // namespace policy
