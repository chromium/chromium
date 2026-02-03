// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/keystore_service.h"

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_log.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/platform_keys/key_permissions/mock_key_permissions_service.h"
#include "chrome/browser/ash/platform_keys/mock_platform_keys_service.h"
#include "chromeos/ash/components/platform_keys/keystore_service_util.h"
#include "chromeos/ash/components/platform_keys/keystore_types.h"
#include "chromeos/ash/components/platform_keys/platform_keys.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

// The tests in this file mostly focus on verifying that KeystoreService can
// forward messages to and from PlatformKeysService, KeyPermissionsService,
// TpmChallengeKey and correctly re-encode arguments in both directions.

namespace ash {
namespace {

using ::attestation::KEY_TYPE_ECC;
using ::attestation::KEY_TYPE_RSA;
using ::base::test::RunOnceCallback;
using ::chromeos::GetPublicKeySuccessResult;
using ::chromeos::KeystoreAlgorithmName;
using ::chromeos::KeystoreError;
using ::chromeos::KeystoreKeyAttributeType;
using ::chromeos::KeystoreSigningScheme;
using ::chromeos::KeystoreType;
using ::chromeos::keystore_service_util::MakeEcdsaKeystoreAlgorithm;
using ::chromeos::keystore_service_util::MakeRsaOaepKeystoreAlgorithm;
using ::chromeos::keystore_service_util::MakeRsassaPkcs1v15KeystoreAlgorithm;
using ::chromeos::platform_keys::HashAlgorithm;
using ::chromeos::platform_keys::Status;
using ::chromeos::platform_keys::TokenId;
using platform_keys::MockKeyPermissionsService;
using platform_keys::MockPlatformKeysService;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;

constexpr char kData[] = "\1\2\3\4\5\6\7";

#define EXPECT_ERROR_LOG(matcher)                                    \
  if (DLOG_IS_ON(ERROR)) {                                           \
    EXPECT_CALL(log_, Log(logging::LOGGING_ERROR, _, _, _, matcher)) \
        .WillOnce(testing::Return(true)); /* suppress logging */     \
  }

std::string GetSubjectPublicKeyInfo(
    const scoped_refptr<net::X509Certificate>& certificate) {
  std::string_view spki_der_piece;
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

std::vector<uint8_t> CertToBlob(
    const scoped_refptr<net::X509Certificate>& cert) {
  const uint8_t* cert_buffer =
      reinterpret_cast<const uint8_t*>(CRYPTO_BUFFER_data(cert->cert_buffer()));
  return std::vector<uint8_t>(
      cert_buffer,
      UNSAFE_TODO(cert_buffer + CRYPTO_BUFFER_len(cert->cert_buffer())));
}

void AssertBlobEq(const chromeos::KeystoreBinaryResult& result,
                  const std::vector<uint8_t>& expected_blob) {
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_blob);
}

void AssertCertListEq(
    const std::vector<std::vector<uint8_t>>& received_cert_list,
    std::unique_ptr<net::CertificateList> expected_cert_list) {
  ASSERT_EQ(received_cert_list.size(), expected_cert_list->size());
  for (size_t i = 0; i < received_cert_list.size(); ++i) {
    const scoped_refptr<net::X509Certificate>& expected_cert =
        (*expected_cert_list)[i];

    const std::vector<uint8_t>& received_binary_cert = received_cert_list[i];
    scoped_refptr<net::X509Certificate> received_cert =
        net::X509Certificate::CreateFromBytes(received_binary_cert);
    ASSERT_TRUE(received_cert);

    EXPECT_TRUE(expected_cert->EqualsIncludingChain(received_cert.get()));
  }
}

template <typename T>
void AssertErrorEq(const T& result, KeystoreError expected_error) {
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), expected_error);
}

// Matches a certificate of the type `scoped_refptr<net::X509Certificate>`.
MATCHER_P(CertEq, expected_cert, "Certificates don't match.") {
  return expected_cert && arg && expected_cert->EqualsIncludingChain(arg.get());
}

// Matches strings that start with `expected_prefix`.
MATCHER_P(StrStartsWith, expected_prefix, "Unexpected string.") {
  return base::StartsWith(arg, expected_prefix);
}

class KeystoreServiceTest : public testing::Test {
 public:
  KeystoreServiceTest()
      : keystore_service_(&platform_keys_service_, &key_permissions_service_) {}
  KeystoreServiceTest(const KeystoreServiceTest&) = delete;
  auto operator=(const KeystoreServiceTest&) = delete;
  ~KeystoreServiceTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<MockPlatformKeysService> platform_keys_service_;
  StrictMock<MockKeyPermissionsService> key_permissions_service_;
  KeystoreService keystore_service_;
  base::test::MockLog log_;
};

// A mock for observing callbacks that return a single result of the type |T|
// and saving it.
template <typename T>
struct CallbackObserver {
  MOCK_METHOD(void, Callback, (T result));

  auto GetCallback() {
    EXPECT_CALL(*this, Callback).WillOnce(MoveArg<0>(&result));
    return base::BindOnce(&CallbackObserver<T>::Callback,
                          base::Unretained(this));
  }

  std::optional<T> result;
};

// A mock for observing callbacks that return a single result of the type |T| by
// const reference and saving it.
template <typename T>
struct CallbackObserverRef {
  MOCK_METHOD(void, Callback, (const T& result));

  auto GetCallback() {
    EXPECT_CALL(*this, Callback).WillOnce(testing::SaveArg<0>(&result));
    return base::BindOnce(&CallbackObserverRef<T>::Callback,
                          base::Unretained(this));
  }

  std::optional<T> result;
};

// A mock for observing status results returned via a callback.
struct StatusCallbackObserver {
  MOCK_METHOD(void, Callback, (bool is_error, KeystoreError error));

  auto GetCallback() {
    EXPECT_CALL(*this, Callback)
        .WillOnce(
            DoAll(MoveArg<0>(&result_is_error), MoveArg<1>(&result_error)));
    return base::BindOnce(&StatusCallbackObserver::Callback,
                          base::Unretained(this));
  }

  bool has_value() const { return result_is_error.has_value(); }

  std::optional<bool> result_is_error;
  KeystoreError result_error = KeystoreError::kUnknown;
};

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, UserKeystoreRsassaPkcs1v15GenerateKeySuccess) {
  const unsigned int modulus_length = 2048;

  EXPECT_CALL(
      platform_keys_service_,
      GenerateRSAKey(TokenId::kUser, modulus_length, /*sw_backed=*/false,
                     /*callback=*/_))
      .WillOnce(RunOnceCallback<3>(GetPublicKeyBin(), Status::kSuccess));
  CallbackObserver<chromeos::KeystoreBinaryResult> observer;
  keystore_service_.GenerateKey(
      KeystoreType::kUser,
      MakeRsassaPkcs1v15KeystoreAlgorithm(modulus_length, /*sw_backed=*/false),
      observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertBlobEq(observer.result.value(), GetPublicKeyBin());
}

TEST_F(KeystoreServiceTest, UserKeystoreRsaOaepGenerateKeySuccess) {
  const unsigned int modulus_length = 2048;

  EXPECT_CALL(
      platform_keys_service_,
      GenerateRSAKey(TokenId::kUser, modulus_length, /*sw_backed=*/false,
                     /*callback=*/_))
      .WillOnce(RunOnceCallback<3>(GetPublicKeyBin(), Status::kSuccess));
  CallbackObserver<chromeos::KeystoreBinaryResult> observer;
  keystore_service_.GenerateKey(
      KeystoreType::kUser,
      MakeRsaOaepKeystoreAlgorithm(modulus_length, /*sw_backed=*/false),
      observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertBlobEq(observer.result.value(), GetPublicKeyBin());
}

TEST_F(KeystoreServiceTest, DeviceKeystoreEcdsaGenerateKeySuccess) {
  const std::string named_curve = "test_named_curve";

  EXPECT_CALL(platform_keys_service_,
              GenerateECKey(TokenId::kSystem, named_curve, /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(GetPublicKeyBin(), Status::kSuccess));

  CallbackObserver<chromeos::KeystoreBinaryResult> observer;
  keystore_service_.GenerateKey(KeystoreType::kDevice,
                                MakeEcdsaKeystoreAlgorithm(named_curve),
                                observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertBlobEq(observer.result.value(), GetPublicKeyBin());
}

TEST_F(KeystoreServiceTest, DeviceKeystoreRsaAlgoGenerateKeyFail) {
  EXPECT_CALL(platform_keys_service_, GenerateRSAKey)
      .WillOnce(
          RunOnceCallback<3>(std::vector<uint8_t>(), Status::kErrorInternal));

  CallbackObserver<chromeos::KeystoreBinaryResult> observer;
  keystore_service_.GenerateKey(
      KeystoreType::kDevice,
      MakeRsassaPkcs1v15KeystoreAlgorithm(/*modulus_length=*/2048,
                                          /*sw_backed=*/false),
      observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(), KeystoreError::kInternal);
}

TEST_F(KeystoreServiceTest, UserKeystoreEcAlgoGenerateKeyFail) {
  EXPECT_CALL(platform_keys_service_, GenerateECKey)
      .WillOnce(
          RunOnceCallback<2>(std::vector<uint8_t>(), Status::kErrorInternal));

  CallbackObserver<chromeos::KeystoreBinaryResult> observer;
  keystore_service_.GenerateKey(
      KeystoreType::kUser,
      MakeEcdsaKeystoreAlgorithm(/*named_curve=*/"named_curve_1"),
      observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(), KeystoreError::kInternal);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, SignRsaSuccess) {
  // Accepted and returned data are the same. This is not realistic, but doesn't
  // matter here.
  EXPECT_CALL(
      platform_keys_service_,
      SignRsaPkcs1(std::optional<TokenId>(TokenId::kUser), GetDataBin(),
                   GetPublicKeyBin(), HashAlgorithm::HASH_ALGORITHM_SHA256,
                   /*callback=*/_))
      .WillOnce(RunOnceCallback<4>(GetDataBin(), Status::kSuccess));

  CallbackObserver<chromeos::KeystoreBinaryResult> observer;
  keystore_service_.Sign(KeystoreType::kUser, GetPublicKeyBin(),
                         KeystoreSigningScheme::kRsassaPkcs1V15Sha256,
                         GetDataBin(), observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertBlobEq(observer.result.value(), GetDataBin());
}

TEST_F(KeystoreServiceTest, SignEcSuccess) {
  // Accepted and returned data are the same. This is not realistic, but doesn't
  // matter here.
  EXPECT_CALL(platform_keys_service_,
              SignEcdsa(std::optional<TokenId>(TokenId::kSystem), GetDataBin(),
                        GetPublicKeyBin(), HashAlgorithm::HASH_ALGORITHM_SHA512,
                        /*callback=*/_))
      .WillOnce(RunOnceCallback<4>(GetDataBin(), Status::kSuccess));

  CallbackObserver<chromeos::KeystoreBinaryResult> observer;
  keystore_service_.Sign(KeystoreType::kDevice, GetPublicKeyBin(),
                         KeystoreSigningScheme::kEcdsaSha512, GetDataBin(),
                         observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertBlobEq(observer.result.value(), GetDataBin());
}

TEST_F(KeystoreServiceTest, UsingRsassaPkcs1V15NoneSignSuccess) {
  EXPECT_CALL(platform_keys_service_,
              SignRSAPKCS1Raw(std::optional<TokenId>(TokenId::kSystem),
                              GetDataBin(), GetPublicKeyBin(),
                              /*callback=*/_))
      .WillOnce(RunOnceCallback<3>(GetDataBin(), Status::kSuccess));

  KeystoreSigningScheme sign_scheme =
      KeystoreSigningScheme::kRsassaPkcs1V15None;
  CallbackObserver<chromeos::KeystoreBinaryResult> observer;

  keystore_service_.Sign(KeystoreType::kDevice, GetPublicKeyBin(), sign_scheme,
                         GetDataBin(), observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertBlobEq(observer.result.value(), GetDataBin());
}

TEST_F(KeystoreServiceTest, KeyNotAllowedSignFail) {
  EXPECT_CALL(platform_keys_service_, SignEcdsa)
      .WillOnce(RunOnceCallback<4>(std::vector<uint8_t>(),
                                   Status::kErrorKeyNotAllowedForOperation));

  CallbackObserver<chromeos::KeystoreBinaryResult> observer;
  keystore_service_.Sign(KeystoreType::kDevice, GetPublicKeyBin(),
                         KeystoreSigningScheme::kEcdsaSha512, GetDataBin(),
                         observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(),
                KeystoreError::kKeyNotAllowedForOperation);
}

TEST_F(KeystoreServiceTest, UnknownSignSchemeSignFail) {
  CallbackObserver<chromeos::KeystoreBinaryResult> observer;
  KeystoreSigningScheme unknown_sign_scheme = KeystoreSigningScheme::kUnknown;

  keystore_service_.Sign(KeystoreType::kDevice, GetPublicKeyBin(),
                         unknown_sign_scheme, GetDataBin(),
                         observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(),
                KeystoreError::kUnsupportedAlgorithmType);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, RemoveKeySuccess) {
  EXPECT_CALL(platform_keys_service_,
              RemoveKey(TokenId::kSystem, GetPublicKeyBin(), /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(Status::kSuccess));

  StatusCallbackObserver observer;
  keystore_service_.RemoveKey(KeystoreType::kDevice, GetPublicKeyBin(),
                              observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, false);
}

TEST_F(KeystoreServiceTest, RemoveKeyFail) {
  EXPECT_CALL(platform_keys_service_,
              RemoveKey(TokenId::kSystem, GetPublicKeyBin(), /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(Status::kErrorKeyNotFound));

  StatusCallbackObserver observer;
  keystore_service_.RemoveKey(KeystoreType::kDevice, GetPublicKeyBin(),
                              observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, true);
  EXPECT_EQ(observer.result_error, KeystoreError::kKeyNotFound);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, SelectClientCertificatesSuccess) {
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

  CallbackObserver<chromeos::KeystoreSelectClientCertificatesResult> observer;
  keystore_service_.SelectClientCertificates(cert_authorities_bin,
                                             observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  AssertCertListEq(observer.result.value().value(), GetCertificateList());
}

TEST_F(KeystoreServiceTest, SelectClientCertificatesFail) {
  EXPECT_CALL(platform_keys_service_, SelectClientCertificates)
      .WillOnce(WithArg<1>([](auto callback) {
        std::move(callback).Run({}, Status::kErrorInternal);
      }));

  CallbackObserver<chromeos::KeystoreSelectClientCertificatesResult> observer;
  keystore_service_.SelectClientCertificates({}, observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(), KeystoreError::kInternal);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, GetKeyTagsSuccess) {
  EXPECT_CALL(key_permissions_service_,
              IsCorporateKey(GetPublicKeyBin(), /*callback=*/_))
      .WillOnce(
          RunOnceCallback<1>(std::optional<bool>(true), Status::kSuccess));

  CallbackObserver<chromeos::GetKeyTagsResult> observer;
  keystore_service_.GetKeyTags(GetPublicKeyBin(), observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_EQ(observer.result.value().value(),
            static_cast<uint64_t>(chromeos::KeyTag::kCorporate));
}

TEST_F(KeystoreServiceTest, GetKeyTagsFail) {
  EXPECT_CALL(key_permissions_service_, IsCorporateKey)
      .WillOnce(RunOnceCallback<1>(std::nullopt, Status::kErrorInternal));

  CallbackObserver<chromeos::GetKeyTagsResult> observer;
  keystore_service_.GetKeyTags(GetPublicKeyBin(), observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(), KeystoreError::kInternal);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, AddKeyTagsSuccess) {
  const uint64_t tags = static_cast<uint64_t>(chromeos::KeyTag::kCorporate);

  EXPECT_CALL(key_permissions_service_,
              SetCorporateKey(GetPublicKeyBin(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(Status::kSuccess));

  StatusCallbackObserver observer;
  keystore_service_.AddKeyTags(GetPublicKeyBin(), tags, observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, false);
}

TEST_F(KeystoreServiceTest, AddKeyTagsFail) {
  const uint64_t tags = static_cast<uint64_t>(chromeos::KeyTag::kCorporate);

  EXPECT_CALL(key_permissions_service_,
              SetCorporateKey(GetPublicKeyBin(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(Status::kErrorInternal));

  StatusCallbackObserver observer;
  keystore_service_.AddKeyTags(GetPublicKeyBin(), tags, observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, true);
  EXPECT_EQ(observer.result_error, KeystoreError::kInternal);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, CanUserGrantPermissionForKey) {
  EXPECT_CALL(key_permissions_service_,
              CanUserGrantPermissionForKey(GetPublicKeyBin(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(false));

  CallbackObserver<bool> observer;
  keystore_service_.CanUserGrantPermissionForKey(GetPublicKeyBin(),
                                                 observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  EXPECT_EQ(observer.result, false);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, SetAttributeForKeySuccess) {
  EXPECT_CALL(platform_keys_service_,
              SetAttributeForKey(
                  TokenId::kUser, GetPublicKeyBin(),
                  chromeos::platform_keys::KeyAttributeType::kPlatformKeysTag,
                  GetDataBin(),
                  /*callback=*/_))
      .WillOnce(RunOnceCallback<4>(Status::kSuccess));

  StatusCallbackObserver observer;
  keystore_service_.SetAttributeForKey(
      KeystoreType::kUser, GetPublicKeyBin(),
      KeystoreKeyAttributeType::kPlatformKeysTag, GetDataBin(),
      observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, false);
}

TEST_F(KeystoreServiceTest, SetAttributeForKeyFail) {
  EXPECT_CALL(platform_keys_service_,
              SetAttributeForKey(
                  TokenId::kUser, GetPublicKeyBin(),
                  chromeos::platform_keys::KeyAttributeType::kPlatformKeysTag,
                  GetDataBin(),
                  /*callback=*/_))
      .WillOnce(RunOnceCallback<4>(Status::kErrorKeyAttributeSettingFailed));

  StatusCallbackObserver observer;
  keystore_service_.SetAttributeForKey(
      KeystoreType::kUser, GetPublicKeyBin(),
      KeystoreKeyAttributeType::kPlatformKeysTag, GetDataBin(),
      observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, true);
  EXPECT_EQ(observer.result_error, KeystoreError::kKeyAttributeSettingFailed);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, GetPublicKeySuccess) {
  const std::vector<uint8_t> cert_bin =
      CertToBlob(GetCertificateList()->front());

  CallbackObserver<chromeos::GetPublicKeyResult> observer;
  keystore_service_.GetPublicKey(
      cert_bin, KeystoreAlgorithmName::kRsassaPkcs115, observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());

  ASSERT_TRUE(observer.result.value().has_value());
  const chromeos::GetPublicKeySuccessResult& success_result =
      observer.result.value().value();
  EXPECT_EQ(success_result.public_key, GetPublicKeyBin());

  ASSERT_TRUE(std::holds_alternative<chromeos::RsassaPkcs115Params>(
      success_result.algorithm_properties));
  const auto& params = std::get<chromeos::RsassaPkcs115Params>(
      success_result.algorithm_properties);
  EXPECT_EQ(params.rsa_params.modulus_length, 2048u);
  EXPECT_EQ(params.rsa_params.public_exponent, (std::vector<uint8_t>{1, 0, 1}));
}

TEST_F(KeystoreServiceTest, RsaOaepAlgoGetPublicKeyFail) {
  const std::vector<uint8_t> cert_bin =
      CertToBlob(GetCertificateList()->front());

  CallbackObserver<chromeos::GetPublicKeyResult> observer;
  keystore_service_.GetPublicKey(cert_bin, KeystoreAlgorithmName::kRsaOaep,
                                 observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(),
                KeystoreError::kAlgorithmNotPermittedByCertificate);
}

TEST_F(KeystoreServiceTest, UnknownAlgoGetPublicKeyFail) {
  const std::vector<uint8_t> cert_bin =
      CertToBlob(GetCertificateList()->front());

  CallbackObserver<chromeos::GetPublicKeyResult> observer;
  keystore_service_.GetPublicKey(cert_bin, KeystoreAlgorithmName::kUnknown,
                                 observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(),
                KeystoreError::kAlgorithmNotPermittedByCertificate);
}

TEST_F(KeystoreServiceTest, BadCertificateGetPublicKeyFail) {
  // Using some random sequence as certificate.
  const std::vector<uint8_t> bad_cert_bin = {10, 11, 12, 13, 14, 15};
  CallbackObserver<chromeos::GetPublicKeyResult> observer;

  keystore_service_.GetPublicKey(bad_cert_bin,
                                 KeystoreAlgorithmName::kRsassaPkcs115,
                                 observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(), KeystoreError::kCertificateInvalid);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, GetKeyStoresEmptySuccess) {
  EXPECT_CALL(platform_keys_service_, GetTokens)
      .WillOnce(RunOnceCallback<0>(std::vector<TokenId>({}), Status::kSuccess));

  CallbackObserver<chromeos::GetKeyStoresResult> observer;
  keystore_service_.GetKeyStores(observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_TRUE(observer.result.value().value().empty());
}

TEST_F(KeystoreServiceTest, GetKeyStoresUserSuccess) {
  EXPECT_CALL(platform_keys_service_, GetTokens)
      .WillOnce(RunOnceCallback<0>(std::vector<TokenId>({TokenId::kUser}),
                                   Status::kSuccess));

  CallbackObserver<chromeos::GetKeyStoresResult> observer;
  keystore_service_.GetKeyStores(observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_THAT(observer.result.value().value(),
              ElementsAre(chromeos::KeystoreType::kUser));
}

TEST_F(KeystoreServiceTest, GetKeyStoresDeviceSuccess) {
  EXPECT_CALL(platform_keys_service_, GetTokens)
      .WillOnce(RunOnceCallback<0>(std::vector<TokenId>({TokenId::kSystem}),
                                   Status::kSuccess));

  CallbackObserver<chromeos::GetKeyStoresResult> observer;
  keystore_service_.GetKeyStores(observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_THAT(observer.result.value().value(),
              ElementsAre(chromeos::KeystoreType::kDevice));
}

TEST_F(KeystoreServiceTest, GetKeyStoresDeviceUserSuccess) {
  EXPECT_CALL(platform_keys_service_, GetTokens)
      .WillOnce(RunOnceCallback<0>(
          std::vector<TokenId>({TokenId::kUser, TokenId::kSystem}),
          Status::kSuccess));

  CallbackObserver<chromeos::GetKeyStoresResult> observer;
  keystore_service_.GetKeyStores(observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_THAT(observer.result.value().value(),
              UnorderedElementsAre(chromeos::KeystoreType::kUser,
                                   chromeos::KeystoreType::kDevice));
}

TEST_F(KeystoreServiceTest, GetKeyStoresFail) {
  EXPECT_CALL(platform_keys_service_, GetTokens)
      .WillOnce(
          RunOnceCallback<0>(std::vector<TokenId>({}), Status::kErrorInternal));

  CallbackObserver<chromeos::GetKeyStoresResult> observer;
  keystore_service_.GetKeyStores(observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(), KeystoreError::kInternal);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, GetCertificatesSuccess) {
  EXPECT_CALL(platform_keys_service_,
              GetCertificates(TokenId::kUser, /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(GetCertificateList(), Status::kSuccess));

  CallbackObserver<chromeos::GetCertificatesResult> observer;
  keystore_service_.GetCertificates(KeystoreType::kUser,
                                    observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  AssertCertListEq(observer.result.value().value(), GetCertificateList());
}

TEST_F(KeystoreServiceTest, InternalErrorThenGetCertificatesFail) {
  EXPECT_CALL(platform_keys_service_,
              GetCertificates(TokenId::kUser, /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(std::make_unique<net::CertificateList>(),
                                   Status::kErrorInternal));

  CallbackObserver<chromeos::GetCertificatesResult> observer;
  keystore_service_.GetCertificates(KeystoreType::kUser,
                                    observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  AssertErrorEq(observer.result.value(), KeystoreError::kInternal);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, AddCertificateSuccess) {
  auto cert_list = GetCertificateList();

  EXPECT_CALL(platform_keys_service_,
              ImportCertificate(TokenId::kSystem, CertEq(cert_list->front()),
                                /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(Status::kSuccess));

  StatusCallbackObserver observer;
  keystore_service_.AddCertificate(KeystoreType::kDevice,
                                   CertToBlob(cert_list->front()),
                                   observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, false);
}

TEST_F(KeystoreServiceTest, InvalidCertificateThenAddCertificateFail) {
  auto valid_cert = GetCertificateList()->front();
  StatusCallbackObserver observer;
  // Mocking very long input as a reason for invalid certificate.
  EXPECT_CALL(platform_keys_service_,
              ImportCertificate(TokenId::kSystem, CertEq(valid_cert),
                                /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(Status::kErrorInputTooLong));

  keystore_service_.AddCertificate(
      KeystoreType::kDevice, CertToBlob(valid_cert), observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, true);
  EXPECT_EQ(observer.result_error, KeystoreError::kInputTooLong);
}

TEST_F(KeystoreServiceTest, NotParsebleCertThenAddCertificateFail) {
  std::vector<uint8_t> empty_cert_blob;
  StatusCallbackObserver observer;

  keystore_service_.AddCertificate(KeystoreType::kDevice, empty_cert_blob,
                                   observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, true);
  EXPECT_EQ(observer.result_error, KeystoreError::kCertificateInvalid);
}

//------------------------------------------------------------------------------

TEST_F(KeystoreServiceTest, RemoveCertificateSuccess) {
  auto cert_list = GetCertificateList();

  EXPECT_CALL(platform_keys_service_,
              RemoveCertificate(TokenId::kSystem, CertEq(cert_list->front()),
                                /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(Status::kSuccess));

  StatusCallbackObserver observer;
  keystore_service_.RemoveCertificate(KeystoreType::kDevice,
                                      CertToBlob(cert_list->front()),
                                      observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, false);
}

TEST_F(KeystoreServiceTest, RemoveCertificateFail) {
  auto cert_list = GetCertificateList();

  EXPECT_CALL(platform_keys_service_,
              RemoveCertificate(TokenId::kSystem, CertEq(cert_list->front()),
                                /*callback=*/_))
      .WillOnce(RunOnceCallback<2>(Status::kErrorCertificateInvalid));

  StatusCallbackObserver observer;
  keystore_service_.RemoveCertificate(KeystoreType::kDevice,
                                      CertToBlob(cert_list->front()),
                                      observer.GetCallback());

  ASSERT_TRUE(observer.has_value());
  EXPECT_EQ(observer.result_is_error, true);
  EXPECT_EQ(observer.result_error, KeystoreError::kCertificateInvalid);
}

//------------------------------------------------------------------------------

attestation::MockTpmChallengeKey* InjectMockChallengeKey() {
  auto mock_challenge_key =
      std::make_unique<attestation::MockTpmChallengeKey>();
  attestation::MockTpmChallengeKey* challenge_key_ptr =
      mock_challenge_key.get();
  attestation::TpmChallengeKeyFactory::SetForTesting(
      std::move(mock_challenge_key));
  return challenge_key_ptr;
}

TEST_F(KeystoreServiceTest, ChallengeUserKeyNoMigrateSuccess) {
  // Incoming challenge and outgoing challenge response are imitated with the
  // same data blob. It is not realistic, but good enough for this test.

  attestation::MockTpmChallengeKey* challenge_key_ptr =
      InjectMockChallengeKey();

  EXPECT_CALL(
      *challenge_key_ptr,
      BuildResponse(::attestation::ENTERPRISE_USER,
                    /*profile=*/_, /*callback=*/_, /*challenge=*/GetDataStr(),
                    /*register_key=*/false,
                    /*key_crypto_type=*/KEY_TYPE_RSA,
                    /*key_name=*/std::string(),
                    /*signals=*/_))
      .WillOnce(RunOnceCallback<2>(
          attestation::TpmChallengeKeyResult::MakeChallengeResponse(
              GetDataStr())));

  CallbackObserver<chromeos::ChallengeAttestationOnlyKeystoreResult> observer;
  keystore_service_.ChallengeAttestationOnlyKeystore(
      KeystoreType::kUser, /*challenge=*/GetDataBin(), /*migrate=*/false,
      KeystoreAlgorithmName::kRsassaPkcs115, observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_EQ(observer.result.value().value(), GetDataBin());
}

TEST_F(KeystoreServiceTest, ChallengeUserKeyMigrateSuccess) {
  // Incoming challenge and outgoing challenge response are imitated with the
  // same data blob. It is not realistic, but good enough for this test.

  attestation::MockTpmChallengeKey* challenge_key_ptr =
      InjectMockChallengeKey();

  EXPECT_CALL(
      *challenge_key_ptr,
      BuildResponse(::attestation::ENTERPRISE_USER,
                    /*profile=*/_, /*callback=*/_, /*challenge=*/GetDataStr(),
                    /*register_key=*/true,
                    /*key_crypto_type=*/KEY_TYPE_RSA,
                    /*key_name=*/std::string(),
                    /*signals=*/_))
      .WillOnce(RunOnceCallback<2>(
          attestation::TpmChallengeKeyResult::MakeChallengeResponse(
              GetDataStr())));

  CallbackObserver<chromeos::ChallengeAttestationOnlyKeystoreResult> observer;
  keystore_service_.ChallengeAttestationOnlyKeystore(
      KeystoreType::kUser, /*challenge=*/GetDataBin(), /*migrate=*/true,
      KeystoreAlgorithmName::kRsassaPkcs115, observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_EQ(observer.result.value().value(), GetDataBin());
}

TEST_F(KeystoreServiceTest, ChallengeDeviceKeyNoMigrateSuccess) {
  // Incoming challenge and outgoing challenge response are imitated with the
  // same data blob. It is not realistic, but good enough for this test.

  attestation::MockTpmChallengeKey* challenge_key_ptr =
      InjectMockChallengeKey();

  EXPECT_CALL(
      *challenge_key_ptr,
      BuildResponse(::attestation::ENTERPRISE_MACHINE,
                    /*profile=*/_, /*callback=*/_, /*challenge=*/GetDataStr(),
                    /*register_key=*/false,
                    /*key_crypto_type=*/KEY_TYPE_RSA,
                    /*key_name=*/std::string(),
                    /*signals=*/_))
      .WillOnce(RunOnceCallback<2>(
          attestation::TpmChallengeKeyResult::MakeChallengeResponse(
              GetDataStr())));

  CallbackObserver<chromeos::ChallengeAttestationOnlyKeystoreResult> observer;
  keystore_service_.ChallengeAttestationOnlyKeystore(
      KeystoreType::kDevice, /*challenge=*/GetDataBin(),
      /*migrate=*/false, KeystoreAlgorithmName::kRsassaPkcs115,
      observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_EQ(observer.result.value().value(), GetDataBin());
}

TEST_F(KeystoreServiceTest, ChallengeDeviceKeyMigrateSuccess) {
  // Incoming challenge and outgoing challenge response are imitated with the
  // same data blob. It is not realistic, but good enough for this test.

  attestation::MockTpmChallengeKey* challenge_key_ptr =
      InjectMockChallengeKey();

  EXPECT_CALL(
      *challenge_key_ptr,
      BuildResponse(::attestation::ENTERPRISE_MACHINE,
                    /*profile=*/_, /*callback=*/_, /*challenge=*/GetDataStr(),
                    /*register_key=*/true,
                    /*key_crypto_type=*/KEY_TYPE_RSA,
                    /*key_name=*/StrStartsWith("attest-ent-machine-keystore-"),
                    /*signals=*/_))
      .WillOnce(RunOnceCallback<2>(
          attestation::TpmChallengeKeyResult::MakeChallengeResponse(
              GetDataStr())));

  CallbackObserver<chromeos::ChallengeAttestationOnlyKeystoreResult> observer;
  keystore_service_.ChallengeAttestationOnlyKeystore(
      KeystoreType::kDevice, /*challenge=*/GetDataBin(),
      /*migrate=*/true, KeystoreAlgorithmName::kRsassaPkcs115,
      observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_EQ(observer.result.value().value(), GetDataBin());
}

TEST_F(KeystoreServiceTest, ChallengeUserEcdsaKeyMigrateSuccess) {
  // Incoming challenge and outgoing challenge response are imitated with the
  // same data blob. It is not realistic, but good enough for this test.

  attestation::MockTpmChallengeKey* challenge_key_ptr =
      InjectMockChallengeKey();

  EXPECT_CALL(
      *challenge_key_ptr,
      BuildResponse(::attestation::ENTERPRISE_USER,
                    /*profile=*/_, /*callback=*/_, /*challenge=*/GetDataStr(),
                    /*register_key=*/true,
                    /*key_crypto_type=*/KEY_TYPE_ECC,
                    /*key_name=*/std::string(),
                    /*signals=*/_))
      .WillOnce(RunOnceCallback<2>(
          attestation::TpmChallengeKeyResult::MakeChallengeResponse(
              GetDataStr())));

  CallbackObserver<chromeos::ChallengeAttestationOnlyKeystoreResult> observer;
  keystore_service_.ChallengeAttestationOnlyKeystore(
      KeystoreType::kUser, /*challenge=*/GetDataBin(), /*migrate=*/true,
      KeystoreAlgorithmName::kEcdsa, observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_TRUE(observer.result.value().has_value());
  EXPECT_EQ(observer.result.value().value(), GetDataBin());
}

TEST_F(KeystoreServiceTest, ChallengeKeyFail) {
  attestation::MockTpmChallengeKey* challenge_key_ptr =
      InjectMockChallengeKey();

  auto challenge_result = attestation::TpmChallengeKeyResult::MakeError(
      attestation::TpmChallengeKeyResultCode::kDbusError);

  EXPECT_CALL(
      *challenge_key_ptr,
      BuildResponse(::attestation::ENTERPRISE_USER,
                    /*profile=*/_, /*callback=*/_, /*challenge=*/GetDataStr(),
                    /*register_key=*/false,
                    /*key_crypto_type=*/KEY_TYPE_RSA,
                    /*key_name=*/std::string(),
                    /*signals=*/_))
      .WillOnce(RunOnceCallback<2>(challenge_result));

  CallbackObserver<chromeos::ChallengeAttestationOnlyKeystoreResult> observer;
  keystore_service_.ChallengeAttestationOnlyKeystore(
      KeystoreType::kUser, /*challenge=*/GetDataBin(),
      /*migrate=*/false, KeystoreAlgorithmName::kRsassaPkcs115,
      observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_FALSE(observer.result.value().has_value());
  EXPECT_EQ(observer.result.value().error(),
            challenge_result.GetErrorMessage());
}

TEST_F(KeystoreServiceTest, ChallengeRsaOaepKeyFails) {
  CallbackObserver<chromeos::ChallengeAttestationOnlyKeystoreResult> observer;

  keystore_service_.ChallengeAttestationOnlyKeystore(
      KeystoreType::kUser, /*challenge=*/GetDataBin(), /*migrate=*/false,
      KeystoreAlgorithmName::kRsaOaep, observer.GetCallback());

  ASSERT_TRUE(observer.result.has_value());
  ASSERT_FALSE(observer.result.value().has_value());
  EXPECT_EQ(observer.result.value().error(),
            chromeos::platform_keys::KeystoreErrorToString(
                KeystoreError::kUnsupportedKeyType));
}

}  // namespace
}  // namespace ash
