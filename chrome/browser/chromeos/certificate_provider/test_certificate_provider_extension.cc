// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/common/extensions/api/certificate_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/notification_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/key_util.h"
#include "net/test/test_data_directory.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace {

// List of algorithms that the extension claims to support for the returned
// certificates.
constexpr extensions::api::certificate_provider::Hash kSupportedHashes[] = {
    extensions::api::certificate_provider::Hash::HASH_SHA256,
    extensions::api::certificate_provider::Hash::HASH_SHA1};

base::Value ConvertBytesToValue(base::span<const uint8_t> bytes) {
  base::Value value(base::Value::Type::LIST);
  for (auto byte : bytes)
    value.Append(byte);
  return value;
}

std::vector<uint8_t> ExtractBytesFromValue(const base::Value& value) {
  std::vector<uint8_t> bytes;
  for (const base::Value& item_value : value.GetList())
    bytes.push_back(base::checked_cast<uint8_t>(item_value.GetInt()));
  return bytes;
}

base::span<const uint8_t> GetCertDer(const net::X509Certificate& certificate) {
  return base::as_bytes(base::make_span(
      net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer())));
}

base::Value MakeCertInfoValue(const net::X509Certificate& certificate) {
  base::Value cert_info_value(base::Value::Type::DICTIONARY);
  cert_info_value.SetKey("certificate",
                         ConvertBytesToValue(GetCertDer(certificate)));
  base::Value supported_hashes_value(base::Value::Type::LIST);
  for (auto supported_hash : kSupportedHashes) {
    supported_hashes_value.Append(base::Value(
        extensions::api::certificate_provider::ToString(supported_hash)));
  }
  cert_info_value.SetKey("supportedHashes", std::move(supported_hashes_value));
  return cert_info_value;
}

std::string ConvertValueToJson(const base::Value& value) {
  std::string json;
  CHECK(base::JSONWriter::Write(value, &json));
  return json;
}

base::Value ParseJsonToValue(const std::string& json) {
  base::Optional<base::Value> value = base::JSONReader::Read(json);
  CHECK(value);
  return std::move(*value);
}

bool RsaSignPrehashed(const EVP_PKEY& key,
                      int openssl_digest_type,
                      const std::vector<uint8_t>& digest,
                      std::vector<uint8_t>* signature) {
  RSA* const rsa_key = EVP_PKEY_get0_RSA(&key);
  if (!rsa_key)
    return false;
  unsigned signature_size = 0;
  signature->resize(RSA_size(rsa_key));
  if (!RSA_sign(openssl_digest_type, digest.data(), digest.size(),
                signature->data(), &signature_size, rsa_key)) {
    signature->clear();
    return false;
  }
  signature->resize(signature_size);
  return true;
}

}  // namespace

TestCertificateProviderExtension::TestCertificateProviderExtension(
    content::BrowserContext* browser_context,
    const std::string& extension_id)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      certificate_(net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                           "client_1.pem")),
      private_key_(net::key_util::LoadEVP_PKEYFromPEM(
          net::GetTestCertsDirectory().Append(
              FILE_PATH_LITERAL("client_1.key")))) {
  DCHECK(browser_context_);
  DCHECK(!extension_id_.empty());
  CHECK(certificate_);
  CHECK(private_key_);
  notification_registrar_.Add(this,
                              extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                              content::NotificationService::AllSources());
}

TestCertificateProviderExtension::~TestCertificateProviderExtension() = default;

void TestCertificateProviderExtension::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE, type);

  extensions::TestSendMessageFunction* function =
      content::Source<extensions::TestSendMessageFunction>(source).ptr();
  if (!function->extension() || function->extension_id() != extension_id_ ||
      function->browser_context() != browser_context_) {
    // Ignore messages targeted to other extensions.
    return;
  }

  const std::string& message =
      content::Details<std::pair<std::string, bool*>>(details)->first;
  base::Value message_value = ParseJsonToValue(message);
  CHECK(message_value.is_list());
  CHECK(message_value.GetList().size());
  CHECK(message_value.GetList()[0].is_string());
  const std::string& request_type = message_value.GetList()[0].GetString();
  base::Value response;
  if (request_type == "onCertificatesRequested") {
    CHECK_EQ(message_value.GetList().size(), 1U);
    response = HandleCertificatesRequest();
  } else if (request_type == "onSignDigestRequested") {
    CHECK_EQ(message_value.GetList().size(), 2U);
    response =
        HandleSignDigestRequest(/*sign_request=*/message_value.GetList()[1]);
  } else {
    LOG(FATAL) << "Unexpected JS message type: " << request_type;
  }
  function->Reply(ConvertValueToJson(response));
}

base::Value TestCertificateProviderExtension::HandleCertificatesRequest() {
  base::Value cert_info_values(base::Value::Type::LIST);
  if (!should_fail_certificate_requests_)
    cert_info_values.Append(MakeCertInfoValue(*certificate_));
  return cert_info_values;
}

base::Value TestCertificateProviderExtension::HandleSignDigestRequest(
    const base::Value& sign_request) {
  CHECK_EQ(*sign_request.FindKey("certificate"),
           ConvertBytesToValue(GetCertDer(*certificate_)));

  const std::vector<uint8_t> digest =
      ExtractBytesFromValue(*sign_request.FindKey("digest"));

  const extensions::api::certificate_provider::Hash hash =
      extensions::api::certificate_provider::ParseHash(
          sign_request.FindKey("hash")->GetString());
  int openssl_digest_type = 0;
  if (hash == extensions::api::certificate_provider::Hash::HASH_SHA256)
    openssl_digest_type = NID_sha256;
  else if (hash == extensions::api::certificate_provider::Hash::HASH_SHA1)
    openssl_digest_type = NID_sha1;
  else
    LOG(FATAL) << "Unexpected signature request hash: " << hash;

  if (should_fail_sign_digest_requests_)
    return base::Value();
  std::vector<uint8_t> signature;
  CHECK(
      RsaSignPrehashed(*private_key_, openssl_digest_type, digest, &signature));
  return ConvertBytesToValue(signature);
}
