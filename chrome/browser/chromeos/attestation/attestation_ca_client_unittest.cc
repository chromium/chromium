// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/bind_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace attestation {

class AttestationCAClientTest : public ::testing::Test {
 public:
  AttestationCAClientTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        num_invocations_(0),
        result_(false) {}

  ~AttestationCAClientTest() override {}

  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_url_loader_factory_);

    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_resource_request_ = request;
        }));
  }

  void DataCallback(bool result, const std::string& data) {
    ++num_invocations_;
    result_ = result;
    data_ = data;
  }

  void DeleteClientDataCallback(AttestationCAClient* client,
                                bool result,
                                const std::string& data) {
    delete client;
    DataCallback(result, data);
  }

 protected:
  void CheckURLAndSendResponse(GURL expected_url,
                               net::Error error,
                               int response_code) {
    CHECK(test_url_loader_factory_.NumPending() == 1);
    EXPECT_EQ(expected_url, last_resource_request_.url);
    std::string response =
        network::GetUploadData(last_resource_request_) + "_response";
    test_url_loader_factory_.AddResponse(last_resource_request_.url.spec(),
                                         response);
    base::RunLoop().RunUntilIdle();
  }

  void SendResponse(net::Error error, net::HttpStatusCode response_code) {
    CHECK(test_url_loader_factory_.NumPending() == 1);
    auto url_response_head = network::CreateURLResponseHead(response_code);
    network::URLLoaderCompletionStatus completion_status(error);
    std::string response =
        network::GetUploadData(last_resource_request_) + "_response";

    test_url_loader_factory_.AddResponse(last_resource_request_.url,
                                         std::move(url_response_head), response,
                                         completion_status);
    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  network::ResourceRequest last_resource_request_;

  // For use with DataCallback.
  int num_invocations_;
  bool result_;
  std::string data_;
};

TEST_F(AttestationCAClientTest, EnrollRequest) {
  AttestationCAClient client;
  client.SendEnrollRequest(
      "enroll",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  CheckURLAndSendResponse(GURL("https://chromeos-ca.gstatic.com/enroll"),
                          net::OK, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("enroll_response", data_);
}

TEST_F(AttestationCAClientTest, CertificateRequest) {
  AttestationCAClient client;
  client.SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  CheckURLAndSendResponse(GURL("https://chromeos-ca.gstatic.com/sign"), net::OK,
                          net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("certificate_response", data_);
}

TEST_F(AttestationCAClientTest, CertificateRequestNetworkFailure) {
  AttestationCAClient client;
  client.SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::ERR_FAILED, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_FALSE(result_);
  EXPECT_EQ("", data_);
}

TEST_F(AttestationCAClientTest, CertificateRequestHttpError) {
  AttestationCAClient client;
  client.SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::OK, net::HTTP_NOT_FOUND);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_FALSE(result_);
  EXPECT_EQ("", data_);
}

TEST_F(AttestationCAClientTest, DeleteOnCallback) {
  AttestationCAClient* client = new AttestationCAClient();
  client->SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DeleteClientDataCallback,
                 base::Unretained(this),
                 client));
  SendResponse(net::OK, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("certificate_response", data_);
}

class AttestationCAClientAttestationServerTest
    : public AttestationCAClientTest {};

TEST_F(AttestationCAClientAttestationServerTest, DefaultEnrollRequest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kAttestationServer, "default");
  AttestationCAClient client;
  client.SendEnrollRequest("enroll",
                           base::Bind(&AttestationCAClientTest::DataCallback,
                                      base::Unretained(this)));
  CheckURLAndSendResponse(GURL("https://chromeos-ca.gstatic.com/enroll"),
                          net::OK, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("enroll_response", data_);
}

TEST_F(AttestationCAClientAttestationServerTest, DefaultCertificateRequest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kAttestationServer, "default");
  AttestationCAClient client;
  client.SendCertificateRequest(
      "certificate", base::Bind(&AttestationCAClientTest::DataCallback,
                                base::Unretained(this)));
  CheckURLAndSendResponse(GURL("https://chromeos-ca.gstatic.com/sign"), net::OK,
                          net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("certificate_response", data_);
}

TEST_F(AttestationCAClientAttestationServerTest, TestEnrollRequest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kAttestationServer, "test");
  AttestationCAClient client;
  client.SendEnrollRequest("enroll",
                           base::Bind(&AttestationCAClientTest::DataCallback,
                                      base::Unretained(this)));
  CheckURLAndSendResponse(GURL("https://asbestos-qa.corp.google.com/enroll"),
                          net::OK, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("enroll_response", data_);
}

TEST_F(AttestationCAClientAttestationServerTest, TestCertificateRequest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kAttestationServer, "test");
  AttestationCAClient client;
  client.SendCertificateRequest(
      "certificate", base::Bind(&AttestationCAClientTest::DataCallback,
                                base::Unretained(this)));
  CheckURLAndSendResponse(GURL("https://asbestos-qa.corp.google.com/sign"),
                          net::OK, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("certificate_response", data_);
}

}  // namespace attestation
}  // namespace chromeos
