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
#include "chrome/browser/chromeos/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

// The tests in this file mostly focus on verifying that KeystoreService can
// forward messages to and from PlatformKeysService and correctly re-encode
// arguments in both directions.

namespace crosapi {
namespace {

using base::test::RunOnceCallback;
using chromeos::platform_keys::BuildMockPlatformKeysService;
using chromeos::platform_keys::HashAlgorithm;
using chromeos::platform_keys::MockPlatformKeysService;
using chromeos::platform_keys::Status;
using chromeos::platform_keys::TokenId;
using crosapi::keystore_service_util::MakeEcKeystoreSigningAlgorithm;
using crosapi::keystore_service_util::MakeRsaKeystoreSigningAlgorithm;
using testing::_;
using testing::StrictMock;

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
  ASSERT_EQ(result->which(), mojom::KeystoreBinaryResult::Tag::kBlob);
  EXPECT_EQ(result->get_blob(), expected_blob);
}

void AssertErrorEq(const mojom::KeystoreBinaryResultPtr& result,
                   mojom::KeystoreError expected_error) {
  ASSERT_TRUE(result);
  ASSERT_EQ(result->which(), mojom::KeystoreBinaryResult::Tag::kError);
  EXPECT_EQ(result->get_error(), expected_error);
}

class KeystoreServiceAshTest : public testing::Test {
 public:
  KeystoreServiceAshTest() : keystore_service_(&platform_keys_service_) {}
  KeystoreServiceAshTest(const KeystoreServiceAshTest&) = delete;
  auto operator=(const KeystoreServiceAshTest&) = delete;
  ~KeystoreServiceAshTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<MockPlatformKeysService> platform_keys_service_;
  KeystoreServiceAsh keystore_service_;
};

// A mock for observing binary results returned via a callback.
struct BinaryCallbackObserver {
  MOCK_METHOD(void, Callback, (mojom::KeystoreBinaryResultPtr result));

  auto GetCallback() {
    EXPECT_CALL(*this, Callback).WillOnce(MoveArg<0>(&result));
    return base::BindOnce(&BinaryCallbackObserver::Callback,
                          base::Unretained(this));
  }

  mojom::KeystoreBinaryResultPtr result;
};

TEST_F(KeystoreServiceAshTest, GenerateUserRsaKeySuccess) {
  const unsigned int modulus_length = 2048;

  EXPECT_CALL(platform_keys_service_,
              GenerateRSAKey(TokenId::kUser, modulus_length, /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(GetPublicKeyStr(), Status::kSuccess));
  BinaryCallbackObserver observer;
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

}  // namespace
}  // namespace crosapi
