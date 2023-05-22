// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/kcer_nss/kcer_token_impl_nss.h"
#include "chromeos/components/kcer/kcer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/secure_hash.h"
#include "crypto/signature_verifier.h"
#include "net/cert/pem.h"
#include "net/test/cert_builder.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// The tests here provide only the minimal coverage for the basic functionality
// of Kcer. More thorough testing, including edge cases, will be done in a
// fuzzer.
// TODO(244408716): Implement the fuzzer.

using testing::UnorderedElementsAre;

namespace kcer {

// Test-only overloads for better errors from EXPECT_EQ, etc.
std::ostream& operator<<(std::ostream& stream, Error val) {
  stream << static_cast<int>(val);
  return stream;
}
std::ostream& operator<<(std::ostream& stream, Token val) {
  stream << static_cast<int>(val);
  return stream;
}
std::ostream& operator<<(std::ostream& stream, PublicKey val) {
  stream << "{\n";
  stream << "  token: " << val.GetToken() << "\n";
  stream << "  pkcs11_id: " << base::Base64Encode(val.GetPkcs11Id().value())
         << "\n";
  stream << "  spki: " << base::Base64Encode(val.GetSpki().value()) << "\n";
  stream << "}\n";
  return stream;
}

namespace {

constexpr char kPublicKeyBase64[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArURIGgAq8joyzjFdUpzmOeDa5VgTC8"
    "n77sMCQsm01mwk+6NwHhCSyCfXoB9EuMcKynj9SZbCgArnsHcZiqBsKpU/VnBO/"
    "vp5MSY5qFMYxEpjPYSQcASUkOlkVYieQN6NK4FUynPJBIh3Rs6LUHlGU+"
    "w3GifCl3Be4Q0om61Eo+jxQJBlRFTyqETh0AeHI2lEK9hsePsn8AMJn2tv7GoaiS+"
    "RoZsMAcDg8uhtmlQB/"
    "eoy7MtXwSchI0e2Q8QdUneNp529Ee+pUQ5Uki1L2pE4Pnyj+j2i2x4wGFGdJgiBMSvtpvdPdF+"
    "NMfjdbVaDzTF3rcL3lNCxRb4xk3TMFXV7dQIDAQAB";

std::string KeyTypeToStr(KeyType key_type) {
  switch (key_type) {
    case KeyType::kRsa:
      return "kRsa";
    case KeyType::kEcc:
      return "kEcc";
  }
}

std::vector<uint8_t> StrToBytes(const std::string& val) {
  return std::vector<uint8_t>(val.begin(), val.end());
}

scoped_refptr<base::SingleThreadTaskRunner> IOTaskRunner() {
  return content::GetIOThreadTaskRunner({});
}

std::string ToString(const std::vector<SigningScheme>& vec) {
  std::stringstream res;
  res << "[";
  for (const SigningScheme& s : vec) {
    res << static_cast<int>(s) << ", ";
  }
  res << "]";
  return res.str();
}

std::string ToString(const absl::optional<chaps::KeyPermissions>& val) {
  if (!val.has_value()) {
    return "<empty>";
  }
  // Should be updated if `KeyPermissions` struct is changed.
  return base::StringPrintf("[arc:%d corp:%d]", val->key_usages().arc(),
                            val->key_usages().corporate());
}

bool operator==(const absl::optional<chaps::KeyPermissions>& a,
                const absl::optional<chaps::KeyPermissions>& b) {
  if (!a.has_value() || !b.has_value()) {
    return (a.has_value() == b.has_value());
  }
  return (a->SerializeAsString() == b->SerializeAsString());
}

bool KeyInfoEquals(const KeyInfo& expected, const KeyInfo& actual) {
  if (expected.is_hardware_backed != actual.is_hardware_backed) {
    LOG(ERROR) << "ERROR: is_hardware_backed: expected: "
               << expected.is_hardware_backed
               << ", actual: " << actual.is_hardware_backed;
    return false;
  }
  if (expected.key_type != actual.key_type) {
    LOG(ERROR) << "ERROR: key_type: expected: " << int(expected.key_type)
               << ", actual: " << int(actual.key_type);
    return false;
  }
  if (expected.supported_signing_schemes != actual.supported_signing_schemes) {
    LOG(ERROR) << "ERROR: supported_signing_schemes: expected: "
               << ToString(expected.supported_signing_schemes)
               << ", actual: " << ToString(actual.supported_signing_schemes);
    return false;
  }
  if (expected.nickname != actual.nickname) {
    LOG(ERROR) << "ERROR: nickname: expected: "
               << expected.nickname.value_or("<empty>")
               << ", actual: " << actual.nickname.value_or("<empty>");
    return false;
  }
  if (expected.key_permissions != actual.key_permissions) {
    LOG(ERROR) << "ERROR: key_permissions: expected: "
               << ToString(expected.key_permissions)
               << ", actual: " << ToString(actual.key_permissions);
    return false;
  }
  if (expected.cert_provisioning_profile_id !=
      actual.cert_provisioning_profile_id) {
    LOG(ERROR) << "ERROR: cert_provisioning_profile_id: expected: "
               << expected.cert_provisioning_profile_id.value_or("<empty>")
               << ", actual: "
               << actual.cert_provisioning_profile_id.value_or("<empty>");
    return false;
  }
  return true;
}

// Reads a file in the PEM format, decodes it, returns the content of the first
// PEM block in the DER format. Currently supports CERTIFICATE and PRIVATE KEY
// block types.
absl::optional<std::vector<uint8_t>> ReadPemFileReturnDer(
    const base::FilePath& path) {
  std::string pem_data;
  if (!base::ReadFileToString(path, &pem_data)) {
    return absl::nullopt;
  }

  net::PEMTokenizer tokenizer(pem_data, {"CERTIFICATE", "PRIVATE KEY"});
  if (!tokenizer.GetNext()) {
    return absl::nullopt;
  }
  return StrToBytes(tokenizer.data());
}

// Returns |hash| prefixed with DER-encoded PKCS#1 DigestInfo with
// AlgorithmIdentifier=id-sha256.
// This is useful for testing Kcer::SignRsaPkcs1Raw which only
// appends PKCS#1 v1.5 padding before signing.
std::vector<uint8_t> PrependSHA256DigestInfo(base::span<const uint8_t> hash) {
  // DER-encoded PKCS#1 DigestInfo "prefix" with
  // AlgorithmIdentifier=id-sha256.
  // The encoding is taken from https://tools.ietf.org/html/rfc3447#page-43
  const std::vector<uint8_t> kDigestInfoSha256DerData = {
      0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};

  std::vector<uint8_t> result;
  result.reserve(kDigestInfoSha256DerData.size() + hash.size());

  result.insert(result.end(), kDigestInfoSha256DerData.begin(),
                kDigestInfoSha256DerData.end());
  result.insert(result.end(), hash.begin(), hash.end());
  return result;
}

// A helper class to work with tokens (that exist on the IO thread) from the UI
// thread.
class TokenHolder {
 public:
  explicit TokenHolder(Token token) {
    io_token_ = std::make_unique<internal::KcerTokenImplNss>(token);
    io_token_->SetAttributeTranslationForTesting(/*is_enabled=*/true);
    weak_ptr_ = io_token_->GetWeakPtr();
    // After this point `io_token_` should only be used on the IO thread.
  }

  ~TokenHolder() {
    weak_ptr_.reset();
    IOTaskRunner()->DeleteSoon(FROM_HERE, std::move(io_token_));
  }

  void Initialize() {
    base::RunLoop run_loop;

    IOTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &internal::KcerTokenImplNss::Initialize, weak_ptr_,
            crypto::ScopedPK11Slot(PK11_ReferenceSlot(nss_slot_.slot()))),
        run_loop.QuitClosure());
  }

  void FailInitialization() {
    base::RunLoop run_loop;

    IOTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&internal::KcerTokenImplNss::Initialize, weak_ptr_,
                       /*nss_slot=*/nullptr),
        run_loop.QuitClosure());
  }

  base::WeakPtr<internal::KcerTokenImplNss> GetWeakPtr() { return weak_ptr_; }

 private:
  base::WeakPtr<internal::KcerTokenImplNss> weak_ptr_;
  std::unique_ptr<internal::KcerTokenImplNss> io_token_;
  crypto::ScopedTestNSSDB nss_slot_;
};

class KcerNssTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
};

std::unique_ptr<net::CertBuilder> MakeCertIssuer() {
  auto issuer = std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr,
                                                   /*issuer=*/nullptr);
  issuer->SetSubjectCommonName("IssuerSubjectCommonName");
  issuer->GenerateRSAKey();
  return issuer;
}

// Creates a certificate builder that can generate a self-signed certificate for
// the `public_key`.
std::unique_ptr<net::CertBuilder> MakeCertBuilder(
    net::CertBuilder* issuer,
    const std::vector<uint8_t>& public_key) {
  std::unique_ptr<net::CertBuilder> cert_builder =
      net::CertBuilder::FromSubjectPublicKeyInfo(public_key, issuer);
  cert_builder->SetSignatureAlgorithm(net::SignatureAlgorithm::kRsaPkcs1Sha256);
  auto now = base::Time::Now();
  cert_builder->SetValidity(now, now + base::Days(30));
  cert_builder->SetSubjectCommonName("SubjectCommonName");

  return cert_builder;
}

// Test that if a method is called with a token that is not (and won't be)
// available, then an error is returned.
TEST_F(KcerNssTest, UseUnavailableTokenThenGetError) {
  std::unique_ptr<Kcer> kcer =
      internal::CreateKcer(IOTaskRunner(), /*user_token=*/nullptr,
                           /*device_token=*/nullptr);

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer->GenerateRsaKey(Token::kUser, /*modulus_length_bits=*/2048,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());

  ASSERT_FALSE(generate_waiter.Get().has_value());
  EXPECT_EQ(generate_waiter.Get().error(), Error::kTokenIsNotAvailable);
}

TEST_F(KcerNssTest, ImportCertForImportedKey) {
  absl::optional<std::vector<uint8_t>> key = ReadPemFileReturnDer(
      net::GetTestCertsDirectory().AppendASCII("client_1.key"));
  ASSERT_TRUE(key.has_value() && (key->size() > 0));
  absl::optional<std::vector<uint8_t>> cert = ReadPemFileReturnDer(
      net::GetTestCertsDirectory().AppendASCII("client_1.pem"));
  ASSERT_TRUE(cert.has_value() && (cert->size() > 0));

  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer =
      internal::CreateKcer(IOTaskRunner(), user_token.GetWeakPtr(),
                           /*device_token=*/nullptr);

  base::test::TestFuture<base::expected<PublicKey, Error>> import_key_waiter;
  kcer->ImportKey(Token::kUser, Pkcs8PrivateKeyInfoDer(std::move(key.value())),
                  import_key_waiter.GetCallback());
  ASSERT_TRUE(import_key_waiter.Get().has_value());

  const PublicKey& public_key = import_key_waiter.Get().value();

  EXPECT_EQ(public_key.GetToken(), Token::kUser);
  // Arbitrary bytes, not much to check about them.
  EXPECT_EQ(public_key.GetPkcs11Id()->size(), 20u);
  // Arbitrary bytes, not much to check about them.
  EXPECT_EQ(public_key.GetSpki()->size(), 294u);

  base::test::TestFuture<base::expected<void, Error>> import_cert_waiter;
  kcer->ImportCertFromBytes(Token::kUser, CertDer(std::move(cert.value())),
                            import_cert_waiter.GetCallback());
  EXPECT_TRUE(import_cert_waiter.Get().has_value());
}

// Test that a certificate can not be imported, if there's no key for it on the
// token.
TEST_F(KcerNssTest, ImportCertWithoutKeyThenFail) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder = MakeCertBuilder(
      issuer.get(), base::Base64Decode(kPublicKeyBase64).value());

  CertDer cert(StrToBytes(cert_builder->GetDER()));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer->ImportCertFromBytes(Token::kUser, std::move(cert),
                            import_waiter.GetCallback());
  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kFailedToImportCertificate);

  // Double check that ListCerts doesn't find the cert.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<0>().empty());  // Cert list is empty.
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
}

// Test that all methods can be queued while a token is initializing and that
// the entire task queue can be processed when initialization completes (in this
// case - completes with a failure).
TEST_F(KcerNssTest, QueueTasksFailInitializationThenGetErrors) {
  TokenHolder user_token(Token::kUser);

  std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder = MakeCertBuilder(
      issuer.get(), base::Base64Decode(kPublicKeyBase64).value());

  // Internal values don't matter, they won't be accessed during this test.
  scoped_refptr<Cert> fake_cert = base::MakeRefCounted<Cert>(
      Token::kUser, Pkcs11Id(), /*nickname=*/std::string(),
      /*x509_cert=*/nullptr);

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_rsa_waiter;
  kcer->GenerateRsaKey(Token::kUser, /*modulus_length_bits=*/2048,
                       /*hardware_backed=*/true,
                       generate_rsa_waiter.GetCallback());
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_ec_waiter;
  kcer->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                      /*hardware_backed=*/true,
                      generate_ec_waiter.GetCallback());
  base::test::TestFuture<base::expected<PublicKey, Error>> import_key_waiter;
  kcer->ImportKey(Token::kUser, Pkcs8PrivateKeyInfoDer({1, 2, 3}),
                  import_key_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>>
      import_cert_from_bytes_waiter;
  kcer->ImportCertFromBytes(Token::kUser, CertDer({1, 2, 3}),
                            import_cert_from_bytes_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> import_x509_cert_waiter;
  kcer->ImportX509Cert(Token::kUser,
                       /*cert=*/cert_builder->GetX509Certificate(),
                       import_x509_cert_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> remove_cert_waiter;
  kcer->RemoveCert(fake_cert, remove_cert_waiter.GetCallback());
  base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
      list_keys_waiter;
  kcer->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      list_certs_waiter;
  kcer->ListCerts({Token::kUser}, list_certs_waiter.GetCallback());
  base::test::TestFuture<base::expected<bool, Error>> does_key_exist_waiter;
  kcer->DoesPrivateKeyExist(PrivateKeyHandle(PublicKeySpki()),
                            does_key_exist_waiter.GetCallback());
  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  kcer->Sign(PrivateKeyHandle(PublicKeySpki()), SigningScheme::kRsaPkcs1Sha512,
             DataToSign({1, 2, 3}), sign_waiter.GetCallback());
  base::test::TestFuture<base::expected<Signature, Error>> sign_digest_waiter;
  kcer->SignRsaPkcs1Raw(PrivateKeyHandle(PublicKeySpki()),
                        DigestWithPrefix({1, 2, 3}),
                        sign_digest_waiter.GetCallback());
  base::test::TestFuture<base::expected<TokenInfo, Error>>
      get_token_info_waiter;
  kcer->GetTokenInfo(Token::kUser, get_token_info_waiter.GetCallback());
  base::test::TestFuture<base::expected<KeyInfo, Error>> get_key_info_waiter;
  kcer->GetKeyInfo(PrivateKeyHandle(PublicKeySpki()),
                   get_key_info_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_nickname_waiter;
  kcer->SetKeyNickname(PrivateKeyHandle(PublicKeySpki()), "new_nickname",
                       set_nickname_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_permissions_waiter;
  kcer->SetKeyPermissions(PrivateKeyHandle(PublicKeySpki()),
                          chaps::KeyPermissions(),
                          set_permissions_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_cert_prov_waiter;
  kcer->SetCertProvisioningProfileId(PrivateKeyHandle(PublicKeySpki()),
                                     "cert_prov_id",
                                     set_cert_prov_waiter.GetCallback());
  // Close the list with one more GenerateRsaKey, so all methods are tested
  // with other methods before and after them.
  base::test::TestFuture<base::expected<PublicKey, Error>>
      generate_rsa_waiter_2;
  kcer->GenerateRsaKey(Token::kUser, /*modulus_length_bits=*/2048,
                       /*hardware_backed=*/true,
                       generate_rsa_waiter_2.GetCallback());
  // TODO(244408716): Add more methods when they are implemented.

  user_token.FailInitialization();

  ASSERT_FALSE(generate_rsa_waiter.Get().has_value());
  EXPECT_EQ(generate_rsa_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(generate_ec_waiter.Get().has_value());
  EXPECT_EQ(generate_ec_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_key_waiter.Get().has_value());
  EXPECT_EQ(import_key_waiter.Get().error(), Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_cert_from_bytes_waiter.Get().has_value());
  EXPECT_EQ(import_cert_from_bytes_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_x509_cert_waiter.Get().has_value());
  EXPECT_EQ(import_x509_cert_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(remove_cert_waiter.Get().has_value());
  EXPECT_EQ(remove_cert_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(list_keys_waiter.Get<1>().empty());
  EXPECT_EQ(list_keys_waiter.Get<1>().at(Token::kUser),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(list_certs_waiter.Get<1>().empty());
  EXPECT_EQ(list_certs_waiter.Get<1>().at(Token::kUser),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(does_key_exist_waiter.Get().has_value());
  EXPECT_EQ(does_key_exist_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kTokenInitializationFailed);
  ASSERT_FALSE(sign_digest_waiter.Get().has_value());
  EXPECT_EQ(sign_digest_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(get_token_info_waiter.Get().has_value());
  EXPECT_EQ(get_token_info_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(get_key_info_waiter.Get().has_value());
  EXPECT_EQ(get_key_info_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(set_nickname_waiter.Get().has_value());
  EXPECT_EQ(set_nickname_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(set_permissions_waiter.Get().has_value());
  EXPECT_EQ(set_permissions_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(set_cert_prov_waiter.Get().has_value());
  EXPECT_EQ(set_cert_prov_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(generate_rsa_waiter_2.Get().has_value());
  EXPECT_EQ(generate_rsa_waiter_2.Get().error(),
            Error::kTokenInitializationFailed);
}

TEST_F(KcerNssTest, ListKeys) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();
  TokenHolder device_token(Token::kDevice);
  device_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), device_token.GetWeakPtr());

  std::vector<PublicKey> all_expected_keys;
  std::vector<PublicKey> user_expected_keys;
  std::vector<PublicKey> device_expected_keys;

  // Initially there should be no keys.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer->ListKeys({Token::kUser, Token::kDevice},
                   list_keys_waiter.GetCallback());

    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Generate a key.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer->GenerateRsaKey(Token::kUser, /*modulus_length_bits=*/2048,
                         /*hardware_backed=*/true,
                         generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    user_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // The new key should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer->ListKeys({Token::kUser, Token::kDevice},
                   list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Generate a key on a different token.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer->GenerateRsaKey(Token::kDevice, /*modulus_length_bits=*/2048,
                         /*hardware_backed=*/true,
                         generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    device_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // Keys from both tokens should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer->ListKeys({Token::kUser, Token::kDevice},
                   list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Generate a key of a different type on user token.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                        /*hardware_backed=*/true,
                        generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    user_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // Generate a key of a different type on device token.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer->GenerateEcKey(Token::kDevice, EllipticCurve::kP256,
                        /*hardware_backed=*/true,
                        generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    device_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // Keys of both types from both tokens should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer->ListKeys({Token::kUser, Token::kDevice},
                   list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Keys of both types only from the user token should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(user_expected_keys));
  }

  // Keys of both types only from the device token should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer->ListKeys({Token::kDevice}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(device_expected_keys));
  }
}

// Test that Kcer::Sign() works correctly for RSA keys with different signing
// schemes.
// TODO(miersh): Expand crypto::SignatureVerifier to work with more signature
// schemes and add them to the test.
TEST_F(KcerNssTest, SignRsa) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_key_waiter;
  kcer->GenerateRsaKey(Token::kUser, /*modulus_length_bits=*/2048,
                       /*hardware_backed=*/true,
                       generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Get().has_value());
  const PublicKey& public_key = generate_key_waiter.Get().value();

  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Test kRsaPkcs1Sha1 signature.
  {
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer->Sign(PrivateKeyHandle(public_key), SigningScheme::kRsaPkcs1Sha1,
               data_to_sign, sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    crypto::SignatureVerifier signature_verifier;
    ASSERT_TRUE(signature_verifier.VerifyInit(
        crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1,
        signature.value(), public_key.GetSpki().value()));
    signature_verifier.VerifyUpdate(data_to_sign.value());
    EXPECT_TRUE(signature_verifier.VerifyFinal());
  }

  // Test kRsaPkcs1Sha256 signature.
  {
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer->Sign(PrivateKeyHandle(public_key), SigningScheme::kRsaPkcs1Sha256,
               data_to_sign, sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    crypto::SignatureVerifier signature_verifier;
    ASSERT_TRUE(signature_verifier.VerifyInit(
        crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
        signature.value(), public_key.GetSpki().value()));
    signature_verifier.VerifyUpdate(data_to_sign.value());
    EXPECT_TRUE(signature_verifier.VerifyFinal());
  }

  // Test kRsaPssRsaeSha256 signature.
  {
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer->Sign(PrivateKeyHandle(public_key), SigningScheme::kRsaPssRsaeSha256,
               data_to_sign, sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    crypto::SignatureVerifier signature_verifier;
    ASSERT_TRUE(signature_verifier.VerifyInit(
        crypto::SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256,
        signature.value(), public_key.GetSpki().value()));
    signature_verifier.VerifyUpdate(data_to_sign.value());
    EXPECT_TRUE(signature_verifier.VerifyFinal());
  }
}

// Test that Kcer::Sign() works correctly for ECC keys.
// TODO(miersh): Expand crypto::SignatureVerifier to work with more signature
// schemes and add them to the test.
TEST_F(KcerNssTest, SignEcc) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_key_waiter;
  kcer->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                      /*hardware_backed=*/true,
                      generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Get().has_value());
  const PublicKey& public_key = generate_key_waiter.Get().value();

  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Test kEcdsaSecp256r1Sha256 signature.
  {
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer->Sign(PrivateKeyHandle(public_key),
               SigningScheme::kEcdsaSecp256r1Sha256, data_to_sign,
               sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    crypto::SignatureVerifier signature_verifier;
    ASSERT_TRUE(signature_verifier.VerifyInit(
        crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
        signature.value(), public_key.GetSpki().value()));
    signature_verifier.VerifyUpdate(data_to_sign.value());
    EXPECT_TRUE(signature_verifier.VerifyFinal());
  }
}

// Test that `Kcer::SignRsaPkcs1Raw()` produces a correct signature.
TEST_F(KcerNssTest, SignRsaPkcs1Raw) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_key_waiter;
  kcer->GenerateRsaKey(Token::kUser, /*modulus_length_bits=*/2048,
                       /*hardware_backed=*/true,
                       generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Get().has_value());
  const PublicKey& public_key = generate_key_waiter.Get().value();

  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // A caller would need to hash the data themself before calling
  // `SignRsaPkcs1Raw`, do that here.
  auto hasher = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  hasher->Update(data_to_sign->data(), data_to_sign->size());
  std::vector<uint8_t> hash(hasher->GetHashLength());
  hasher->Finish(hash.data(), hash.size());
  DigestWithPrefix digest_with_prefix(PrependSHA256DigestInfo(hash));

  // Generate the signature.
  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  kcer->SignRsaPkcs1Raw(PrivateKeyHandle(public_key),
                        std::move(digest_with_prefix),
                        sign_waiter.GetCallback());
  ASSERT_TRUE(sign_waiter.Get().has_value());
  const Signature& signature = sign_waiter.Get().value();

  // Verify the signature.
  crypto::SignatureVerifier signature_verifier;
  ASSERT_TRUE(signature_verifier.VerifyInit(
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
      signature.value(), public_key.GetSpki().value()));
  signature_verifier.VerifyUpdate(data_to_sign.value());
  EXPECT_TRUE(signature_verifier.VerifyFinal());

  // Verify that manual hashing + `SignRsaPkcs1Raw` produces the same
  // signature as just `Sign`.
  base::test::TestFuture<base::expected<Signature, Error>> normal_sign_waiter;
  kcer->Sign(PrivateKeyHandle(public_key), SigningScheme::kRsaPkcs1Sha256,
             data_to_sign, normal_sign_waiter.GetCallback());
  ASSERT_TRUE(normal_sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().value(), normal_sign_waiter.Get().value());
}

// Test that Kcer::GetTokenInfo() method returns meaningful values.
TEST_F(KcerNssTest, GetTokenInfo) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  base::test::TestFuture<base::expected<TokenInfo, Error>>
      get_token_info_waiter;
  kcer->GetTokenInfo(Token::kUser, get_token_info_waiter.GetCallback());
  ASSERT_TRUE(get_token_info_waiter.Get().has_value());
  const TokenInfo& token_info = get_token_info_waiter.Get().value();

  // These values don't have to be exactly like this, they are what a software
  // NSS slot returns in tests. Still useful to test that they are not
  // completely off.
  EXPECT_THAT(token_info.pkcs11_id, testing::Lt(1000u));
  EXPECT_THAT(token_info.token_name,
              testing::StartsWith("NSS Application Slot"));
  EXPECT_EQ(token_info.module_name, "NSS Internal PKCS #11 Module");
}

// Test RSA specific fields from GetKeyInfo's result.
TEST_F(KcerNssTest, GetKeyInfoForRsaKey) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer->GenerateRsaKey(Token::kUser, /*modulus_length_bits=*/2048,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
  kcer->GetKeyInfo(PrivateKeyHandle(public_key), key_info_waiter.GetCallback());
  ASSERT_TRUE(key_info_waiter.Get().has_value());
  const KeyInfo& key_info = key_info_waiter.Get().value();
  EXPECT_EQ(key_info.key_type, KeyType::kRsa);
  EXPECT_THAT(
      key_info.supported_signing_schemes,
      UnorderedElementsAre(
          SigningScheme::kRsaPkcs1Sha1, SigningScheme::kRsaPkcs1Sha256,
          SigningScheme::kRsaPkcs1Sha384, SigningScheme::kRsaPkcs1Sha512,
          SigningScheme::kRsaPssRsaeSha256, SigningScheme::kRsaPssRsaeSha384,
          SigningScheme::kRsaPssRsaeSha512));
}

// Test ECC specific fields from GetKeyInfo's result.
TEST_F(KcerNssTest, GetKeyInfoForEccKey) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                      /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
  kcer->GetKeyInfo(PrivateKeyHandle(public_key), key_info_waiter.GetCallback());
  ASSERT_TRUE(key_info_waiter.Get().has_value());
  const KeyInfo& key_info = key_info_waiter.Get().value();
  EXPECT_EQ(key_info.key_type, KeyType::kEcc);
  EXPECT_THAT(key_info.supported_signing_schemes,
              UnorderedElementsAre(SigningScheme::kEcdsaSecp256r1Sha256,
                                   SigningScheme::kEcdsaSecp384r1Sha384,
                                   SigningScheme::kEcdsaSecp521r1Sha512));
}

// Test generic fields from GetKeyInfo's result and they get updated after
// related Set* methods.
TEST_F(KcerNssTest, GetKeyInfoGeneric) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                      /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  KeyInfo expected_key_info;
  // Hardware- vs software-backed indicators on real devices are provided by
  // Chaps and are wrong in unit tests.
  expected_key_info.is_hardware_backed = true;
  // NSS sets an empty nickname by default, this doesn't have to be like this
  // in general.
  expected_key_info.nickname = "";
  // Custom attributes are stored differently in tests and have empty values by
  // default.
  expected_key_info.key_permissions = chaps::KeyPermissions();
  expected_key_info.cert_provisioning_profile_id = "";

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer->GetKeyInfo(PrivateKeyHandle(public_key),
                     key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    const KeyInfo& key_info = key_info_waiter.Get().value();

    // Copy some fields, their values are covered by dedicated tests, this
    // test only checks that they don't change when they shouldn't.
    expected_key_info.key_type = key_info.key_type;
    expected_key_info.supported_signing_schemes =
        key_info.supported_signing_schemes;

    EXPECT_TRUE(KeyInfoEquals(expected_key_info, key_info));
  }

  {
    expected_key_info.nickname = "new_nickname";

    base::test::TestFuture<base::expected<void, Error>> set_nickname_waiter;
    kcer->SetKeyNickname(PrivateKeyHandle(public_key),
                         expected_key_info.nickname.value(),
                         set_nickname_waiter.GetCallback());
    ASSERT_TRUE(set_nickname_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer->GetKeyInfo(PrivateKeyHandle(public_key),
                     key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    EXPECT_TRUE(
        KeyInfoEquals(expected_key_info, key_info_waiter.Get().value()));
  }

  {
    expected_key_info.key_permissions->mutable_key_usages()->set_corporate(
        true);
    expected_key_info.key_permissions->mutable_key_usages()->set_arc(true);

    base::test::TestFuture<base::expected<void, Error>> set_permissions_waiter;
    kcer->SetKeyPermissions(PrivateKeyHandle(public_key),
                            expected_key_info.key_permissions.value(),
                            set_permissions_waiter.GetCallback());
    ASSERT_TRUE(set_permissions_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer->GetKeyInfo(PrivateKeyHandle(public_key),
                     key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    EXPECT_TRUE(
        KeyInfoEquals(expected_key_info, key_info_waiter.Get().value()));
  }

  {
    expected_key_info.cert_provisioning_profile_id = "cert_prov_id_123";

    base::test::TestFuture<base::expected<void, Error>> set_permissions_waiter;
    kcer->SetCertProvisioningProfileId(
        PrivateKeyHandle(public_key),
        expected_key_info.cert_provisioning_profile_id.value(),
        set_permissions_waiter.GetCallback());
    ASSERT_TRUE(set_permissions_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer->GetKeyInfo(PrivateKeyHandle(public_key),
                     key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    EXPECT_TRUE(
        KeyInfoEquals(expected_key_info, key_info_waiter.Get().value()));
  }
}

// Test different ways to call DoesPrivateKeyExist() method and that it returns
// correct results when Kcer has access to one token.
TEST_F(KcerNssTest, DoesPrivateKeyExistOneToken) {
  TokenHolder device_token(Token::kDevice);
  device_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), /*user_token=*/nullptr, device_token.GetWeakPtr());

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer->GenerateEcKey(Token::kDevice, EllipticCurve::kP256,
                      /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  // The private key should be found by the PublicKey.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                              does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // The private key should be found by the SPKI.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(PrivateKeyHandle(public_key.GetSpki()),
                              does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // The private key should be found on the specified token by the SPKI.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kDevice, public_key.GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // Looking for a key on a non-existing token should return an error.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kUser, public_key.GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_FALSE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().error(), Error::kTokenIsNotAvailable);
  }

  // Looking for a key by an invalid SPKI should return an error.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::vector<uint8_t>{1, 2, 3})),
        does_exist_waiter.GetCallback());
    ASSERT_FALSE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().error(), Error::kFailedToGetKeyId);
  }

  // Looking for a non-existing key should return a negative result.
  {
    std::vector<uint8_t> non_existing_key =
        base::Base64Decode(kPublicKeyBase64).value();
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::move(non_existing_key))),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value())
        << does_exist_waiter.Get().error();
    EXPECT_EQ(does_exist_waiter.Get().value(), false);
  }
}

class KcerNssAllKeyTypesTest : public KcerNssTest,
                               public testing::WithParamInterface<KeyType> {
 protected:
  KeyType GetKeyType() { return GetParam(); }
};

// Test different ways to call DoesPrivateKeyExist() method and that it returns
// correct results when Kcer has access to two tokens.
TEST_P(KcerNssAllKeyTypesTest, DoesPrivateKeyExistTwoTokens) {
  TokenHolder device_token(Token::kDevice);
  device_token.Initialize();
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), device_token.GetWeakPtr());

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  switch (GetKeyType()) {
    case KeyType::kRsa:
      kcer->GenerateRsaKey(Token::kDevice, /*modulus_length_bits=*/2048,
                           /*hardware_backed=*/true,
                           generate_waiter.GetCallback());
      break;
    case KeyType::kEcc:
      kcer->GenerateEcKey(Token::kDevice, EllipticCurve::kP256,
                          /*hardware_backed=*/true,
                          generate_waiter.GetCallback());
      break;
  }
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  // The private key should be found by the PublicKey.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                              does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // The private key should be found by the SPKI.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(PrivateKeyHandle(public_key.GetSpki()),
                              does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // The private key should be found on the specified token by the SPKI.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kDevice, public_key.GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // Looking for a key on another (existing) token should return a negative
  // result.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kUser, public_key.GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), false);
  }

  // Looking for a key by an incorrect SPKI should return an error.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::vector<uint8_t>{1, 2, 3})),
        does_exist_waiter.GetCallback());
    ASSERT_FALSE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().error(), Error::kFailedToGetKeyId);
  }

  // Looking for a non-existing key should return a negative result.
  {
    std::vector<uint8_t> non_existing_key =
        base::Base64Decode(kPublicKeyBase64).value();
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::move(non_existing_key))),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value())
        << does_exist_waiter.Get().error();
    EXPECT_EQ(does_exist_waiter.Get().value(), false);
  }
}

// Test that all methods work together as expected. Simulate a potential
// lifecycle of a key and related objects.
TEST_P(KcerNssAllKeyTypesTest, AllMethodsTogether) {
  TokenHolder user_token(Token::kUser);
  user_token.Initialize();

  std::unique_ptr<Kcer> kcer = internal::CreateKcer(
      IOTaskRunner(), user_token.GetWeakPtr(), /*device_token=*/nullptr);

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  switch (GetKeyType()) {
    case KeyType::kRsa:
      kcer->GenerateRsaKey(Token::kUser, /*modulus_length_bits=*/2048,
                           /*hardware_backed=*/true,
                           generate_waiter.GetCallback());
      break;
    case KeyType::kEcc:
      kcer->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                          /*hardware_backed=*/true,
                          generate_waiter.GetCallback());
      break;
  }
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder =
      MakeCertBuilder(issuer.get(), public_key.GetSpki().value());

  // Import a cert for the key.
  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer->ImportX509Cert(Token::kUser, cert_builder->GetX509Certificate(),
                       import_waiter.GetCallback());
  EXPECT_TRUE(import_waiter.Get().has_value());

  // List certs, make sure the new cert is listed.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  const auto& certs =
      certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>();
  ASSERT_EQ(certs.size(), 1u);
  EXPECT_TRUE(certs.front()->GetX509Cert()->EqualsExcludingChain(
      cert_builder->GetX509Certificate().get()));

  // Remove the cert.
  base::test::TestFuture<base::expected<void, Error>> remove_cert_waiter;
  kcer->RemoveCert(certs.front(), remove_cert_waiter.GetCallback());
  ASSERT_TRUE(remove_cert_waiter.Get().has_value());

  // Check that the cert cannot be found anymore.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter_2;
  kcer->ListCerts({Token::kUser}, certs_waiter_2.GetCallback());
  EXPECT_TRUE(certs_waiter_2.Get<1>().empty());  // Error map is empty.
  ASSERT_EQ(certs_waiter_2.Get<std::vector<scoped_refptr<const Cert>>>().size(),
            0u);

  std::unique_ptr<net::CertBuilder> issuer_2 = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder_2 =
      MakeCertBuilder(issuer_2.get(), public_key.GetSpki().value());

  // Import another cert for the key to check that the key was not removed and
  // is still usable.
  base::test::TestFuture<base::expected<void, Error>> import_waiter_2;
  kcer->ImportX509Cert(Token::kUser, cert_builder_2->GetX509Certificate(),
                       import_waiter_2.GetCallback());
  EXPECT_TRUE(import_waiter_2.Get().has_value());
}

INSTANTIATE_TEST_SUITE_P(AllKeyTypes,
                         KcerNssAllKeyTypesTest,
                         testing::Values(KeyType::kRsa, KeyType::kEcc),
                         // Make test names more readable:
                         [](const auto& info) {
                           return KeyTypeToStr(info.param);
                         });

}  // namespace
}  // namespace kcer
