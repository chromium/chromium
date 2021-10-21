// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"

#include <memory>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;
using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using testing::_;
using testing::ByMove;
using testing::InSequence;
using testing::Invoke;
using testing::Not;
using testing::Return;
using testing::StrictMock;

namespace enterprise_connectors {

using test::MockKeyNetworkDelegate;
using test::MockKeyPersistenceDelegate;

namespace {

const char kNonce[] = "nonce";
const char kDmServerUrl[] = "dmserver.example.com";
const char kDmToken[] = "dm_token";

std::string CreateResponse(BPKUP::ResponseCode response_code = BPKUP::SUCCESS) {
  enterprise_management::DeviceManagementResponse response;
  response.mutable_browser_public_key_upload_response()->set_response_code(
      response_code);
  std::string response_str;
  response.SerializeToString(&response_str);
  return response_str;
}

KeyPersistenceDelegate::KeyInfo CreateEmptyKeyPair() {
  return {BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()};
}

}  // namespace

// Tests a success key rotation flow when a TPM key and TPM key provider are
// available.
TEST(KeyRotationManagerTest, RotateWithAdminRights_Tpm_WithKey) {
  // The factory creates instances backed by fake TPM keys.
  test::ScopedKeyPersistenceDelegateFactory scoped_factory;
  auto mock_persistence_delegate = scoped_factory.CreateMockedDelegate();
  auto original_key_wrapped = scoped_factory.wrapped_key();

  // The mocked delegate is already set-up to return a working TPM key and
  // provider.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider()).Times(2);
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_TPM_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(CreateResponse()));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_TRUE(manager->RotateWithAdminRights(dm_server_url, kDmToken, kNonce));
}

// Tests a success key rotation flow when TPM key provider is available, but no
// previous key was created.
TEST(KeyRotationManagerTest, RotateWithAdminRights_Tpm_NoKey) {
  // The factory creates instances backed by fake TPM keys.
  test::ScopedKeyPersistenceDelegateFactory scoped_factory;
  auto mock_persistence_delegate = scoped_factory.CreateMockedDelegate();

  // The mocked delegate is already set-up to return a working TPM key and
  // provider. Force it to not return a key.
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(CreateEmptyKeyPair()));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_TPM_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(CreateResponse()));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_TRUE(manager->RotateWithAdminRights(dm_server_url, kDmToken, kNonce));
}

// Tests a success key rotation flow when a TPM key provider is not available
// and no key previously existed.
TEST(KeyRotationManagerTest, RotateWithAdminRights_NoTpm_NoKey) {
  auto mock_persistence_delegate =
      std::make_unique<MockKeyPersistenceDelegate>();
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(CreateEmptyKeyPair()));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(CreateResponse()));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_TRUE(manager->RotateWithAdminRights(dm_server_url, kDmToken, kNonce));
}

// Tests a success key rotation flow when a TPM key provider is not available
// and a key previously existed.
TEST(KeyRotationManagerTest, RotateWithAdminRights_NoTpm_WithKey) {
  ECSigningKeyProvider ec_key_provider;
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto key = ec_key_provider.GenerateSigningKeySlowly(acceptable_algorithms);

  auto mock_persistence_delegate =
      std::make_unique<MockKeyPersistenceDelegate>();
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(KeyPersistenceDelegate::KeyInfo(
          BPKUR::CHROME_BROWSER_OS_KEY, key->GetWrappedKey())));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, _))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(CreateResponse()));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_TRUE(manager->RotateWithAdminRights(dm_server_url, kDmToken, kNonce));
}

// Tests a failed key rotation flow when a TPM key provider is not available
// and a key previously existed, but storing the new key locally failed.
TEST(KeyRotationManagerTest, RotateWithAdminRights_NoTpm_WithKey_StoreFailed) {
  ECSigningKeyProvider ec_key_provider;
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto key = ec_key_provider.GenerateSigningKeySlowly(acceptable_algorithms);
  auto original_key_wrapped = key->GetWrappedKey();

  auto mock_persistence_delegate =
      std::make_unique<MockKeyPersistenceDelegate>();
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(KeyPersistenceDelegate::KeyInfo(
          BPKUR::CHROME_BROWSER_OS_KEY, key->GetWrappedKey())));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(false));

  auto mock_network_delegate =
      std::make_unique<testing::StrictMock<MockKeyNetworkDelegate>>();

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  GURL dm_server_url(kDmServerUrl);
  EXPECT_FALSE(manager->RotateWithAdminRights(dm_server_url, kDmToken, kNonce));
}

// Tests a success key rotation flow when a TPM key provider is not available
// and a key previously existed, and the network request transiently failed
// twice before succeeding.
TEST(KeyRotationManagerTest,
     RotateWithAdminRights_NoTpm_WithKey_EventualNetworkSuccess) {
  ECSigningKeyProvider ec_key_provider;
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto key = ec_key_provider.GenerateSigningKeySlowly(acceptable_algorithms);
  auto original_key_wrapped = key->GetWrappedKey();

  auto mock_persistence_delegate =
      std::make_unique<MockKeyPersistenceDelegate>();
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(KeyPersistenceDelegate::KeyInfo(
          BPKUR::CHROME_BROWSER_OS_KEY, key->GetWrappedKey())));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(CreateResponse(BPKUP::UNDEFINED)))
      .WillOnce(Return(CreateResponse(BPKUP::SUCCESS)));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_TRUE(manager->RotateWithAdminRights(dm_server_url, kDmToken, kNonce));
}

// Tests a failed key rotation flow when a TPM key provider is not available
// and a key previously existed, and the network request transiently failed
// twice before really failing. Also, in this case, the original key should be
// stored back.
TEST(KeyRotationManagerTest,
     RotateWithAdminRights_NoTpm_WithKey_EventualNetworkFailure) {
  ECSigningKeyProvider ec_key_provider;
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto key = ec_key_provider.GenerateSigningKeySlowly(acceptable_algorithms);
  auto original_key_wrapped = key->GetWrappedKey();

  InSequence s;

  auto mock_persistence_delegate =
      std::make_unique<MockKeyPersistenceDelegate>();
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(KeyPersistenceDelegate::KeyInfo(
          BPKUR::CHROME_BROWSER_OS_KEY, original_key_wrapped)));
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(
      *mock_persistence_delegate,
      StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, Not(original_key_wrapped)))
      .WillOnce(Return(true));

  GURL dm_server_url(kDmServerUrl);
  auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
  EXPECT_CALL(*mock_network_delegate,
              SendPublicKeyToDmServerSync(dm_server_url, kDmToken, _))
      .WillOnce(Return(CreateResponse(BPKUP::UNDEFINED)))
      .WillOnce(Return(CreateResponse(BPKUP::INVALID_SIGNATURE)));

  EXPECT_CALL(*mock_persistence_delegate,
              StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY, original_key_wrapped))
      .WillOnce(Return(true));

  auto manager = KeyRotationManager::CreateForTesting(
      std::move(mock_network_delegate), std::move(mock_persistence_delegate));

  EXPECT_FALSE(manager->RotateWithAdminRights(dm_server_url, kDmToken, kNonce));
}

}  // namespace enterprise_connectors
