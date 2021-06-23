// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/keystore_service_ash.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/mock_key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gtest/include/gtest/gtest.h"

// The tests in this file mostly focus on verifying that KeystoreService can
// forward messages to and from PlatformKeysService and correctly re-encode
// arguments in both directions.

namespace crosapi {
namespace {

using base::test::RunOnceCallback;
using chromeos::platform_keys::BuildMockPlatformKeysService;
using chromeos::platform_keys::HashAlgorithm;
using chromeos::platform_keys::MockKeyPermissionsService;
using chromeos::platform_keys::MockPlatformKeysService;
using chromeos::platform_keys::Status;
using chromeos::platform_keys::TokenId;
using crosapi::keystore_service_util::MakeEcKeystoreSigningAlgorithm;
using crosapi::keystore_service_util::MakeRsaKeystoreSigningAlgorithm;
using testing::_;
using testing::DoAll;
using testing::StrictMock;
using testing::WithArg;

constexpr char kData[] = "\1\2\3\4\5\6\7";

std::string Base64Decode(const char* input) {
  std::string result;
  CHECK(base::Base64Decode(input, &result));
  return result;
}

std::string GetSubjectPublicKeyInfo(
    const scoped_refptr<net::X509Certificate> certificate) {
  base::StringPiece spki_der_piece;
  bool ok = net::asn1::ExtractSPKIFromDERCert(
      net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
      &spki_der_piece);
  CHECK(ok && !spki_der_piece.empty());
  return std::string(spki_der_piece);
}

// Returns a list with one certificate.
std::unique_ptr<net::CertificateList> GetCertificateList() {
  static base::NoDestructor<net::CertificateList> cert_list;
  if (cert_list->empty()) {
    net::SSLInfo ssl_info = net::SSLInfo();
    ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    CHECK(ssl_info.is_valid());
    cert_list->push_back(ssl_info.cert);
  }
  return std::make_unique<net::CertificateList>(*cert_list);
}

const std::string& GetPublicKeyStr() {
  static base::NoDestructor<const std::string> result(
      GetSubjectPublicKeyInfo(*GetCertificateList()->begin()));
  return *result;
}

const std::vector<uint8_t>& GetPublicKeyBin() {
  static base::NoDestructor<const std::vector<uint8_t>> result(
      GetPublicKeyStr().begin(), GetPublicKeyStr().end());
  return *result;
}

const std::string& GetDataStr() {
  static base::NoDestructor<const std::string> result(kData);
  return *result;
}

const std::vector<uint8_t>& GetDataBin() {
  static base::NoDestructor<const std::vector<uint8_t>> result(
      GetDataStr().begin(), GetDataStr().end());
  return *result;
}

void AssertBlobEq(const mojom::KeystoreBinaryResultPtr& result,
                  const std::vector<uint8_t>& expected_blob) {
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_blob());
  EXPECT_EQ(result->get_blob(), expected_blob);
}

template <typename T>
void AssertErrorEq(const T& result, mojom::KeystoreError expected_error) {
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), expected_error);
}

class KeystoreServiceAshTest : public testing::Test {
 public:
  KeystoreServiceAshTest()
      : keystore_service_(&platform_keys_service_, &key_permissions_service_) {}
  KeystoreServiceAshTest(const KeystoreServiceAshTest&) = delete;
  auto operator=(const KeystoreServiceAshTest&) = delete;
  ~KeystoreServiceAshTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<MockPlatformKeysService> platform_keys_service_;
  StrictMock<MockKeyPermissionsService> key_permissions_service_;
  KeystoreServiceAsh keystore_service_;
};

// A mock for observing callbacks that return a singe result of the type |T| and
// saving it.
template <typename T>
struct CallbackObserver {
  MOCK_METHOD(void, Callback, (T result));

  auto GetCallback() {
    EXPECT_CALL(*this, Callback).WillOnce(MoveArg<0>(&result));
    return base::BindOnce(&CallbackObserver<T>::Callback,
                          base::Unretained(this));
  }

  T result;
};

using BinaryCallbackObserver = CallbackObserver<mojom::KeystoreBinaryResultPtr>;
using SelectCertsCallbackObserver =
    CallbackObserver<mojom::KeystoreSelectClientCertificatesResultPtr>;

// A mock for observing status results returned via a callback.
struct StatusCallbackObserver {
  MOCK_METHOD(void, Callback, (bool is_error, mojom::KeystoreError error));

  auto GetCallback() {
    EXPECT_CALL(*this, Callback)
        .WillOnce(
            DoAll(MoveArg<0>(&result_is_error), MoveArg<1>(&result_error)));
    return base::BindOnce(&StatusCallbackObserver::Callback,
                          base::Unretained(this));
  }

  bool result_is_error = false;
  mojom::KeystoreError result_error = mojom::KeystoreError::kUnknown;
};

TEST_F(KeystoreServiceAshTest, GenerateUserRsaKeySuccess) {
  const unsigned int modulus_length = 2048;

  EXPECT_CALL(platform_keys_service_,
              GenerateRSAKey(TokenId::kUser, modulus_length, /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(GetPublicKeyStr(), Status::kSuccess));
  CallbackObserver<mojom::KeystoreBinaryResultPtr> observer;
  keystore_service_.GenerateKey(mojom::KeystoreType::kUser,
                                MakeRsaKeystoreSigningAlgorithm(modulus_length),
                                observer.GetCallback());

  AssertBlobEq(observer.result, GetPublicKeyBin());
}

TEST_F(KeystoreServiceAshTest, GenerateDeviceEcKeySuccess) {
  const std::string named_curve = "test_named_curve";

  EXPECT_CALL(platform_keys_service_,
              GenerateECKey(TokenId::kSystem, named_curve, /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(GetPublicKeyStr(), Status::kSuccess));

  BinaryCallbackObserver observer;
  keystore_service_.GenerateKey(mojom::KeystoreType::kDevice,
                                MakeEcKeystoreSigningAlgorithm(named_curve),
                                observer.GetCallback());

  AssertBlobEq(observer.result, GetPublicKeyBin());
}

TEST_F(KeystoreServiceAshTest, GenerateKeyFail) {
  EXPECT_CALL(platform_keys_service_, GenerateECKey)
      .WillOnce(RunOnceCallback<2>("", Status::kErrorInternal));

  BinaryCallbackObserver observer;
  keystore_service_.GenerateKey(mojom::KeystoreType::kUser,
                                MakeEcKeystoreSigningAlgorithm("named_curve_1"),
                                observer.GetCallback());

  AssertErrorEq(observer.result, mojom::KeystoreError::kInternal);
}

TEST_F(KeystoreServiceAshTest, SignRsaSuccess) {
  // Accepted and returned data are the same. This is not realistic, but doesn't
  // matter here.
  EXPECT_CALL(platform_keys_service_,
              SignRSAPKCS1Digest(absl::optional<TokenId>(TokenId::kUser),
                                 GetDataStr(), GetPublicKeyStr(),
                                 HashAlgorithm::HASH_ALGORITHM_SHA256,
                                 /*callback=*/_))
      .WillOnce(RunOnceCallback<4>(GetDataStr(), Status::kSuccess));

  BinaryCallbackObserver observer;
  keystore_service_.Sign(
      /*is_keystore_provided=*/true, mojom::KeystoreType::kUser,
      GetPublicKeyBin(), mojom::KeystoreSigningScheme::kRsassaPkcs1V15Sha256,
      GetDataBin(), observer.GetCallback());

  AssertBlobEq(observer.result, GetDataBin());
}

TEST_F(KeystoreServiceAshTest, SignEcSuccess) {
  // Accepted and returned data are the same. This is not realistic, but doesn't
  // matter here.
  EXPECT_CALL(
      platform_keys_service_,
      SignECDSADigest(absl::optional<TokenId>(TokenId::kSystem), GetDataStr(),
                      GetPublicKeyStr(), HashAlgorithm::HASH_ALGORITHM_SHA512,
                      /*callback=*/_))
      .WillOnce(RunOnceCallback<4>(GetDataStr(), Status::kSuccess));

  BinaryCallbackObserver observer;
  keystore_service_.Sign(
      /*is_keystore_provided=*/true, mojom::KeystoreType::kDevice,
      GetPublicKeyBin(), mojom::KeystoreSigningScheme::kEcdsaSha512,
      GetDataBin(), observer.GetCallback());

  AssertBlobEq(observer.result, GetDataBin());
}

TEST_F(KeystoreServiceAshTest, SignFail) {
  EXPECT_CALL(platform_keys_service_, SignECDSADigest)
      .WillOnce(RunOnceCallback<4>("", Status::kErrorKeyNotAllowedForSigning));

  BinaryCallbackObserver observer;
  keystore_service_.Sign(
      /*is_keystore_provided=*/true, mojom::KeystoreType::kDevice,
      GetPublicKeyBin(), mojom::KeystoreSigningScheme::kEcdsaSha512,
      GetDataBin(), observer.GetCallback());

  AssertErrorEq(observer.result,
                mojom::KeystoreError::kKeyNotAllowedForSigning);
}

TEST_F(KeystoreServiceAshTest, RemoveKeySuccess) {
  EXPECT_CALL(platform_keys_service_,
              RemoveKey(TokenId::kSystem, GetPublicKeyStr(), /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(Status::kSuccess));

  StatusCallbackObserver observer;
  keystore_service_.RemoveKey(mojom::KeystoreType::kDevice, GetPublicKeyBin(),
                              observer.GetCallback());

  EXPECT_EQ(observer.result_is_error, false);
}

TEST_F(KeystoreServiceAshTest, RemoveKeyFail) {
  EXPECT_CALL(platform_keys_service_,
              RemoveKey(TokenId::kSystem, GetPublicKeyStr(), /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(Status::kErrorKeyNotFound));

  StatusCallbackObserver observer;
  keystore_service_.RemoveKey(mojom::KeystoreType::kDevice, GetPublicKeyBin(),
                              observer.GetCallback());

  EXPECT_EQ(observer.result_is_error, true);
  EXPECT_EQ(observer.result_error, mojom::KeystoreError::kKeyNotFound);
}

TEST_F(KeystoreServiceAshTest, SelectClientCertificatesSuccess) {
  std::vector<std::vector<uint8_t>> cert_authorities_bin = {
      {1, 2, 3}, {2, 3, 4}, {3, 4, 5}};
  std::vector<std::string> cert_authorities_str = {"\1\2\3", "\2\3\4",
                                                   "\3\4\5"};

  EXPECT_CALL(platform_keys_service_,
              SelectClientCertificates(cert_authorities_str,
                                       /*callback=*/_))
      .WillOnce(WithArg<1>([](auto callback) {
        std::move(callback).Run(GetCertificateList(), Status::kSuccess);
      }));

  SelectCertsCallbackObserver observer;
  keystore_service_.SelectClientCertificates(cert_authorities_bin,
                                             observer.GetCallback());

  ASSERT_TRUE(observer.result);
  EXPECT_EQ(observer.result->which(),
            mojom::KeystoreSelectClientCertificatesResult::Tag::kCertificates);
  EXPECT_EQ(observer.result->get_certificates().size(), 1);

  // Check that the cert can be converted back to the original one.
  auto orig_cert_list = GetCertificateList();
  const std::vector<uint8_t>& received_binary_cert =
      observer.result->get_certificates()[0];
  scoped_refptr<net::X509Certificate> received_cert =
      net::X509Certificate::CreateFromBytes(received_binary_cert);
  EXPECT_TRUE(
      orig_cert_list->front()->EqualsIncludingChain(received_cert.get()));
}

TEST_F(KeystoreServiceAshTest, SelectClientCertificatesFail) {
  EXPECT_CALL(platform_keys_service_, SelectClientCertificates)
      .WillOnce(WithArg<1>([](auto callback) {
        std::move(callback).Run({}, Status::kErrorInternal);
      }));

  SelectCertsCallbackObserver observer;
  keystore_service_.SelectClientCertificates({}, observer.GetCallback());

  AssertErrorEq(observer.result, mojom::KeystoreError::kInternal);
}

TEST_F(KeystoreServiceAshTest, GetKeyTagsSuccess) {
  EXPECT_CALL(key_permissions_service_,
              IsCorporateKey(GetPublicKeyStr(), /*callback=*/_))
      .WillOnce(
          RunOnceCallback<1>(absl::optional<bool>(true), Status::kSuccess));

  CallbackObserver<mojom::GetKeyTagsResultPtr> observer;
  keystore_service_.GetKeyTags(GetPublicKeyBin(), observer.GetCallback());

  ASSERT_TRUE(observer.result);
  ASSERT_EQ(observer.result->which(), mojom::GetKeyTagsResult::Tag::kTags);
  EXPECT_EQ(observer.result->get_tags(),
            static_cast<uint64_t>(mojom::KeyTag::kCorporate));
}

TEST_F(KeystoreServiceAshTest, GetKeyTagsFail) {
  EXPECT_CALL(key_permissions_service_, IsCorporateKey)
      .WillOnce(RunOnceCallback<1>(absl::nullopt, Status::kErrorInternal));

  CallbackObserver<mojom::GetKeyTagsResultPtr> observer;
  keystore_service_.GetKeyTags(GetPublicKeyBin(), observer.GetCallback());

  AssertErrorEq(observer.result, mojom::KeystoreError::kInternal);
}

TEST_F(KeystoreServiceAshTest, AddKeyTagsSuccess) {
  const uint64_t tags = static_cast<uint64_t>(mojom::KeyTag::kCorporate);

  EXPECT_CALL(key_permissions_service_,
              SetCorporateKey(GetPublicKeyStr(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(Status::kSuccess));

  StatusCallbackObserver observer;
  keystore_service_.AddKeyTags(GetPublicKeyBin(), tags, observer.GetCallback());

  EXPECT_EQ(observer.result_is_error, false);
}

TEST_F(KeystoreServiceAshTest, AddKeyTagsFail) {
  const uint64_t tags = static_cast<uint64_t>(mojom::KeyTag::kCorporate);

  EXPECT_CALL(key_permissions_service_,
              SetCorporateKey(GetPublicKeyStr(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(Status::kErrorInternal));

  StatusCallbackObserver observer;
  keystore_service_.AddKeyTags(GetPublicKeyBin(), tags, observer.GetCallback());

  EXPECT_EQ(observer.result_is_error, true);
  EXPECT_EQ(observer.result_error, mojom::KeystoreError::kInternal);
}

TEST_F(KeystoreServiceAshTest, CanUserGrantPermissionForKey) {
  EXPECT_CALL(key_permissions_service_,
              CanUserGrantPermissionForKey(GetPublicKeyStr(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(false));

  CallbackObserver<bool> observer;
  keystore_service_.CanUserGrantPermissionForKey(GetPublicKeyBin(),
                                                 observer.GetCallback());

  EXPECT_EQ(observer.result, false);
}

}  // namespace
}  // namespace crosapi
