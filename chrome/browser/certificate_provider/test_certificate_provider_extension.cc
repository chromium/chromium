// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_provider/test_certificate_provider_extension.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/certificate_provider.h"
#include "content/public/browser/browser_context.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/test.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace ash {

namespace {

constexpr char kExtensionId[] = "ecmhnokcdiianioonpgakiooenfnonid";
// Paths relative to |chrome::DIR_TEST_DATA|:
constexpr base::FilePath::CharType kExtensionPath[] =
    FILE_PATH_LITERAL("extensions/test_certificate_provider/extension/");
constexpr base::FilePath::CharType kExtensionPemPath[] =
    FILE_PATH_LITERAL("extensions/test_certificate_provider/extension.pem");

// List of algorithms that the extension claims to support for the returned
// certificates.
constexpr extensions::api::certificate_provider::Algorithm
    kSupportedAlgorithms[] = {
        extensions::api::certificate_provider::Algorithm::
            kRsassaPkcs1V1_5Sha256,
        extensions::api::certificate_provider::Algorithm::kRsassaPkcs1V1_5Sha1};

base::Value ConvertBytesToValue(base::span<const uint8_t> bytes) {
  base::Value::List value;
  for (auto byte : bytes)
    value.Append(byte);
  return base::Value(std::move(value));
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

base::Value MakeClientCertificateInfoValue(
    const net::X509Certificate& certificate) {
  base::Value::Dict cert_info_value;
  base::Value::List certificate_chain;
  certificate_chain.Append(ConvertBytesToValue(GetCertDer(certificate)));
  cert_info_value.Set("certificateChain", std::move(certificate_chain));
  base::Value::List supported_algorithms_value;
  for (auto supported_algorithm : kSupportedAlgorithms) {
    supported_algorithms_value.Append(
        extensions::api::certificate_provider::ToString(supported_algorithm));
  }
  cert_info_value.Set("supportedAlgorithms",
                      std::move(supported_algorithms_value));
  return base::Value(std::move(cert_info_value));
}

std::string ConvertValueToJson(const base::Value& value) {
  std::string json;
  CHECK(base::JSONWriter::Write(value, &json));
  return json;
}

base::Value ParseJsonToValue(const std::string& json) {
  std::optional<base::Value> value = base::JSONReader::Read(json);
  CHECK(value);
  return std::move(*value);
}

bool RsaSignRawData(crypto::RSAPrivateKey* key,
                    uint16_t openssl_signature_algorithm,
                    const std::vector<uint8_t>& input,
                    std::vector<uint8_t>* signature) {
  const EVP_MD* const digest_algorithm =
      SSL_get_signature_algorithm_digest(openssl_signature_algorithm);
  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pkey_ctx = nullptr;
  if (!EVP_DigestSignInit(ctx.get(), &pkey_ctx, digest_algorithm,
                          /*ENGINE* e=*/nullptr, key->key()))
    return false;
  if (SSL_is_signature_algorithm_rsa_pss(openssl_signature_algorithm)) {
    // For RSA-PSS, configure the special padding and set the salt length to be
    // equal to the hash size.
    if (!EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) ||
        !EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, /*salt_len=*/-1)) {
      return false;
    }
  }
  size_t sig_len = 0;
  // Determine the signature length for the buffer.
  if (!EVP_DigestSign(ctx.get(), /*out_sig=*/nullptr, &sig_len, input.data(),
                      input.size()))
    return false;
  signature->resize(sig_len);
  return EVP_DigestSign(ctx.get(), signature->data(), &sig_len, input.data(),
                        input.size()) != 0;
}

void SendReplyToJs(ExtensionTestMessageListener* message_listener,
                   const base::Value& response) {
  message_listener->Reply(ConvertValueToJson(response));
  message_listener->Reset();
}

std::unique_ptr<crypto::RSAPrivateKey> LoadPrivateKeyFromFile(
    const base::FilePath& path) {
  std::string key_pk8;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::ReadFileToString(path, &key_pk8));
  }
  return crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(
      base::as_bytes(base::make_span(key_pk8)));
}

}  // namespace

// static
extensions::ExtensionId TestCertificateProviderExtension::extension_id() {
  return kExtensionId;
}

// static
base::FilePath TestCertificateProviderExtension::GetExtensionSourcePath() {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .Append(kExtensionPath);
}

// static
base::FilePath TestCertificateProviderExtension::GetExtensionPemPath() {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .Append(kExtensionPemPath);
}

// static
scoped_refptr<net::X509Certificate>
TestCertificateProviderExtension::GetCertificate() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
}

// static
std::string TestCertificateProviderExtension::GetCertificateSpki() {
  const scoped_refptr<net::X509Certificate> certificate = GetCertificate();
  std::string_view spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki_bytes)) {
    return {};
  }
  return std::string(spki_bytes);
}

TestCertificateProviderExtension::TestCertificateProviderExtension(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      certificate_(GetCertificate()),
      private_key_(LoadPrivateKeyFromFile(net::GetTestCertsDirectory().Append(
          FILE_PATH_LITERAL("client_1.pk8")))),
      message_listener_(ReplyBehavior::kWillReply) {
  DCHECK(browser_context_);
  CHECK(certificate_);
  CHECK(private_key_);
  // Ignore messages targeted to other extensions or browser contexts.
  message_listener_.set_extension_id(kExtensionId);
  message_listener_.set_browser_context(browser_context);
  message_listener_.SetOnRepeatedlySatisfied(
      base::BindRepeating(&TestCertificateProviderExtension::HandleMessage,
                          base::Unretained(this)));
}

TestCertificateProviderExtension::~TestCertificateProviderExtension() = default;

void TestCertificateProviderExtension::TriggerSetCertificates() {
  base::Value::Dict message_data;
  message_data.Set("name", "setCertificates");
  base::Value::List cert_info_values;
  if (should_provide_certificates_)
    cert_info_values.Append(MakeClientCertificateInfoValue(*certificate_));
  message_data.Set("certificateInfoList", std::move(cert_info_values));

  base::Value::List message;
  message.Append(std::move(message_data));
  auto event = std::make_unique<extensions::Event>(
      extensions::events::FOR_TEST,
      extensions::api::test::OnMessage::kEventName, std::move(message),
      browser_context_);
  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id(), std::move(event));
}

void TestCertificateProviderExtension::HandleMessage(
    const std::string& message) {
  // Handle the request and reply to it (possibly, asynchronously).
  base::Value message_value = ParseJsonToValue(message);
  CHECK(message_value.is_list());
  base::Value::List& message_list = message_value.GetList();
  CHECK(message_list.size());
  CHECK(message_list[0].is_string());
  const std::string& request_type = message_list[0].GetString();
  ReplyToJsCallback send_reply_to_js_callback =
      base::BindOnce(&SendReplyToJs, &message_listener_);
  if (request_type == "getCertificates") {
    CHECK_EQ(message_list.size(), 1U);
    HandleCertificatesRequest(std::move(send_reply_to_js_callback));
  } else if (request_type == "onSignatureRequested") {
    CHECK_EQ(message_list.size(), 4U);
    HandleSignatureRequest(
        /*sign_request=*/message_list[1],
        /*pin_status=*/message_list[2],
        /*pin=*/message_list[3], std::move(send_reply_to_js_callback));
  } else {
    LOG(FATAL) << "Unexpected JS message type: " << request_type;
  }
}

void TestCertificateProviderExtension::HandleCertificatesRequest(
    ReplyToJsCallback callback) {
  ++certificate_request_count_;
  base::Value::List cert_info_values;
  if (should_provide_certificates_)
    cert_info_values.Append(MakeClientCertificateInfoValue(*certificate_));
  std::move(callback).Run(base::Value(std::move(cert_info_values)));
}

void TestCertificateProviderExtension::HandleSignatureRequest(
    const base::Value& sign_request,
    const base::Value& pin_status,
    const base::Value& pin,
    ReplyToJsCallback callback) {
  CHECK_EQ(*sign_request.GetDict().Find("certificate"),
           ConvertBytesToValue(GetCertDer(*certificate_)));
  const std::string pin_status_string = pin_status.GetString();
  const std::string pin_string = pin.GetString();

  const int sign_request_id =
      sign_request.GetDict().FindInt("signRequestId").value();
  const std::vector<uint8_t> input =
      ExtractBytesFromValue(*sign_request.GetDict().Find("input"));

  const extensions::api::certificate_provider::Algorithm algorithm =
      extensions::api::certificate_provider::ParseAlgorithm(
          *sign_request.GetDict().FindString("algorithm"));
  int openssl_signature_algorithm = 0;
  if (algorithm == extensions::api::certificate_provider::Algorithm::
                       kRsassaPkcs1V1_5Sha256) {
    openssl_signature_algorithm = SSL_SIGN_RSA_PKCS1_SHA256;
  } else if (algorithm == extensions::api::certificate_provider::Algorithm::
                              kRsassaPkcs1V1_5Sha1) {
    openssl_signature_algorithm = SSL_SIGN_RSA_PKCS1_SHA1;
  } else {
    LOG(FATAL) << "Unexpected signature request algorithm: "
               << extensions::api::certificate_provider::ToString(algorithm);
  }

  if (should_fail_sign_digest_requests_) {
    // Simulate a failure.
    std::move(callback).Run(/*response=*/base::Value());
    return;
  }

  base::Value::Dict response;
  if (required_pin_.has_value()) {
    if (pin_status_string == "not_requested") {
      // The PIN is required but not specified yet, so request it via the JS
      // side before generating the signature.
      base::Value::Dict pin_request_parameters;
      pin_request_parameters.Set("signRequestId", sign_request_id);
      if (remaining_pin_attempts_ == 0) {
        pin_request_parameters.Set("errorType", "MAX_ATTEMPTS_EXCEEDED");
      }
      response.Set("requestPin", std::move(pin_request_parameters));
      std::move(callback).Run(base::Value(std::move(response)));
      return;
    }
    if (remaining_pin_attempts_ == 0) {
      // The error about the lockout is already displayed, so fail immediately.
      std::move(callback).Run(/*response=*/base::Value());
      return;
    }
    if (pin_status_string == "canceled" ||
        base::StartsWith(pin_status_string,
                         "failed:", base::CompareCase::SENSITIVE)) {
      // The PIN request failed.
      LOG(WARNING) << "PIN request failed: " << pin_status_string;
      // Respond with a failure.
      std::move(callback).Run(/*response=*/base::Value());
      return;
    }
    DCHECK_EQ(pin_status_string, "ok");
    if (pin_string != *required_pin_) {
      // The entered PIN is wrong, so decrement the remaining attempt count, and
      // update the PIN dialog with displaying an error.
      if (remaining_pin_attempts_ > 0)
        --remaining_pin_attempts_;
      base::Value::Dict pin_request_parameters;
      pin_request_parameters.Set("signRequestId", sign_request_id);
      pin_request_parameters.Set("errorType", remaining_pin_attempts_ == 0
                                                  ? "MAX_ATTEMPTS_EXCEEDED"
                                                  : "INVALID_PIN");
      if (remaining_pin_attempts_ > 0) {
        pin_request_parameters.Set("attemptsLeft", remaining_pin_attempts_);
      }
      response.Set("requestPin", std::move(pin_request_parameters));
      std::move(callback).Run(base::Value(std::move(response)));
      return;
    }
    // The entered PIN is correct. Stop the PIN request and proceed to
    // generating the signature.
    base::Value::Dict stop_pin_request_parameters;
    stop_pin_request_parameters.Set("signRequestId", sign_request_id);
    response.Set("stopPinRequest", std::move(stop_pin_request_parameters));
  }
  // Generate and return a valid signature.
  std::vector<uint8_t> signature;
  CHECK(RsaSignRawData(private_key_.get(), openssl_signature_algorithm, input,
                       &signature));
  response.Set("signature", ConvertBytesToValue(signature));
  std::move(callback).Run(base::Value(std::move(response)));
}

}  // namespace ash
