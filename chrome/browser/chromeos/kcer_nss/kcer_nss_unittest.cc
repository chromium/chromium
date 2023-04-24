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
#include "base/task/bind_post_task.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/kcer_nss/kcer_token_impl_nss.h"
#include "chromeos/components/kcer/kcer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/pem.h"
#include "net/test/cert_builder.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

// The tests here provide only the minimal coverage for the basic functionality
// of Kcer. More thorough testing, including edge cases, will be done in a
// fuzzer.
// TODO(244408716): Implement the fuzzer.

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

namespace {

enum class KeyType { kRsa, kEc };

std::string KeyTypeToStr(KeyType key_type) {
  switch (key_type) {
    case KeyType::kRsa:
      return "kRsa";
    case KeyType::kEc:
      return "kEc";
  }
}

constexpr char kPublicKeyBase64[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArURIGgAq8joyzjFdUpzmOeDa5VgTC8"
    "n77sMCQsm01mwk+6NwHhCSyCfXoB9EuMcKynj9SZbCgArnsHcZiqBsKpU/VnBO/"
    "vp5MSY5qFMYxEpjPYSQcASUkOlkVYieQN6NK4FUynPJBIh3Rs6LUHlGU+"
    "w3GifCl3Be4Q0om61Eo+jxQJBlRFTyqETh0AeHI2lEK9hsePsn8AMJn2tv7GoaiS+"
    "RoZsMAcDg8uhtmlQB/"
    "eoy7MtXwSchI0e2Q8QdUneNp529Ee+pUQ5Uki1L2pE4Pnyj+j2i2x4wGFGdJgiBMSvtpvdPdF+"
    "NMfjdbVaDzTF3rcL3lNCxRb4xk3TMFXV7dQIDAQAB";

std::vector<uint8_t> StrToBytes(const std::string& val) {
  return std::vector<uint8_t>(val.begin(), val.end());
}

scoped_refptr<base::SingleThreadTaskRunner> IOTaskRunner() {
  return content::GetIOThreadTaskRunner({});
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

// A helper class to work with tokens (that exist on the IO thread) from the UI
// thread.
class TokenHolder {
 public:
  explicit TokenHolder(Token token) {
    io_token_ = std::make_unique<internal::KcerTokenImplNss>(token);
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
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      list_certs_waiter;
  kcer->ListCerts({Token::kUser}, list_certs_waiter.GetCallback());
  // Close the list with one more GenerateRsaKey, so all methods are tested with
  // other methods before and after them.
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
  ASSERT_FALSE(list_certs_waiter.Get<1>().empty());
  EXPECT_EQ(list_certs_waiter.Get<1>().at(Token::kUser),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(generate_rsa_waiter_2.Get().has_value());
  EXPECT_EQ(generate_rsa_waiter_2.Get().error(),
            Error::kTokenInitializationFailed);
}

class KcerNssAllKeyTypesTest : public KcerNssTest,
                               public testing::WithParamInterface<KeyType> {
 protected:
  KeyType GetKeyType() { return GetParam(); }
};

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
    case KeyType::kEc:
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
                         testing::Values(KeyType::kRsa, KeyType::kEc),
                         // Make test names more readable:
                         [](const auto& info) {
                           return KeyTypeToStr(info.param);
                         });

}  // namespace
}  // namespace kcer
