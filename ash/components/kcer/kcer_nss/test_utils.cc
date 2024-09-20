// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/kcer_nss/test_utils.h"

#include <pk11pub.h>

#include "ash/components/kcer/kcer_token.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/signature_verifier.h"
#include "net/test/cert_builder.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/pem.h"

using SignatureAlgorithm = crypto::SignatureVerifier::SignatureAlgorithm;

namespace kcer {
namespace {
std::string ToString(const std::optional<chaps::KeyPermissions>& val) {
  if (!val.has_value()) {
    return "<empty>";
  }
  // Should be updated if `KeyPermissions` struct is changed.
  return base::StringPrintf("[arc:%d corp:%d]", val->key_usages().arc(),
                            val->key_usages().corporate());
}

crypto::ScopedPK11Slot CopySlotPtr(PK11SlotInfo* slot) {
  return crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot));
}

}  // namespace

TokenHolder::TokenHolder(Token token,
                         HighLevelChapsClient* chaps_client,
                         bool initialize_token) {
  Initialize(token, chaps_client, initialize_token,
             crypto::ScopedPK11Slot(PK11_ReferenceSlot(nss_db_.slot())));
}

TokenHolder::TokenHolder(Token token,
                         HighLevelChapsClient* chaps_client,
                         bool initialize_token,
                         crypto::ScopedPK11Slot nss_slot) {
  Initialize(token, chaps_client, initialize_token, std::move(nss_slot));
}

void TokenHolder::Initialize(Token token,
                             HighLevelChapsClient* chaps_client,
                             bool initialize,
                             crypto::ScopedPK11Slot nss_slot) {
  if (!nss_slot) {
    return;
  }

  io_token_ = std::make_unique<internal::KcerTokenImplNss>(token, chaps_client);
  io_token_->SetAttributeTranslationForTesting(/*is_enabled=*/true);
  weak_ptr_ = io_token_->GetWeakPtr();
  // After this point `io_token_` should only be used on the IO thread.

  nss_slot_ = std::move(nss_slot);

  if (initialize) {
    InitializeToken();
  }
}

TokenHolder::~TokenHolder() {
  weak_ptr_.reset();
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 std::move(io_token_));
}

void TokenHolder::InitializeToken() {
  CHECK(!is_initialized_);
  is_initialized_ = true;

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &internal::KcerToken::InitializeForNss, weak_ptr_,
          crypto::ScopedPK11Slot(PK11_ReferenceSlot(nss_slot_.get()))));
}

void TokenHolder::FailTokenInitialization() {
  CHECK(!is_initialized_);
  is_initialized_ = true;

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&internal::KcerToken::InitializeForNss, weak_ptr_,
                     /*nss_slot=*/nullptr));
}

uint32_t TokenHolder::GetSlotId() {
  return PK11_GetSlotID(nss_slot_.get());
}

//==============================================================================

KeyAndCert::KeyAndCert(PublicKey key, scoped_refptr<const Cert> cert)
    : key(key), cert(cert) {}
KeyAndCert::KeyAndCert(KeyAndCert&&) = default;
KeyAndCert& KeyAndCert::operator=(KeyAndCert&&) = default;
KeyAndCert::~KeyAndCert() = default;

//==============================================================================

TestKcerHolder::TestKcerHolder(PK11SlotInfo* user_slot,
                               PK11SlotInfo* device_slot)
    : user_token_(Token::kUser,
                  /*chaps_client=*/nullptr,
                  true,
                  user_slot ? CopySlotPtr(user_slot) : nullptr),
      device_token_(Token::kDevice,
                    /*chaps_client=*/nullptr,
                    true,
                    device_slot ? CopySlotPtr(device_slot) : nullptr) {
  kcer_ = std::make_unique<kcer::internal::KcerImpl>();
  kcer_->Initialize(content::GetIOThreadTaskRunner({}),
                    user_token_.GetWeakPtr(), device_token_.GetWeakPtr());
}

TestKcerHolder::~TestKcerHolder() = default;

base::WeakPtr<Kcer> TestKcerHolder::GetKcer() {
  return kcer_->GetWeakPtr();
}

//==============================================================================

[[nodiscard]] bool ExpectKeyPermissionsEqual(
    const std::optional<chaps::KeyPermissions>& a,
    const std::optional<chaps::KeyPermissions>& b) {
  bool result = true;
  if (!a.has_value() || !b.has_value()) {
    result = (a.has_value() == b.has_value());
  } else {
    result = (a->SerializeAsString() == b->SerializeAsString());
  }
  if (!result) {
    LOG(ERROR) << "ERROR: key_permissions: a: " << ToString(a)
               << ", b: " << ToString(b);
  }
  return result;
}

bool VerifySignature(SigningScheme signing_scheme,
                     PublicKeySpki spki,
                     DataToSign data_to_sign,
                     Signature signature,
                     bool strict) {
  SignatureAlgorithm signature_algo = SignatureAlgorithm::RSA_PKCS1_SHA1;
  switch (signing_scheme) {
    case SigningScheme::kRsaPkcs1Sha1:
      signature_algo = SignatureAlgorithm::RSA_PKCS1_SHA1;
      break;
    case SigningScheme::kRsaPkcs1Sha256:
      signature_algo = SignatureAlgorithm::RSA_PKCS1_SHA256;
      break;
    case SigningScheme::kRsaPssRsaeSha256:
      signature_algo = SignatureAlgorithm::RSA_PSS_SHA256;
      break;
    case SigningScheme::kEcdsaSecp256r1Sha256:
      signature_algo = SignatureAlgorithm::ECDSA_SHA256;
      break;
    default:
      return !strict;
  }

  crypto::SignatureVerifier signature_verifier;
  if (!signature_verifier.VerifyInit(signature_algo, signature.value(),
                                     spki.value())) {
    LOG(ERROR) << "Failed to initialize signature verifier";
    return false;
  }
  signature_verifier.VerifyUpdate(data_to_sign.value());
  return signature_verifier.VerifyFinal();
}

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

std::optional<std::vector<uint8_t>> ReadPemFileReturnDer(
    const base::FilePath& path) {
  std::string pem_data;
  if (!base::ReadFileToString(path, &pem_data)) {
    return std::nullopt;
  }

  bssl::PEMTokenizer tokenizer(pem_data, {"CERTIFICATE", "PRIVATE KEY"});
  if (!tokenizer.GetNext()) {
    return std::nullopt;
  }
  return std::vector<uint8_t>(tokenizer.data().begin(), tokenizer.data().end());
}

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
  cert_builder->SetSignatureAlgorithm(
      bssl::SignatureAlgorithm::kRsaPkcs1Sha256);
  auto now = base::Time::Now();
  cert_builder->SetValidity(now, now + base::Days(30));
  cert_builder->SetSubjectCommonName("SubjectCommonName");

  return cert_builder;
}

std::vector<uint8_t> ReadTestFile(const std::string& file_name) {
  base::FilePath file_path =
      net::GetTestCertsDirectory().AppendASCII(file_name);
  std::optional<std::vector<uint8_t>> file_data =
      base::ReadFileToBytes(file_path);
  if (!file_data.has_value()) {
    ADD_FAILURE() << "Couldn't read " << file_path;
    return {};
  }
  return file_data.value();
}

base::expected<KeyAndCert, Error> ImportTestKeyAndCert(
    base::WeakPtr<Kcer> kcer,
    Token token,
    std::string_view key_filename,
    std::string_view cert_filename) {
  CHECK(kcer);

  std::optional<std::vector<uint8_t>> key = ReadPemFileReturnDer(
      net::GetTestCertsDirectory().AppendASCII(key_filename));
  if (!key.has_value() || (key->size() == 0)) {
    return base::unexpected(Error::kUnknownError);
  }

  std::optional<std::vector<uint8_t>> cert = ReadPemFileReturnDer(
      net::GetTestCertsDirectory().AppendASCII(cert_filename));
  if (!cert.has_value() || (cert->size() == 0)) {
    return base::unexpected(Error::kUnknownError);
  }

  base::test::TestFuture<base::expected<PublicKey, Error>> import_key_waiter;
  kcer->ImportKey(Token::kUser, Pkcs8PrivateKeyInfoDer(std::move(key.value())),
                  import_key_waiter.GetCallback());
  if (!import_key_waiter.Get().has_value()) {
    return base::unexpected(import_key_waiter.Get().error());
  }

  base::test::TestFuture<base::expected<void, Error>> import_cert_waiter;
  kcer->ImportCertFromBytes(Token::kUser, CertDer(std::move(cert.value())),
                            import_cert_waiter.GetCallback());
  if (!import_cert_waiter.Get().has_value()) {
    return base::unexpected(import_cert_waiter.Get().error());
  }

  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  const auto& certs =
      certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>();
  if (certs.size() != 1u) {
    return base::unexpected(Error::kUnknownError);
  }

  return KeyAndCert{std::move(import_key_waiter.Get().value()), certs[0]};
}

}  // namespace kcer
