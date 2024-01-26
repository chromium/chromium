// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/device_attester.h"

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_device_trust_key_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace enterprise_connectors {

namespace {

constexpr char kFakeDeviceId[] = "fake_device_id";
constexpr char kDmToken[] = "fake-dm-token";
constexpr char kInvalidDmToken[] = "INVALID_DM_TOKEN";
constexpr char kFakeCustomerId[] = "fake_obfuscated_customer_id";
constexpr char kFakeChallengeResponse[] = "fake_challenge_response";

}  // namespace

class DeviceAttesterTest : public testing::Test {
 protected:
  DeviceAttesterTest()
      : device_attester_(&mock_key_manager_,
                         &fake_dm_token_storage_,
                         &mock_browser_cloud_policy_store_) {
    fake_dm_token_storage_.SetDMToken(kDmToken);
    fake_dm_token_storage_.SetClientId(kFakeDeviceId);
    test_key_pair_ =
        persistence_delegate_factory_.CreateKeyPersistenceDelegate()
            ->LoadKeyPair(KeyStorageType::kPermanent, nullptr);
    levels_.insert(DTCPolicyLevel::kBrowser);
  }

  void SetFakeBrowserPolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_obfuscated_customer_id(kFakeCustomerId);
    mock_browser_cloud_policy_store_.set_policy_data_for_testing(
        std::move(policy_data));
  }

  void SetupPubkeyExport(bool can_export_pubkey = true) {
    EXPECT_CALL(mock_key_manager_, ExportPublicKeyAsync(_))
        .WillOnce(Invoke(
            [&, can_export_pubkey](
                base::OnceCallback<void(std::optional<std::string>)> callback) {
              if (can_export_pubkey) {
                auto public_key_info =
                    test_key_pair_->key()->GetSubjectPublicKeyInfo();
                std::string public_key(public_key_info.begin(),
                                       public_key_info.end());
                public_key_ = public_key;
                std::move(callback).Run(public_key);
              } else {
                std::move(callback).Run(std::nullopt);
              }
            }));
  }

  void SetupSignature(bool can_sign = true) {
    EXPECT_CALL(mock_key_manager_, SignStringAsync(_, _))
        .WillOnce(Invoke(
            [&, can_sign](const std::string& str,
                          base::OnceCallback<void(
                              std::optional<std::vector<uint8_t>>)> callback) {
              if (can_sign) {
                signature = test_key_pair_->key()->SignSlowly(
                    base::as_bytes(base::make_span(str)));
                std::move(callback).Run(signature);
              } else {
                std::move(callback).Run(std::nullopt);
              }
            }));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  test::ScopedKeyPersistenceDelegateFactory persistence_delegate_factory_;
  scoped_refptr<SigningKeyPair> test_key_pair_;
  std::optional<std::vector<uint8_t>> signature;
  std::string public_key_;
  testing::StrictMock<test::MockDeviceTrustKeyManager> mock_key_manager_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  policy::MockCloudPolicyStore mock_browser_cloud_policy_store_;
  DeviceAttester device_attester_;
  base::test::TestFuture<void> future_;
  KeyInfo key_info_;
  SignedData signed_data_;
  base::RunLoop run_loop_;
  std::set<DTCPolicyLevel> levels_;
};

// Tests that the correct device details are added when the device attester
// builds the key info.
TEST_F(DeviceAttesterTest, DecorateKeyInfo_Success) {
  SetFakeBrowserPolicyData();
  SetupPubkeyExport();

  device_attester_.DecorateKeyInfo(levels_, key_info_, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_EQ(key_info_.dm_token(), kDmToken);
  EXPECT_EQ(key_info_.device_id(), kFakeDeviceId);
  EXPECT_EQ(key_info_.customer_id(), kFakeCustomerId);
  EXPECT_EQ(key_info_.browser_instance_public_key(), public_key_);
}

// Tests that the correct device details are added when the device attester
// attempts to build the key info with an invalid DM token.
TEST_F(DeviceAttesterTest, DecorateKeyInfo_InvalidDmToken) {
  fake_dm_token_storage_.SetDMToken(kInvalidDmToken);
  SetFakeBrowserPolicyData();
  SetupPubkeyExport();

  device_attester_.DecorateKeyInfo(levels_, key_info_, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(key_info_.has_dm_token());
  EXPECT_EQ(key_info_.device_id(), kFakeDeviceId);
  EXPECT_EQ(key_info_.customer_id(), kFakeCustomerId);
  EXPECT_EQ(key_info_.browser_instance_public_key(), public_key_);
}

// Tests that the correct device details are added when the device attester
// attempts to build the key info with an empty DM token.
TEST_F(DeviceAttesterTest, DecorateKeyInfo_EmptyDmToken) {
  fake_dm_token_storage_.SetDMToken(std::string());
  SetFakeBrowserPolicyData();
  SetupPubkeyExport();

  device_attester_.DecorateKeyInfo(levels_, key_info_, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(key_info_.has_dm_token());
  EXPECT_EQ(key_info_.device_id(), kFakeDeviceId);
  EXPECT_EQ(key_info_.customer_id(), kFakeCustomerId);
  EXPECT_EQ(key_info_.browser_instance_public_key(), public_key_);
}

// Tests that the correct device details are added when the device ID details
// are missing.
TEST_F(DeviceAttesterTest, DecorateKeyInfo_MissingDeviceID) {
  fake_dm_token_storage_.SetClientId(std::string());
  SetFakeBrowserPolicyData();
  SetupPubkeyExport();

  device_attester_.DecorateKeyInfo(levels_, key_info_, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(key_info_.has_dm_token());
  EXPECT_EQ(key_info_.device_id(), "");
  EXPECT_EQ(key_info_.customer_id(), kFakeCustomerId);
  EXPECT_EQ(key_info_.browser_instance_public_key(), public_key_);
}

// Tests that the correct device details are added when the customer ID details
// are missing.
TEST_F(DeviceAttesterTest, DecorateKeyInfo_NoBrowserCustomerId) {
  SetupPubkeyExport();

  device_attester_.DecorateKeyInfo(levels_, key_info_, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(key_info_.has_customer_id());
  EXPECT_EQ(key_info_.dm_token(), kDmToken);
  EXPECT_EQ(key_info_.device_id(), kFakeDeviceId);
  EXPECT_EQ(key_info_.browser_instance_public_key(), public_key_);
}

// Tests that the correct device details are added when a failure occurred
// exporting the public key.
TEST_F(DeviceAttesterTest, DecorateKeyInfo_FailedPublicKeyExport) {
  SetFakeBrowserPolicyData();
  SetupPubkeyExport(/*can_export_pubkey=*/false);

  device_attester_.DecorateKeyInfo(levels_, key_info_, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(key_info_.has_browser_instance_public_key());
  EXPECT_EQ(key_info_.dm_token(), kDmToken);
  EXPECT_EQ(key_info_.device_id(), kFakeDeviceId);
  EXPECT_EQ(key_info_.customer_id(), kFakeCustomerId);
}

// Tests that the correct device details are added when a failure occurred
// exporting the public key.
TEST_F(DeviceAttesterTest, DecorateKeyInfo_MissingBrowserPolicyLevel) {
  SetFakeBrowserPolicyData();
  EXPECT_CALL(mock_key_manager_, ExportPublicKeyAsync(_)).Times(0);
  auto empty_policy_level = std::set<DTCPolicyLevel>();

  device_attester_.DecorateKeyInfo(empty_policy_level, key_info_,
                                   run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(key_info_.has_browser_instance_public_key());
  EXPECT_FALSE(key_info_.has_dm_token());
  EXPECT_FALSE(key_info_.has_device_id());
  EXPECT_FALSE(key_info_.has_customer_id());
}

// Tests that no policy data is added when the browser cloud policy store is
// null.
TEST_F(DeviceAttesterTest, DecorateKeyInfo_MissingBrowserCloudPolicyStore) {
  SetupPubkeyExport();

  DeviceAttester(&mock_key_manager_, &fake_dm_token_storage_, nullptr)
      .DecorateKeyInfo(levels_, key_info_, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(key_info_.has_customer_id());
  EXPECT_EQ(key_info_.dm_token(), kDmToken);
  EXPECT_EQ(key_info_.device_id(), kFakeDeviceId);
  EXPECT_EQ(key_info_.browser_instance_public_key(), public_key_);
}

// Tests when a browser level signature is added to the signed data.
TEST_F(DeviceAttesterTest, SignResponse_Success) {
  SetupSignature();

  device_attester_.SignResponse(levels_, kFakeChallengeResponse, signed_data_,
                                run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(signed_data_.has_signature());
}

// Tests when no browser level signature is added to the signed data.
TEST_F(DeviceAttesterTest, SignResponse_NoSignature) {
  SetupSignature(/*can_sign=*/false);

  device_attester_.SignResponse(levels_, kFakeChallengeResponse, signed_data_,
                                run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(signed_data_.has_signature());
}

// Tests that no browser level signature is added to the signed data when the
// browser level is not in the policy level set.
TEST_F(DeviceAttesterTest, SignResponse_MissingBrowserPolicyLevel) {
  EXPECT_CALL(mock_key_manager_, SignStringAsync(_, _)).Times(0);
  auto empty_policy_level = std::set<DTCPolicyLevel>();

  device_attester_.SignResponse(empty_policy_level, kFakeChallengeResponse,
                                signed_data_, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(signed_data_.has_signature());
}

}  // namespace enterprise_connectors
