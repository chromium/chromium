// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include "ash/components/kcer/chaps/mock_high_level_chaps_client.h"
#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_impl.h"
#include "ash/components/kcer/kcer_nss/test_utils.h"
#include "base/base64.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/secure_hash.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "url/gurl.h"

using testing::UnorderedElementsAreArray;

namespace kcer {

const char kPkcs8Key[] =
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDa+"
    "Dq7TTFSw1AxRkaftrCM8tuPbYH7NTxLdHil0F2y4G+PvrlqN0qB43tRaKJPQEYhG+"
    "RnppXeOk6/AbgOFXBQCPoVJWOjxwMX3ea3rSLM5C9xUP9Rsnf/"
    "fkngD6G6pOo2nYinfgpINQDhGB/"
    "r8BJs69RNhvgdbN4aV7Bz8WGYqKF3DVhV+"
    "Di5zIOPNC9zoZQPey4duMS06OERG7Op8fFws3QoCzEywVdAbe/"
    "R+m5oeg875vLVvmONwDi52mqv4rgbfl+"
    "aPhyyPzoR3hdIPEi13AQB5hmyLAcTDtvcib3beNLw586NXcYgQZdcbLmDkjVRDK4uniE4QaR"
    "UGeRJD2+xAgMBAAECggEABMvoYMcg2WGDuESZZ5u6nn0eZUlT4329H6ECQzg/"
    "KTEvOGydhqUF6eD4B/"
    "vnsZ6POrVFSaZK76EtgukbJUcqcee0b1yljrDyvCXaoojgHjFcaa90HE/"
    "Gvvm++AcXoZfwX826cILQtQK2OCK4EPTDY+U+6LYtaVruZZDTVxgr7V+v4v1EDKEjQc+"
    "Ttupwo6aXSeiTKuqNsXuodoDvcv/"
    "uJgzMDCxi14TZTjaWOz7Xw2JZ+NLbTrsiqTyzmyJousV6/+4sfTYt8/"
    "tz0gMt3Qaddvs+BpTTrYIKTpsGYwPkKDqUdEkC87OQ6a2mXB1lpA7FMpZiiyJ1HpIXHkd+"
    "eLoNkQKBgQDlQtZdTlHu0YLM2WlORFYP8zyx/"
    "9k6rXZVVZLNBcZazEUkcgXJ7MS8pXieiXo6UJEf0PNCCY21ooOMPq6LI0NNF/"
    "yDbvT9Ri7eRNZRZosXIDaB17S93vgZ4Ukri7JJv3Bx+V2Wwl3P2/"
    "g080shqx8ZZsg1aQqpKov9qsavyXAuGQKBgQD0gh2P+rt0jQ9SnJ8ETGzFAXmhZyt4my+"
    "vBuFX8tFvByxMiUIxzsmHxCA6td/6KCWPwoo3xhe7r/"
    "+NHEFe6k9NR3KgrHOrHFcCqOAHWYPuPCKaG0ycbEF3tjzuDbZUJzH4WBgQDEsPlwDYEpkR17"
    "FX6sIMaAPMRvlOcZQZqnXRWQKBgQC6t0+c2E+UYB/"
    "WNG82ZiNthB13nrbNuj54y2PvBHgCtQDO6OpcBTBJr75n5/"
    "GbEsjPD78+lkdKmdvnWZmQCh0i6ZkndjOjHwjGz2t5CjnXkM2zu/kg9jo74aZVB8Yhl/"
    "+9Y2lcglojErS4czlKZ3LBnlsKXM1o7xTqeK6utjFd6QKBgQCAaH0CClm8IgC0EBDq/v/"
    "4jofEHhyUYFuwfdqGh705o/i90S/"
    "0XHc2V+fdLXsNM1xWnYJdPClmpk19XCNwp3kySp2GiErOyDlh6jKNaZOB4A8EA+Y+"
    "GBRhvFFPa+AfXd4+YHVyqCIbc+A7mbjNyAsY8u8p+"
    "M5Vz8hKTBfNStpJMQKBgH6YBNtXqyPDXgqyvRkjZt01I2ZYn2iIHBhHyDWE5j0QjhzF2kLpK"
    "IASO1uOTsaurAhPuyUTR+8EXu2OSP3RoaJ5buwUb8yL2b8Q075WCSp+eXdU6ZmEYdfnKX+"
    "63I9QIWvTFsREZybFR4tPgsQJGq/L8jNIxDxG9D0P1I2Zxrfd";

enum class Method {
  kGenerateRsaKey,
  kGenerateEcKey,
  kImportKey,
  kRunImportCertFromBytesUseRandomInput,
  kRunImportCertFromBytesUseValidCert,
  kImportX509Cert,
  // TODO(244408716): Enable when the methods are implemented.
  // kImportPkcs12Cert,
  // kExportPkcs12Cert,
  kRemoveKeyAndCerts,
  kRemoveCert,
  kListKeys,
  kListCerts,
  kDoesPrivateKeyExist,
  kSign,
  kSignRsaPkcs1Digest,
  kRunSignRsaPkcs1DigestAndVerifySignature,
  kGetAvailableTokens,
  kGetTokenInfo,
  kGetKeyInfo,
  kGetKeyPermissions,
  kGetCertProvisioningProfileId,
  kSetKeyNickname,
  kSetKeyPermissions,
  kSetCertProvisioningProfileId,
  kMaxValue = kSetCertProvisioningProfileId,
};

// Test-only overloads for better errors from EXPECT_EQ, etc.
std::ostream& operator<<(std::ostream& stream, Error val) {
  stream << static_cast<int>(val);
  return stream;
}
std::ostream& operator<<(std::ostream& stream, Token val) {
  stream << static_cast<int>(val);
  return stream;
}

struct Environment {
  Environment() {
    base::test::AllowCheckIsTestForTesting();
    base::CommandLine::Init(0, nullptr);
    TestTimeouts::Initialize();
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

base::span<const uint8_t> GetCertData(
    const scoped_refptr<net::X509Certificate>& cert) {
  return net::x509_util::CryptoBufferAsSpan(cert->cert_buffer());
}
base::span<const uint8_t> GetCertData(const scoped_refptr<const Cert>& cert) {
  return GetCertData(cert->GetX509Cert());
}

struct FuzzHash {
  using is_transparent = void;

  size_t operator()(const PublicKeySpki& spki) const {
    return base::FastHash(spki.value());
  }
  size_t operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return base::FastHash(GetCertData(cert));
  }
  size_t operator()(const scoped_refptr<const Cert>& cert) const {
    return operator()(cert->GetX509Cert());
  }
};

struct FuzzEqual {
  using is_transparent = void;

  bool operator()(const scoped_refptr<net::X509Certificate>& a,
                  const scoped_refptr<net::X509Certificate>& b) const {
    return base::ranges::equal(GetCertData(a), GetCertData(b));
  }
  bool operator()(const scoped_refptr<net::X509Certificate>& a,
                  const scoped_refptr<const Cert>& b) const {
    return base::ranges::equal(GetCertData(a), GetCertData(b));
  }
  bool operator()(const scoped_refptr<const Cert>& a,
                  const scoped_refptr<net::X509Certificate>& b) const {
    return base::ranges::equal(GetCertData(a), GetCertData(b));
  }
  bool operator()(const scoped_refptr<const Cert>& a,
                  const scoped_refptr<const Cert>& b) const {
    return base::ranges::equal(GetCertData(a), GetCertData(b));
  }
};

// Enable testing::UnorderedElementsAreArray to compare against pointers (to
// avoid unnecessary copying).
bool operator==(const PublicKey& a, const PublicKey* b_ptr) {
  CHECK(b_ptr);
  return a == *b_ptr;
}

// A wrapper around `PublicKey` that also stores related certs and attributes
// for convenience.
struct FuzzKey {
  FuzzKey(PublicKey pub_key,
          Token token,
          KeyType type,
          std::optional<RsaModulusLength> rsa_key_size,
          bool can_be_listed)
      : public_key(std::move(pub_key)),
        token(token),
        key_type(type),
        rsa_key_size(rsa_key_size),
        can_be_listed(can_be_listed) {
    // NSS sets an empty nickname by default, this doesn't have to be like this
    // in general.
    nickname = "";
    // Custom attributes are stored differently in tests and have
    // empty values by default.
    key_permissions = chaps::KeyPermissions();
    cert_provisioning_profile_id = "";
  }
  FuzzKey(FuzzKey&&) = default;
  FuzzKey& operator=(FuzzKey&&) = default;
  FuzzKey(const FuzzKey&) = delete;
  auto operator=(const FuzzKey&) = delete;

  PublicKey public_key;
  Token token;
  KeyType key_type;
  std::optional<RsaModulusLength> rsa_key_size;
  // Contains imported net::X509Certificate certs. The corresponding kcer::Cert
  // certs will be found on the next ListCerts (from the related token) and
  // pending certs will be "converted" into kcer::Cert certs and stored in
  // `certs`. The fuzzer uses unordered maps (and not a base::flat_set as the
  // Kcer itself) because the fuzzer might theoretically work with more certs
  // than an average user.
  std::unordered_set<scoped_refptr<net::X509Certificate>, FuzzHash, FuzzEqual>
      pending_certs;
  std::unordered_set<scoped_refptr<const Cert>, FuzzHash, FuzzEqual> certs;
  // TODO(miersh): This is hacky, but NSS doesn't seem to create public key
  // objects for imported keys, and they cannot be found by the "list" method
  // because of that. This should be fixed in Kcer-without-NSS.
  bool can_be_listed = true;
  // TODO(miersh): NSS copies the nickname from a cert into its key in some
  // cases. Kcer-without-NSS won't do that. For now for simplicity the nickname
  // is only checked after a SetNickname() call, and not after importing certs.
  bool nickname_known = false;
  std::optional<std::string> nickname;
  std::optional<chaps::KeyPermissions> key_permissions;
  std::optional<std::string> cert_provisioning_profile_id;
};

//==============================================================================

class CertGenerator {
 public:
  CertGenerator(FuzzedDataProvider& data_provider,
                base::span<const uint8_t> public_key_spki);
  // Returns a randomized (possibly not valid) certificate for
  // `public_key_spki`. The current implementation allows only a single call to
  // this method per instance of the class.
  scoped_refptr<net::X509Certificate> GetX509Cert();

 private:
  inline bool GetBool();
  inline int GetInt();
  inline uint64_t GetUint64();
  inline std::string GetString();
  inline std::vector<uint8_t> GetBytes();
  inline GURL GetGurl();
  inline net::IPAddress GetIpAddress();
  std::vector<bssl::KeyUsageBit> GetKeyUsages();

  void GenerateCert();

  // The current implementation allows only a single call to the GetX509Cert().
  // Not a hard requirement, can be changed if needed.
  bool can_be_used_ = true;
  const raw_ref<FuzzedDataProvider> data_provider_;
  base::span<const uint8_t> public_key_spki_;
  std::unique_ptr<net::CertBuilder> issuer_;
  std::unique_ptr<net::CertBuilder> cert_builder_;
};

CertGenerator::CertGenerator(FuzzedDataProvider& data_provider,
                             base::span<const uint8_t> public_key_spki)
    : data_provider_(data_provider), public_key_spki_(public_key_spki) {}

scoped_refptr<net::X509Certificate> CertGenerator::GetX509Cert() {
  CHECK(can_be_used_);
  can_be_used_ = false;
  GenerateCert();
  return cert_builder_->GetX509Certificate();
}

bool CertGenerator::GetBool() {
  // FuzzedDataProvider is expected to return false from ConsumeBool() when
  // there's no remaining bytes, but make it more explicit since GenerateCert()
  // relies on that.
  return (data_provider_->remaining_bytes() > 0) &&
         data_provider_->ConsumeBool();
}

int CertGenerator::GetInt() {
  return data_provider_->ConsumeIntegral<int>();
}

uint64_t CertGenerator::GetUint64() {
  return data_provider_->ConsumeIntegral<uint64_t>();
}

std::string CertGenerator::GetString() {
  return data_provider_->ConsumeRandomLengthString();
}

inline std::vector<uint8_t> CertGenerator::GetBytes() {
  size_t length = data_provider_->ConsumeIntegralInRange<size_t>(
      0, /*max=*/data_provider_->remaining_bytes());
  return data_provider_->ConsumeBytes<uint8_t>(length);
}

inline GURL CertGenerator::GetGurl() {
  return GURL(data_provider_->ConsumeRandomLengthString());
}

inline net::IPAddress CertGenerator::GetIpAddress() {
  bool use_ip4 = GetBool();
  if (use_ip4) {
    return net::IPAddress(data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>());
  } else {
    return net::IPAddress(data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>(),
                          data_provider_->ConsumeIntegral<uint8_t>());
  }
}

std::vector<bssl::KeyUsageBit> CertGenerator::GetKeyUsages() {
  std::vector<bssl::KeyUsageBit> result;
  uint16_t key_usages = data_provider_->ConsumeIntegral<uint16_t>();
  if (key_usages & bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE) {
    result.push_back(bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE);
  }
  if (key_usages & bssl::KEY_USAGE_BIT_NON_REPUDIATION) {
    result.push_back(bssl::KEY_USAGE_BIT_NON_REPUDIATION);
  }
  if (key_usages & bssl::KEY_USAGE_BIT_KEY_ENCIPHERMENT) {
    result.push_back(bssl::KEY_USAGE_BIT_KEY_ENCIPHERMENT);
  }
  if (key_usages & bssl::KEY_USAGE_BIT_DATA_ENCIPHERMENT) {
    result.push_back(bssl::KEY_USAGE_BIT_DATA_ENCIPHERMENT);
  }
  if (key_usages & bssl::KEY_USAGE_BIT_KEY_AGREEMENT) {
    result.push_back(bssl::KEY_USAGE_BIT_KEY_AGREEMENT);
  }
  if (key_usages & bssl::KEY_USAGE_BIT_KEY_CERT_SIGN) {
    result.push_back(bssl::KEY_USAGE_BIT_KEY_CERT_SIGN);
  }
  if (key_usages & bssl::KEY_USAGE_BIT_CRL_SIGN) {
    result.push_back(bssl::KEY_USAGE_BIT_CRL_SIGN);
  }
  if (key_usages & bssl::KEY_USAGE_BIT_ENCIPHER_ONLY) {
    result.push_back(bssl::KEY_USAGE_BIT_ENCIPHER_ONLY);
  }
  if (key_usages & bssl::KEY_USAGE_BIT_DECIPHER_ONLY) {
    result.push_back(bssl::KEY_USAGE_BIT_DECIPHER_ONLY);
  }
  return result;
}

void CertGenerator::GenerateCert() {
  std::string issuer_common_name = data_provider_->ConsumeRandomLengthString();
  bool issuer_uses_rsa_key = GetBool();

  issuer_ = std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr,
                                               /*issuer=*/nullptr);
  issuer_->SetSubjectCommonName(std::move(issuer_common_name));
  if (issuer_uses_rsa_key) {
    issuer_->GenerateRSAKey();
  } else {
    issuer_->GenerateECKey();
  }

  cert_builder_ = net::CertBuilder::FromSubjectPublicKeyInfo(public_key_spki_,
                                                             issuer_.get());
  // Set some default values to increases the chances for a correct cert.
  cert_builder_->SetSignatureAlgorithm(
      issuer_uses_rsa_key ? bssl::SignatureAlgorithm::kRsaPkcs1Sha256
                          : bssl::SignatureAlgorithm::kEcdsaSha256);
  auto now = base::Time::Now();
  cert_builder_->SetValidity(now, now + base::Days(30));
  cert_builder_->SetSubjectCommonName("SubjectCommonName");
  cert_builder_->SetSerialNumber(data_provider_->ConsumeIntegral<uint64_t>());
  if ((data_provider_->remaining_bytes() == 0) || GetBool()) {
    return;
  }

  // Randomize the cert.
  if (GetBool()) {
    // RFC 5280 guarantees that these values are from [0,2].
    int version = data_provider_->ConsumeIntegralInRange(0, 2);
    cert_builder_->SetCertificateVersion(
        static_cast<bssl::CertificateVersion>(version));
  }
  if (GetBool()) {
    cert_builder_->ClearExtensions();
  }
  for (int i = data_provider_->ConsumeIntegralInRange(0, 100);
       (data_provider_->remaining_bytes() > 0) && (i > 0); --i) {
    std::string oid_str = GetString();
    std::string value = GetString();
    bool critical = GetBool();
    cert_builder_->SetExtension(bssl::der::Input(oid_str), std::move(value),
                                critical);
  }
  if (GetBool()) {
    cert_builder_->SetBasicConstraints(/*is_ca=*/GetBool(),
                                       /*path_len=*/GetInt());
  }
  if (GetBool()) {
    std::vector<std::string> permitted_dns_names;
    while (GetBool()) {
      permitted_dns_names.push_back(GetString());
    }
    std::vector<std::string> excluded_dns_names;
    while (GetBool()) {
      excluded_dns_names.push_back(GetString());
    }
    cert_builder_->SetNameConstraintsDnsNames(permitted_dns_names,
                                              excluded_dns_names);
  }
  if (GetBool()) {
    std::vector<GURL> ca_issuers_urls;
    while (GetBool()) {
      ca_issuers_urls.push_back(GetGurl());
    }
    std::vector<GURL> ocsp_urls;
    while (GetBool()) {
      ocsp_urls.push_back(GetGurl());
    }
    cert_builder_->SetCaIssuersAndOCSPUrls(ca_issuers_urls, ocsp_urls);
  }
  if (GetBool()) {
    std::vector<GURL> urls;
    while (GetBool()) {
      urls.emplace_back(GetString());
    }
    cert_builder_->SetCrlDistributionPointUrls(urls);
  }
  if (GetBool()) {
    cert_builder_->SetIssuerTLV(GetBytes());
  }
  if (GetBool()) {
    cert_builder_->SetSubjectCommonName(GetString());
  }
  if (GetBool()) {
    cert_builder_->SetSubjectTLV(GetBytes());
  }
  if (GetBool()) {
    std::vector<std::string> dns_names;
    while (GetBool()) {
      dns_names.push_back(GetString());
    }
    std::vector<net::IPAddress> ip_addresses;
    while (GetBool()) {
      ip_addresses.push_back(GetIpAddress());
    }
    cert_builder_->SetSubjectAltNames(dns_names, ip_addresses);
  }
  if (GetBool()) {
    std::vector<bssl::KeyUsageBit> key_usages = GetKeyUsages();
    if (!key_usages.empty()) {  // Empty not allowed.
      cert_builder_->SetKeyUsages(key_usages);
    }
  }
  if (GetBool()) {
    std::vector<std::string> memory_holder;
    std::vector<bssl::der::Input> purpose_oids;
    while (GetBool()) {
      memory_holder.push_back(GetString());
      purpose_oids.emplace_back(memory_holder.back());
    }
    if (!purpose_oids.empty()) {  // Empty not allowed.
      cert_builder_->SetExtendedKeyUsages(purpose_oids);
    }
  }
  if (GetBool()) {
    std::vector<std::string> policy_oids;
    while (GetBool()) {
      policy_oids.push_back(GetString());
    }
    cert_builder_->SetCertificatePolicies(policy_oids);
  }
  if (GetBool()) {
    std::vector<std::pair<std::string, std::string>> policy_mappings;
    while (GetBool()) {
      policy_mappings.emplace_back(GetString(), GetString());
    }
    cert_builder_->SetPolicyMappings(policy_mappings);
  }
  if (GetBool()) {
    std::optional<uint64_t> require_explicit_policy;
    if (GetBool()) {
      require_explicit_policy = GetUint64();
    }
    std::optional<uint64_t> inhibit_policy_mapping;
    if (GetBool()) {
      inhibit_policy_mapping = GetUint64();
    }
    cert_builder_->SetPolicyConstraints(require_explicit_policy,
                                        inhibit_policy_mapping);
  }
  if (GetBool()) {
    cert_builder_->SetInhibitAnyPolicy(/*skip_certs=*/GetUint64());
  }
  if (GetBool()) {
    base::Time not_before = base::Time() + base::Microseconds(GetUint64());
    base::Time not_after = base::Time() + base::Microseconds(GetUint64());
    cert_builder_->SetValidity(not_before, not_after);
  }
  if (GetBool()) {
    cert_builder_->SetSubjectKeyIdentifier(GetString());
  }
  if (GetBool()) {
    cert_builder_->SetAuthorityKeyIdentifier(GetString());
  }
  if (GetBool()) {
    cert_builder_->SetSignatureAlgorithm(
        data_provider_->ConsumeEnum<bssl::SignatureAlgorithm>());
  }
  if (GetBool()) {
    cert_builder_->SetSignatureAlgorithmTLV(GetString());
  }
  if (GetBool()) {
    cert_builder_->SetOuterSignatureAlgorithmTLV(GetString());
  }
  if (GetBool()) {
    cert_builder_->SetTBSSignatureAlgorithmTLV(GetString());
  }
}

//==============================================================================

class KcerFuzzer {
 public:
  KcerFuzzer(const uint8_t* data, size_t size) : data_provider_(data, size) {}

  void Run();

 private:
  void InitializeKcer();
  base::WeakPtr<internal::KcerToken> CreateToken(Token token);

  void RunNextMethod();
  void RunGenerateRsaKey();
  void RunGenerateEcKey();
  void RunImportKey();
  void RunImportCertFromBytesUseRandomInput();
  void RunImportCertFromBytesUseValidCert();
  void RunImportX509Cert();
  void RunRemoveKeyAndCerts();
  void RunRemoveCert();
  void RunListKeys();
  void RunListCerts();
  void RunDoesPrivateKeyExist();
  void RunSign();
  void RunSignRsaPkcs1Digest();
  void RunSignRsaPkcs1DigestAndVerifySignature();
  void RunGetAvailableTokens();
  void RunGetTokenInfo();
  void RunGetKeyInfo();
  void RunGetKeyPermissions();
  void RunGetCertProvisioningProfileId();
  void RunSetKeyNickname();
  void RunSetKeyPermissions();
  void RunSetCertProvisioningProfileId();

  // Returns a randomized set of tokens. Can return tokens that were not
  // initialized for the current instance of Kcer.
  base::flat_set<Token> SelectTokens();
  // Returns a pointer to a random FuzzKey from `key_data_`. The pointer is
  // invalidated on `kcer_data_` update. Returns nullptr if there are no
  // existing keys to choose from.
  FuzzKey* SelectFuzzKey();
  // Generates a `PrivateKeyHandle` that can be used to call Kcer methods.
  // `out_kcer_key` will contain a pointer to an existing FuzzKey corresponding
  // to the handle or nullptr (if the handle doesn't describe any existing key).
  // `out_kcer_key` is invalidated on `kcer_data_` update.
  PrivateKeyHandle GeneratePrivateKeyHandle(FuzzKey** out_kcer_key);

  size_t GetSizeT(size_t max);
  std::vector<uint8_t> GetBytes(size_t min);

  // Returns the enum value with the next id for `val`. The enum `T` must have
  // kMaxValue defined.
  template <typename T>
  T NextEnumValue(T val);

  bool keep_fuzzing_ = true;
  bool user_token_available_ = false;
  bool device_token_available_ = false;

  // Tracks whether the key from `kPkcs8Key` was already imported.
  bool example_pkcs8_key_used_ = false;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  MockHighLevelChapsClient chaps_client_;
  base::flat_map<Token, std::unique_ptr<TokenHolder>> available_tokens_;
  std::unique_ptr<Kcer> kcer_;
  // Keeps track of what Kcer is expected to contain.
  std::unordered_map<PublicKeySpki, FuzzKey, FuzzHash> kcer_data_;
  // A counter for the total number of certs in `kcer_data_`. Useful for
  // selecting a random cert.
  size_t certs_counter_ = 0;

  FuzzedDataProvider data_provider_;
};

void KcerFuzzer::Run() {
  InitializeKcer();

  while (!testing::Test::HasFailure() && keep_fuzzing_ &&
         data_provider_.remaining_bytes() > 0) {
    RunNextMethod();
  }
}

void KcerFuzzer::InitializeKcer() {
  base::WeakPtr<internal::KcerToken> user_token_ptr;
  user_token_available_ = data_provider_.ConsumeBool();
  if (user_token_available_) {
    user_token_ptr = CreateToken(Token::kUser);
  }

  base::WeakPtr<internal::KcerToken> device_token_ptr;
  device_token_available_ = data_provider_.ConsumeBool();
  if (device_token_available_) {
    device_token_ptr = CreateToken(Token::kDevice);
  }

  auto kcer = std::make_unique<kcer::internal::KcerImpl>();
  kcer->Initialize(content::GetIOThreadTaskRunner({}), user_token_ptr,
                   device_token_ptr);
  kcer_ = std::move(kcer);
}

base::WeakPtr<internal::KcerToken> KcerFuzzer::CreateToken(Token token) {
  available_tokens_[token] = std::make_unique<TokenHolder>(
      token, &chaps_client_, /*initialized=*/true);
  return available_tokens_[token]->GetWeakPtr();
}

void KcerFuzzer::RunNextMethod() {
  Method next_method = data_provider_.ConsumeEnum<Method>();
  switch (next_method) {
    case Method::kGenerateRsaKey:
      return RunGenerateRsaKey();
    case Method::kGenerateEcKey:
      return RunGenerateEcKey();
    case Method::kImportKey:
      return RunImportKey();
    case Method::kRunImportCertFromBytesUseRandomInput:
      return RunImportCertFromBytesUseRandomInput();
    case Method::kRunImportCertFromBytesUseValidCert:
      return RunImportCertFromBytesUseValidCert();
    case Method::kImportX509Cert:
      return RunImportX509Cert();
    case Method::kRemoveKeyAndCerts:
      return RunRemoveKeyAndCerts();
    case Method::kRemoveCert:
      return RunRemoveCert();
    case Method::kListKeys:
      return RunListKeys();
    case Method::kListCerts:
      return RunListCerts();
    case Method::kDoesPrivateKeyExist:
      return RunDoesPrivateKeyExist();
    case Method::kSign:
      return RunSign();
    case Method::kSignRsaPkcs1Digest:
      return RunSignRsaPkcs1Digest();
    case Method::kRunSignRsaPkcs1DigestAndVerifySignature:
      return RunSignRsaPkcs1DigestAndVerifySignature();
    case Method::kGetAvailableTokens:
      return RunGetAvailableTokens();
    case Method::kGetTokenInfo:
      return RunGetTokenInfo();
    case Method::kGetKeyInfo:
      return RunGetKeyInfo();
    case Method::kGetKeyPermissions:
      return RunGetKeyPermissions();
    case Method::kGetCertProvisioningProfileId:
      return RunGetCertProvisioningProfileId();
    case Method::kSetKeyNickname:
      return RunSetKeyNickname();
    case Method::kSetKeyPermissions:
      return RunSetKeyPermissions();
    case Method::kSetCertProvisioningProfileId:
      return RunSetCertProvisioningProfileId();
  }
}

void KcerFuzzer::RunGenerateRsaKey() {
  Token token = data_provider_.ConsumeEnum<Token>();
  RsaModulusLength modulus_length_bits = data_provider_.ConsumeBool()
                                             ? RsaModulusLength::k1024
                                             : RsaModulusLength::k2048;
  // TODO(miersh): Generating software-backed keys requires d-bus communication
  // with Chaps. Figure out how to simulate that for the fuzzer.
  bool hardware_backed = true;
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateRsaKey(token, modulus_length_bits, hardware_backed,
                        generate_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(generate_waiter.Get().has_value());
    EXPECT_EQ(generate_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  ASSERT_TRUE(generate_waiter.Get().has_value());
  PublicKey public_key = generate_waiter.Take().value();
  PublicKeySpki spki = public_key.GetSpki();
  EXPECT_GE(public_key.GetPkcs11Id()->size(), 4u);
  EXPECT_LE(public_key.GetPkcs11Id()->size(), base::kSHA1Length);
  EXPECT_GE(spki->size(), 4u);
  EXPECT_EQ(public_key.GetToken(), token);

  kcer_data_.emplace(
      std::move(spki),
      FuzzKey(std::move(public_key), token, KeyType::kRsa, modulus_length_bits,
              /*can_be_listed=*/true));
}

void KcerFuzzer::RunGenerateEcKey() {
  Token token = data_provider_.ConsumeEnum<Token>();
  EllipticCurve elliptic_curve = EllipticCurve::kP256;
  // TODO(miersh): Generating software-backed keys requires d-bus communication
  // with Chaps. Figure out how to simulate that for the fuzzer.
  bool hardware_backed = true;

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(token, elliptic_curve, hardware_backed,
                       generate_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(generate_waiter.Get().has_value());
    EXPECT_EQ(generate_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  ASSERT_TRUE(generate_waiter.Get().has_value());
  PublicKey public_key = generate_waiter.Take().value();
  PublicKeySpki spki = public_key.GetSpki();
  EXPECT_GE(public_key.GetPkcs11Id()->size(), 4u);
  EXPECT_LE(public_key.GetPkcs11Id()->size(), base::kSHA1Length);
  EXPECT_GE(spki->size(), 4u);
  EXPECT_EQ(public_key.GetToken(), token);

  kcer_data_.emplace(std::move(spki),
                     FuzzKey(std::move(public_key), token, KeyType::kEcc,
                             /*rsa_key_size=*/std::nullopt,
                             /*can_be_listed=*/true));
}

void KcerFuzzer::RunImportKey() {
  Token token = data_provider_.ConsumeEnum<Token>();

  std::vector<uint8_t> pkcs8_key;
  bool good_key_is_used = false;
  if (!example_pkcs8_key_used_ && data_provider_.ConsumeBool()) {
    std::optional<std::vector<uint8_t>> key_der = base::Base64Decode(kPkcs8Key);
    ASSERT_TRUE(key_der.has_value());
    pkcs8_key = std::move(key_der).value();
    example_pkcs8_key_used_ = true;
    good_key_is_used = true;
  } else {
    pkcs8_key = GetBytes(/*min=*/0);
  }

  base::test::TestFuture<base::expected<PublicKey, Error>> import_key_waiter;
  kcer_->ImportKey(token, Pkcs8PrivateKeyInfoDer(std::move(pkcs8_key)),
                   import_key_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(import_key_waiter.Get().has_value());
    EXPECT_EQ(import_key_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  if (good_key_is_used) {
    EXPECT_TRUE(import_key_waiter.Get().has_value());
    PublicKey public_key = import_key_waiter.Take().value();
    PublicKeySpki spki = public_key.GetSpki();

    kcer_data_.emplace(std::move(spki),
                       FuzzKey(std::move(public_key), token, KeyType::kRsa,
                               /*rsa_key_size=*/std::nullopt,
                               /*can_be_listed=*/false));
    return;
  }

  if (import_key_waiter.Get().has_value()) {
    // TODO(miersh): Ideally the fuzzer would figure out the type of the key and
    // add it to `kcer_data_`. But the chances of randomly finding a good key
    // are quite low. For simplicity just finish the fuzzer iteration with
    // success.
    keep_fuzzing_ = false;
    return;
  }
}

// Runs `ImportCertFromBytes()` with random bytes as an input. It's generally
// not expected that the bytes will encode a correct certificate.
void KcerFuzzer::RunImportCertFromBytesUseRandomInput() {
  Token token = data_provider_.ConsumeEnum<Token>();
  CertDer cert(GetBytes(/*min=*/0));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportCertFromBytes(token, cert, import_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(import_waiter.Get().has_value());
    EXPECT_EQ(import_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  // `cert` is probably incorrect, just fuzz for crashes here. Also see
  // RunImportCertFromBytesUseValidCert().
  EXPECT_TRUE(import_waiter.Wait());

  if (import_waiter.Get().has_value()) {
    scoped_refptr<net::X509Certificate> x509_cert =
        net::X509Certificate::CreateFromBytes(std::move(cert).value());
    // If Kcer imported it, it's expected to be parsable.
    EXPECT_TRUE(x509_cert);
    // TODO(miersh): The chances of randomly finding a cert are quite low. For
    // simplicity just finish the fuzzer iteration with success. Ideally the
    // fuzzer would extract the public key from the cert and assign the cert to
    // the correct FuzzKey in `kcer_data_`,
    // net/cert/asn1_util.h:ExtractSPKIFromDERCert() can potentially be used for
    // that.
    keep_fuzzing_ = false;
    return;
  }
}

// Generates a certificate for an existing key, encodes it as bytes and runs
// `ImportCertFromBytes()` using the bytes as an input. The certificate is
// randomized and can be invalid, but the general structure of the certificate
// will be valid and the fuzzer is expected to find valid certificates quite
// often.
void KcerFuzzer::RunImportCertFromBytesUseValidCert() {
  Token token = data_provider_.ConsumeEnum<Token>();
  FuzzKey* key = SelectFuzzKey();
  if (!key) {
    return;
  }
  CertGenerator cert_generator(data_provider_,
                               key->public_key.GetSpki().value());
  scoped_refptr<net::X509Certificate> cert = cert_generator.GetX509Cert();
  if (!cert) {
    return;
  }

  base::span<const uint8_t> cert_data = GetCertData(cert);
  CertDer cert_der(std::vector<uint8_t>(cert_data.begin(), cert_data.end()));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportCertFromBytes(token, std::move(cert_der),
                             import_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(import_waiter.Get().has_value());
    EXPECT_EQ(import_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  if (key->token != token) {
    ASSERT_FALSE(import_waiter.Get().has_value());
    EXPECT_EQ(import_waiter.Get().error(), Error::kKeyNotFound);
    return;
  }

  if (import_waiter.Get().has_value()) {
    key->nickname_known = false;
    key->pending_certs.insert(cert);
  }
}

void KcerFuzzer::RunImportX509Cert() {
  Token token = data_provider_.ConsumeEnum<Token>();
  FuzzKey* key = SelectFuzzKey();
  if (!key) {
    return;
  }
  CertGenerator cert_generator(data_provider_,
                               key->public_key.GetSpki().value());
  scoped_refptr<net::X509Certificate> cert = cert_generator.GetX509Cert();
  if (!cert) {
    return;
  }

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportX509Cert(token, cert, import_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(import_waiter.Get().has_value());
    EXPECT_EQ(import_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  if (key->token != token) {
    ASSERT_FALSE(import_waiter.Get().has_value());
    EXPECT_EQ(import_waiter.Get().error(), Error::kKeyNotFound);
    return;
  }

  if (import_waiter.Get().has_value()) {
    key->nickname_known = false;
    key->pending_certs.insert(cert);
  }
}

void KcerFuzzer::RunRemoveKeyAndCerts() {
  FuzzKey* key;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&key);

  base::test::TestFuture<base::expected<void, Error>> remove_key_waiter;
  kcer_->RemoveKeyAndCerts(key_handle, remove_key_waiter.GetCallback());

  if (available_tokens_.empty() ||
      (key_handle.GetTokenInternal().has_value() &&
       !base::Contains(available_tokens_,
                       key_handle.GetTokenInternal().value()))) {
    ASSERT_FALSE(remove_key_waiter.Get().has_value());
    EXPECT_EQ(remove_key_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  if (!key) {
    EXPECT_FALSE(remove_key_waiter.Get().has_value());
    return;
  }
  EXPECT_TRUE(remove_key_waiter.Get().has_value())
      << remove_key_waiter.Get().error();

  certs_counter_ -= key->certs.size();
  EXPECT_EQ(1u, kcer_data_.erase(key->public_key.GetSpki()));
}

void KcerFuzzer::RunRemoveCert() {
  if (certs_counter_ == 0) {
    return;
  }

  // This index describes an N-th cert across all keys.
  size_t cert_idx = GetSizeT(certs_counter_ - 1);
  // Iterate over all keys, count certs until the key holding the selected cert
  // is found.
  FuzzKey* key = nullptr;
  size_t certs_counted = 0;
  for (auto& [spki, kcer_key] : kcer_data_) {
    if (certs_counted + kcer_key.certs.size() <= cert_idx) {
      certs_counted += kcer_key.certs.size();
      continue;
    }
    key = &kcer_key;
    // Convert `cert_idx` into a local index for `key` (instead of a global
    // one for all the keys).
    cert_idx -= certs_counted;
    break;
  }
  ASSERT_TRUE(key && (cert_idx < key->certs.size()));

  auto cert_iter = key->certs.begin();
  std::advance(cert_iter, cert_idx);

  base::test::TestFuture<base::expected<void, Error>> remove_cert_waiter;
  kcer_->RemoveCert(*cert_iter, remove_cert_waiter.GetCallback());

  EXPECT_TRUE(remove_cert_waiter.Get().has_value());

  // Remove the cert from `kcer_data_`.
  key->certs.erase(cert_iter);
  certs_counter_--;
}

void KcerFuzzer::RunListKeys() {
  base::flat_set<Token> tokens = SelectTokens();

  std::vector<const PublicKey*> expected_result;
  for (const auto& [spki, kcer_key] : kcer_data_) {
    if (base::Contains(tokens, kcer_key.public_key.GetToken()) &&
        kcer_key.can_be_listed) {
      expected_result.push_back(&kcer_key.public_key);
    }
  }

  base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
      list_waiter;
  kcer_->ListKeys(std::move(tokens), list_waiter.GetCallback());
  EXPECT_THAT(list_waiter.Get<0>(), UnorderedElementsAreArray(expected_result));

  for (Token token : tokens) {
    if (!base::Contains(available_tokens_, token)) {
      EXPECT_TRUE(list_waiter.Get<1>().at(token) ==
                  Error::kTokenIsNotAvailable);
    }
  }
}

void KcerFuzzer::RunListCerts() {
  base::flat_set<Token> tokens = SelectTokens();

  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      list_certs_waiter;
  kcer_->ListCerts(tokens, list_certs_waiter.GetCallback());

  const std::vector<scoped_refptr<const Cert>>& certs =
      list_certs_waiter.Get<0>();
  std::unordered_set<scoped_refptr<const Cert>, FuzzHash, FuzzEqual>
      listed_certs(certs.begin(), certs.end());

  for (auto& [spki, kcer_key] : kcer_data_) {
    // Skip data that is on unrelated tokens and should not have been found.
    if (!base::Contains(tokens, kcer_key.public_key.GetToken())) {
      continue;
    }
    // Check that all known certs are found. Remove matched certs from the
    // `listed_certs`.
    for (const scoped_refptr<const Cert>& cert : kcer_key.certs) {
      auto iter = listed_certs.find(cert);
      EXPECT_TRUE(iter != listed_certs.end());
      listed_certs.erase(iter);
    }
    // Check that all pending certs are found. Replace pending
    // net::X509Certificate objects with the found kcer::Cert alternatives.
    // Found certs are extracted (i.e. removed) from `listed_certs`.
    for (const scoped_refptr<net::X509Certificate>& cert :
         kcer_key.pending_certs) {
      auto iter = listed_certs.find(cert);
      EXPECT_TRUE(iter != listed_certs.end());
      scoped_refptr<const Cert> found_cert =
          std::move(listed_certs.extract(iter)).value();
      // kcer_certs_.insert(found_cert);
      kcer_key.certs.insert(std::move(found_cert));
      certs_counter_++;
    }
    // All pending certs were found and replaced, the set can be cleared
    // now.
    kcer_key.pending_certs.clear();
  }
  // If `listed_certs` are not empty by this point, some additional unexpected
  // certs were found.
  EXPECT_TRUE(listed_certs.empty());

  const base::flat_map<Token, Error>& errors = list_certs_waiter.Get<1>();
  for (const auto& [token, error] : errors) {
    if (error == Error::kTokenIsNotAvailable) {
      EXPECT_FALSE(base::Contains(available_tokens_, token));
    } else {
      // Other errors are not expected.
      ADD_FAILURE();
    }
  }
}

void KcerFuzzer::RunDoesPrivateKeyExist() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);

  base::test::TestFuture<base::expected<bool, Error>> key_exist_waiter;
  kcer_->DoesPrivateKeyExist(key_handle, key_exist_waiter.GetCallback());

  if (available_tokens_.empty() ||
      (key_handle.GetTokenInternal().has_value() &&
       !base::Contains(available_tokens_,
                       key_handle.GetTokenInternal().value()))) {
    ASSERT_FALSE(key_exist_waiter.Get().has_value());
    EXPECT_EQ(key_exist_waiter.Get().error(), Error::kTokenIsNotAvailable);

    return;
  }

  if (expected_key) {
    ASSERT_TRUE(key_exist_waiter.Get().has_value());
    EXPECT_TRUE(key_exist_waiter.Get().value());
  } else {
    EXPECT_TRUE(!key_exist_waiter.Get().has_value() ||
                !key_exist_waiter.Get().value());
  }
}

std::vector<SigningScheme> GetAllSigningSchemes() {
  std::vector<SigningScheme> all_schemes;
  all_schemes.reserve(10);
  // Effectively unused.
  SigningScheme a = SigningScheme::kRsaPkcs1Sha1;
  // Use switch-case to ensure that all values are handled.
  switch (a) {
    case SigningScheme::kRsaPkcs1Sha1:
      all_schemes.push_back(SigningScheme::kRsaPkcs1Sha1);
      [[fallthrough]];
    case SigningScheme::kRsaPkcs1Sha256:
      all_schemes.push_back(SigningScheme::kRsaPkcs1Sha256);
      [[fallthrough]];
    case SigningScheme::kRsaPkcs1Sha384:
      all_schemes.push_back(SigningScheme::kRsaPkcs1Sha384);
      [[fallthrough]];
    case SigningScheme::kRsaPkcs1Sha512:
      all_schemes.push_back(SigningScheme::kRsaPkcs1Sha512);
      [[fallthrough]];
    case SigningScheme::kEcdsaSecp256r1Sha256:
      all_schemes.push_back(SigningScheme::kEcdsaSecp256r1Sha256);
      [[fallthrough]];
    case SigningScheme::kEcdsaSecp384r1Sha384:
      all_schemes.push_back(SigningScheme::kEcdsaSecp384r1Sha384);
      [[fallthrough]];
    case SigningScheme::kEcdsaSecp521r1Sha512:
      all_schemes.push_back(SigningScheme::kEcdsaSecp521r1Sha512);
      [[fallthrough]];
    case SigningScheme::kRsaPssRsaeSha256:
      all_schemes.push_back(SigningScheme::kRsaPssRsaeSha256);
      [[fallthrough]];
    case SigningScheme::kRsaPssRsaeSha384:
      all_schemes.push_back(SigningScheme::kRsaPssRsaeSha384);
      [[fallthrough]];
    case SigningScheme::kRsaPssRsaeSha512:
      all_schemes.push_back(SigningScheme::kRsaPssRsaeSha512);
  }
  return all_schemes;
}

bool DoesKeyTypeMatchSigningScheme(KeyType key_type,
                                   SigningScheme signing_scheme) {
  switch (signing_scheme) {
    case SigningScheme::kRsaPkcs1Sha1:
    case SigningScheme::kRsaPkcs1Sha256:
    case SigningScheme::kRsaPkcs1Sha384:
    case SigningScheme::kRsaPkcs1Sha512:
    case SigningScheme::kRsaPssRsaeSha256:
    case SigningScheme::kRsaPssRsaeSha384:
    case SigningScheme::kRsaPssRsaeSha512:
      return (key_type == KeyType::kRsa);
    case SigningScheme::kEcdsaSecp256r1Sha256:
    case SigningScheme::kEcdsaSecp384r1Sha384:
    case SigningScheme::kEcdsaSecp521r1Sha512:
      return (key_type == KeyType::kEcc);
  }
}

void KcerFuzzer::RunSign() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);

  // Cannot use ConsumeEnum() because the values are not sequential.
  static std::vector<SigningScheme> all_schemes = GetAllSigningSchemes();
  SigningScheme signing_scheme =
      all_schemes.at(GetSizeT(all_schemes.size() - 1));

  DataToSign data_to_sign(GetBytes(/*min=*/0));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  kcer_->Sign(key_handle, signing_scheme, data_to_sign,
              sign_waiter.GetCallback());

  if (!expected_key) {
    EXPECT_FALSE(sign_waiter.Get().has_value());
    return;
  }

  if (sign_waiter.Get().has_value()) {
    // Not strict, i.e. silently skip verification for signing schemes if it's
    // not implemented yet.
    EXPECT_TRUE(VerifySignature(
        signing_scheme, expected_key->public_key.GetSpki(), data_to_sign,
        sign_waiter.Get().value(), /*strict=*/false));
    return;
  }

  if (sign_waiter.Get().error() == Error::kKeyDoesNotSupportSigningScheme) {
    EXPECT_FALSE(
        DoesKeyTypeMatchSigningScheme(expected_key->key_type, signing_scheme));
    return;
  }

  if (expected_key->rsa_key_size.has_value() &&
      (expected_key->rsa_key_size.value() == RsaModulusLength::k1024) &&
      (signing_scheme == SigningScheme::kRsaPssRsaeSha512)) {
    // The key is too small, a failure is expected.
    return;
  }

  // No other errors is expected.
  ADD_FAILURE() << "Unexpected error: " << sign_waiter.Get().error();
}

// Runs SignRsaPkcs1Raw() with a random input, most likely incorrect one.
// Primarily check for crashes.
void KcerFuzzer::RunSignRsaPkcs1Digest() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);

  DigestWithPrefix digest_with_prefix(GetBytes(/*min=*/0));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  kcer_->SignRsaPkcs1Raw(key_handle, std::move(digest_with_prefix),
                         sign_waiter.GetCallback());

  if (!expected_key) {
    EXPECT_FALSE(sign_waiter.Get().has_value());

    return;
  }

  // Can't check the signature without knowing the original data. Just fuzz for
  // crashes here. Also see RunSignRsaPkcs1DigestAndVerifySignature().
  EXPECT_TRUE(sign_waiter.Wait());
}

// Runs SignRsaPkcs1Raw() with a correct DigestWithPrefix and verifies the
// signature for it.
void KcerFuzzer::RunSignRsaPkcs1DigestAndVerifySignature() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);

  DataToSign data_to_sign(GetBytes(/*min=*/0));

  auto hasher = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  hasher->Update(data_to_sign->data(), data_to_sign->size());
  std::vector<uint8_t> hash(hasher->GetHashLength());
  hasher->Finish(hash.data(), hash.size());
  DigestWithPrefix digest_with_prefix(PrependSHA256DigestInfo(hash));

  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  kcer_->SignRsaPkcs1Raw(key_handle, std::move(digest_with_prefix),
                         sign_waiter.GetCallback());

  if (!expected_key) {
    EXPECT_FALSE(sign_waiter.Get().has_value());

    return;
  }

  if (sign_waiter.Get().has_value()) {
    EXPECT_TRUE(VerifySignature(SigningScheme::kRsaPkcs1Sha256,
                                expected_key->public_key.GetSpki(),
                                data_to_sign, sign_waiter.Get().value()));
  }
}

void KcerFuzzer::RunGetAvailableTokens() {
  base::test::TestFuture<base::flat_set<Token>> get_tokens_waiter;
  kcer_->GetAvailableTokens(get_tokens_waiter.GetCallback());
  const base::flat_set<Token>& available_tokens = get_tokens_waiter.Get();

  for (const auto& [expected_token, v] : available_tokens_) {
    EXPECT_TRUE(base::Contains(available_tokens, expected_token));
  }
}

void KcerFuzzer::RunGetTokenInfo() {
  Token token = data_provider_.ConsumeEnum<Token>();

  base::test::TestFuture<base::expected<TokenInfo, Error>> token_info_waiter;
  kcer_->GetTokenInfo(token, token_info_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(token_info_waiter.Get().has_value());
    EXPECT_EQ(token_info_waiter.Get().error(), Error::kTokenIsNotAvailable);

    return;
  }

  ASSERT_TRUE(token_info_waiter.Get().has_value());
  const TokenInfo& token_info = token_info_waiter.Get().value();
  // These values don't have to be exactly like this, they are what a software
  // NSS slot returns in tests. Still useful to test that they are not
  // completely off.
  EXPECT_THAT(token_info.pkcs11_id, testing::Lt(1000u));
  EXPECT_THAT(token_info.token_name,
              testing::StartsWith("NSS Application Slot"));
  EXPECT_EQ(token_info.module_name, "NSS Internal PKCS #11 Module");
}

void KcerFuzzer::RunGetKeyInfo() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);

  base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
  kcer_->GetKeyInfo(key_handle, key_info_waiter.GetCallback());

  if (available_tokens_.empty() ||
      (key_handle.GetTokenInternal().has_value() &&
       !base::Contains(available_tokens_,
                       key_handle.GetTokenInternal().value()))) {
    ASSERT_FALSE(key_info_waiter.Get().has_value());
    EXPECT_EQ(key_info_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  if (!expected_key) {
    EXPECT_FALSE(key_info_waiter.Get().has_value());
    return;
  }
  ASSERT_TRUE(key_info_waiter.Get().has_value());
  const KeyInfo& key_info = key_info_waiter.Get().value();

  // Software-backed keys are never generated in the current implementation.
  EXPECT_EQ(key_info.is_hardware_backed, true);
  EXPECT_EQ(key_info.key_type, expected_key->key_type);

  if (expected_key->nickname_known) {
    EXPECT_EQ(key_info.nickname, expected_key->nickname);
  }
}

void KcerFuzzer::RunGetKeyPermissions() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);

  base::test::TestFuture<
      base::expected<std::optional<chaps::KeyPermissions>, Error>>
      key_permissions_waiter;
  kcer_->GetKeyPermissions(key_handle, key_permissions_waiter.GetCallback());

  if (available_tokens_.empty() ||
      (key_handle.GetTokenInternal().has_value() &&
       !base::Contains(available_tokens_,
                       key_handle.GetTokenInternal().value()))) {
    ASSERT_FALSE(key_permissions_waiter.Get().has_value());
    EXPECT_EQ(key_permissions_waiter.Get().error(),
              Error::kTokenIsNotAvailable);
    return;
  }

  if (!expected_key) {
    EXPECT_FALSE(key_permissions_waiter.Get().has_value());
    return;
  }
  ASSERT_TRUE(key_permissions_waiter.Get().has_value());
  EXPECT_TRUE(ExpectKeyPermissionsEqual(key_permissions_waiter.Get().value(),
                                        expected_key->key_permissions));
}

void KcerFuzzer::RunGetCertProvisioningProfileId() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);

  base::test::TestFuture<base::expected<std::optional<std::string>, Error>>
      cert_prov_waiter;
  kcer_->GetCertProvisioningProfileId(key_handle,
                                      cert_prov_waiter.GetCallback());

  if (available_tokens_.empty() ||
      (key_handle.GetTokenInternal().has_value() &&
       !base::Contains(available_tokens_,
                       key_handle.GetTokenInternal().value()))) {
    ASSERT_FALSE(cert_prov_waiter.Get().has_value());
    EXPECT_EQ(cert_prov_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  if (!expected_key) {
    EXPECT_FALSE(cert_prov_waiter.Get().has_value());
    return;
  }
  ASSERT_TRUE(cert_prov_waiter.Get().has_value());
  EXPECT_EQ(cert_prov_waiter.Get().value(),
            expected_key->cert_provisioning_profile_id);
}

void KcerFuzzer::RunSetKeyNickname() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);
  std::string nickname = data_provider_.ConsumeRandomLengthString();

  base::test::TestFuture<base::expected<void, Error>> set_nickname_waiter;
  kcer_->SetKeyNickname(std::move(key_handle), nickname,
                        set_nickname_waiter.GetCallback());
  if (!expected_key) {
    EXPECT_FALSE(set_nickname_waiter.Get().has_value());
    return;
  }

  EXPECT_TRUE(set_nickname_waiter.Get().has_value());
  expected_key->nickname_known = true;

  // Fuzzer can generate strings with null characters inside. Kcer is expected
  // to only handle the beginning of the string until the first null character.
  auto pos = nickname.find('\0');
  if (pos != std::string::npos) {
    nickname.resize(pos);
  }

  expected_key->nickname = nickname;
}

void KcerFuzzer::RunSetKeyPermissions() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);
  chaps::KeyPermissions key_permissions;
  key_permissions.mutable_key_usages()->set_arc(data_provider_.ConsumeBool());
  key_permissions.mutable_key_usages()->set_corporate(
      data_provider_.ConsumeBool());

  base::test::TestFuture<base::expected<void, Error>> set_permissions_waiter;
  kcer_->SetKeyPermissions(std::move(key_handle), key_permissions,
                           set_permissions_waiter.GetCallback());
  if (!expected_key) {
    EXPECT_FALSE(set_permissions_waiter.Get().has_value());

    return;
  }

  EXPECT_TRUE(set_permissions_waiter.Get().has_value());
  expected_key->key_permissions = key_permissions;
}

void KcerFuzzer::RunSetCertProvisioningProfileId() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);
  std::string cert_prov_id = data_provider_.ConsumeRandomLengthString();

  base::test::TestFuture<base::expected<void, Error>> set_cert_prov_id_waiter;
  kcer_->SetCertProvisioningProfileId(std::move(key_handle), cert_prov_id,
                                      set_cert_prov_id_waiter.GetCallback());
  if (!expected_key) {
    EXPECT_FALSE(set_cert_prov_id_waiter.Get().has_value());

    return;
  }

  EXPECT_TRUE(set_cert_prov_id_waiter.Get().has_value());
  expected_key->cert_provisioning_profile_id = cert_prov_id;
}

base::flat_set<Token> KcerFuzzer::SelectTokens() {
  base::flat_set<Token> tokens;
  // Assume there are only two different tokens.
  static_assert(static_cast<int>(Token::kMaxValue) == 1);
  uint number_of_tokens = data_provider_.ConsumeIntegralInRange<uint>(
      /*min=*/0, /*max=*/2);
  if (number_of_tokens == 0) {
    // Do nothing.
  } else if (number_of_tokens == 1) {
    tokens.insert(data_provider_.ConsumeEnum<Token>());
  } else {
    tokens.insert(Token::kUser);
    tokens.insert(Token::kDevice);
  }
  return tokens;
}

// The returned pointer is invalidated on `kcer_data_` update.
FuzzKey* KcerFuzzer::SelectFuzzKey() {
  if (kcer_data_.empty()) {
    return nullptr;
  }

  auto random_iter = kcer_data_.begin();
  size_t offset = GetSizeT(kcer_data_.size() - 1);
  std::advance(random_iter, offset);
  FuzzKey& kcer_key = random_iter->second;
  return &kcer_key;
}

PrivateKeyHandle KcerFuzzer::GeneratePrivateKeyHandle(FuzzKey** out_kcer_key) {
  *out_kcer_key = nullptr;

  // Algorithms (or approaches) to generate a handle.
  enum class Algo {
    kExistingPublicKey,
    kSpkiFromExistingKey,
    kSpkiWithTokenFromExistingKey,
    kSpkiWithInvertedTokenFromExistingKey,
    kRandomSpki,
    kRandomSpkiWithToken,
    kMaxValue = kRandomSpkiWithToken,
  };

  Algo algo = data_provider_.ConsumeEnum<Algo>();
  // Some ways to create a handle only work in specific conditions (e.g. need at
  // least one key to exist). Cycle through them until a working one is found.
  // kRandomSpki always works, so the cycle is not infinite.
  while (true) {
    switch (algo) {
      case Algo::kExistingPublicKey: {
        FuzzKey* kcer_key = SelectFuzzKey();
        if (!kcer_key) {
          break;
        }
        *out_kcer_key = kcer_key;
        return PrivateKeyHandle(kcer_key->public_key);
      }
      case Algo::kSpkiFromExistingKey: {
        FuzzKey* kcer_key = SelectFuzzKey();
        if (!kcer_key) {
          break;
        }
        *out_kcer_key = kcer_key;
        return PrivateKeyHandle(kcer_key->public_key.GetSpki());
      }
      case Algo::kSpkiWithTokenFromExistingKey: {
        FuzzKey* kcer_key = SelectFuzzKey();
        if (!kcer_key) {
          break;
        }
        *out_kcer_key = kcer_key;
        return PrivateKeyHandle(kcer_key->public_key.GetToken(),
                                kcer_key->public_key.GetSpki());
      }
      case Algo::kSpkiWithInvertedTokenFromExistingKey: {
        FuzzKey* kcer_key = SelectFuzzKey();
        if (!kcer_key) {
          break;
        }
        Token token = kcer_key->public_key.GetToken();
        static_assert(static_cast<int>(Token::kMaxValue) == 1);
        token = token == Token::kUser ? Token::kDevice : Token::kUser;
        return PrivateKeyHandle(token, kcer_key->public_key.GetSpki());
      }
      case Algo::kRandomSpki: {
        PrivateKeyHandle result(PublicKeySpki(GetBytes(/*min=*/1)));
        auto iter = kcer_data_.find(result.GetSpkiInternal());
        if (iter != kcer_data_.end()) {
          *out_kcer_key = &(iter->second);
        }
        return result;
      }
      case Algo::kRandomSpkiWithToken: {
        Token token = data_provider_.ConsumeEnum<Token>();
        PrivateKeyHandle result(token, PublicKeySpki(GetBytes(/*min=*/1)));
        auto iter = kcer_data_.find(result.GetSpkiInternal());
        if ((iter != kcer_data_.end()) &&
            (iter->second.public_key.GetToken() == token)) {
          *out_kcer_key = &(iter->second);
        }
        return result;
      }
    }
    algo = NextEnumValue(algo);
  }
}

size_t KcerFuzzer::GetSizeT(size_t max) {
  return data_provider_.ConsumeIntegralInRange<size_t>(0, max);
}

std::vector<uint8_t> KcerFuzzer::GetBytes(size_t min) {
  std::vector<uint8_t> bytes = data_provider_.ConsumeBytes<uint8_t>(
      GetSizeT(/*max=*/data_provider_.remaining_bytes()));
  while (bytes.size() < min) {
    bytes.push_back(0);  // Append zeros to get to the minimum required length.
  }
  return bytes;
}

template <typename T>
T KcerFuzzer::NextEnumValue(T val) {
  using NumericT = std::underlying_type_t<T>;
  NumericT numeric_val = static_cast<NumericT>(val);
  NumericT number_of_values = static_cast<NumericT>(T::kMaxValue) + 1;
  return static_cast<T>((numeric_val + 1) % number_of_values);
}

}  // namespace kcer

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // The method might run multiple times within the same execution, initialize
  // the environment only once using a static variable.
  static kcer::Environment env;

  kcer::KcerFuzzer fuzzer(data, size);
  fuzzer.Run();

  if (testing::Test::HasFailure()) {
    // Simulate a crash to make the fuzzer report the issue.
    abort();
  }
  return 0;
}
