// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/certificate_error_reporter.h"

#include <stdint.h>
#include <string.h>

#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "components/encrypted_messages/message_encrypter.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace {

static const char kHkdfLabel[] = "certificate report";
const char kDummyHttpReportUri[] = "http://example.test";
const char kDummyHttpsReportUri[] = "https://example.test";
const char kDummyReport[] = "a dummy report";
const uint32_t kServerPublicKeyTestVersion = 16;

class ErrorReporterTest : public ::testing::Test {
 public:
  ErrorReporterTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    memset(server_private_key_, 1, sizeof(server_private_key_));
    X25519_public_from_private(server_public_key_, server_private_key_);
  }

  ~ErrorReporterTest() override {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  uint8_t server_public_key_[32];
  uint8_t server_private_key_[32];

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ErrorReporterTest);
};

// Test that ErrorReporter::SendExtendedReportingReport sends
// an encrypted or plaintext extended reporting report as appropriate.
TEST_F(ErrorReporterTest, ExtendedReportingSendReport) {
  GURL latest_report_uri;
  std::string latest_report;
  std::string latest_content_type;

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        latest_report_uri = request.url;
        request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                  &latest_content_type);
        latest_report = network::GetUploadData(request);
      }));

  // Data should not be encrypted when sent to an HTTPS URL.
  const GURL https_url(kDummyHttpsReportUri);
  CertificateErrorReporter https_reporter(test_shared_loader_factory_,
                                          https_url, server_public_key_,
                                          kServerPublicKeyTestVersion);
  https_reporter.SendExtendedReportingReport(
      kDummyReport, base::OnceCallback<void()>(),
      base::OnceCallback<void(int, int)>());
  EXPECT_EQ(latest_report_uri, https_url);
  EXPECT_EQ(latest_report, kDummyReport);

  // Data should be encrypted when sent to an HTTP URL.
  const GURL http_url(kDummyHttpReportUri);
  CertificateErrorReporter http_reporter(test_shared_loader_factory_, http_url,
                                         server_public_key_,
                                         kServerPublicKeyTestVersion);
  http_reporter.SendExtendedReportingReport(
      kDummyReport, base::OnceCallback<void()>(),
      base::OnceCallback<void(int, int)>());

  EXPECT_EQ(latest_report_uri, http_url);
  EXPECT_EQ("application/octet-stream", latest_content_type);

  std::string uploaded_report;
  encrypted_messages::EncryptedMessage encrypted_report;
  ASSERT_TRUE(encrypted_report.ParseFromString(latest_report));
  EXPECT_EQ(kServerPublicKeyTestVersion,
            encrypted_report.server_public_key_version());
  EXPECT_EQ(
      encrypted_messages::EncryptedMessage::AEAD_ECDH_AES_128_CTR_HMAC_SHA256,
      encrypted_report.algorithm());
  // TODO(estark): kHkdfLabel needs to include the null character in the label
  // due to a matching error in the server for the case of certificate
  // reporting, the strlen + 1 can be removed once that error is fixed.
  // https://crbug.com/517746
  ASSERT_TRUE(encrypted_messages::DecryptMessageForTesting(
      server_private_key_,
      base::StringPiece(kHkdfLabel, strlen(kHkdfLabel) + 1), encrypted_report,
      &uploaded_report));

  EXPECT_EQ(kDummyReport, uploaded_report);
}

// Tests that an UMA histogram is recorded if a report fails to send.
TEST_F(ErrorReporterTest, ErroredRequestCallsCallback) {
  base::RunLoop run_loop;

  const GURL report_uri("http://foo.com/bar");

  test_url_loader_factory_.AddResponse(
      report_uri, network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_FAILED));

  CertificateErrorReporter reporter(test_shared_loader_factory_, report_uri);

  reporter.SendExtendedReportingReport(
      kDummyReport, base::BindLambdaForTesting([&]() { FAIL(); }),
      base::BindLambdaForTesting(
          [&](int net_error, int http_response_code) { run_loop.Quit(); }));
  run_loop.Run();
}

// Tests that an UMA histogram is recorded if a report is successfully sent.
TEST_F(ErrorReporterTest, SuccessfulRequestCallsCallback) {
  base::RunLoop run_loop;

  const GURL report_uri("http://foo.com/bar");
  test_url_loader_factory_.AddResponse(report_uri.spec(), "some data");

  CertificateErrorReporter reporter(test_shared_loader_factory_, report_uri);

  reporter.SendExtendedReportingReport(
      kDummyReport, base::BindLambdaForTesting([&]() { run_loop.Quit(); }),
      base::BindLambdaForTesting(
          [&](int net_error, int http_response_code) { FAIL(); }));
  run_loop.Run();
}

}  // namespace
