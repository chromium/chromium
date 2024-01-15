// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_PROVIDER_TEST_CERTIFICATE_PROVIDER_EXTENSION_H_
#define CHROME_BROWSER_CERTIFICATE_PROVIDER_TEST_CERTIFICATE_PROVIDER_EXTENSION_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace crypto {
class RSAPrivateKey;
}

namespace ash {

// This class provides the C++ side of the test certificate provider extension's
// implementation (the JavaScript side is in
// chrome/test/data/extensions/test_certificate_provider).
//
// It subscribes itself for requests from the JavaScript side of the extension,
// and implements the cryptographic operations using the "client_1" test
// certificate and private key (see src/net/data/ssl/certificates). The
// supported signature algorithms are currently hardcoded to PKCS #1 v1.5 with
// SHA-1 and SHA-256.
class TestCertificateProviderExtension final {
 public:
  static extensions::ExtensionId extension_id();
  static base::FilePath GetExtensionSourcePath();
  static base::FilePath GetExtensionPemPath();
  // Returns the certificate provided by the extension.
  static scoped_refptr<net::X509Certificate> GetCertificate();
  static std::string GetCertificateSpki();

  explicit TestCertificateProviderExtension(
      content::BrowserContext* browser_context);

  TestCertificateProviderExtension(const TestCertificateProviderExtension&) =
      delete;
  TestCertificateProviderExtension& operator=(
      const TestCertificateProviderExtension&) = delete;

  ~TestCertificateProviderExtension();

  // Causes the extension to call chrome.certificateProvider.setCertificates,
  // providing the certificates that are currently available.
  void TriggerSetCertificates();

  int certificate_request_count() const { return certificate_request_count_; }

  // Sets the PIN that will be required when doing every signature request.
  // (By default, no PIN is requested.)
  void set_require_pin(const std::string& pin) { required_pin_ = pin; }

  // Sets the number of remaining PIN attempts.
  // Zero number means the lockout state, when no attempts are allowed anymore.
  // A negative number denotes infinite number of attempts, which is the default
  // behavior.
  void set_remaining_pin_attempts(int remaining_pin_attempts) {
    remaining_pin_attempts_ = remaining_pin_attempts;
  }

  // Sets whether the extension should return any certificates in response to a
  // onCertificatesRequested request or a TriggerSetCertificates() call.
  void set_should_provide_certificates(bool should_provide_certificates) {
    should_provide_certificates_ = should_provide_certificates;
  }

  // Sets whether the extension should respond with a failure to the
  // onSignDigestRequested requests.
  void set_should_fail_sign_digest_requests(
      bool should_fail_sign_digest_requests) {
    should_fail_sign_digest_requests_ = should_fail_sign_digest_requests;
  }

 private:
  using ReplyToJsCallback =
      base::OnceCallback<void(const base::Value& response)>;

  void HandleMessage(const std::string& message);

  void HandleCertificatesRequest(ReplyToJsCallback callback);
  void HandleSignatureRequest(const base::Value& sign_request,
                              const base::Value& pin_status,
                              const base::Value& pin,
                              ReplyToJsCallback callback);

  const raw_ptr<content::BrowserContext> browser_context_;
  const scoped_refptr<net::X509Certificate> certificate_;
  std::unique_ptr<crypto::RSAPrivateKey> private_key_;
  int certificate_request_count_ = 0;
  // When non-empty, contains the expected PIN; the implementation will request
  // the PIN on every signature request in this case.
  std::optional<std::string> required_pin_;
  // The number of remaining PIN attempts.
  // When equal to zero, signature requests will be failed immediately; when is
  // negative, infinite number of attempts is allowed.
  int remaining_pin_attempts_ = -1;
  bool should_provide_certificates_ = true;
  bool should_fail_sign_digest_requests_ = false;
  ExtensionTestMessageListener message_listener_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_CERTIFICATE_PROVIDER_TEST_CERTIFICATE_PROVIDER_EXTENSION_H_
