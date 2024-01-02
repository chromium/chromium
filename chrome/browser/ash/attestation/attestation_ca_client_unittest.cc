// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/attestation_ca_client.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace ash {
namespace attestation {

namespace {
class MockNetworkContext : public network::TestNetworkContext {
 public:
  MockNetworkContext() {
    ON_CALL(*this, LookUpProxyForURL(_, _, _))
        .WillByDefault(
            Invoke(this, &MockNetworkContext::LookUpProxyForURLInternal));
  }
  ~MockNetworkContext() override = default;
  MOCK_METHOD(void,
              LookUpProxyForURL,
              (const GURL& url,
               const net::NetworkAnonymizationKey& network_anonymization_key,
               mojo::PendingRemote<::network::mojom::ProxyLookupClient>
                   proxy_lookup_client),
              (override));

  void SetProxyPresence(const GURL& url, bool is_present) {
    proxy_presence_table_[url] = is_present;
  }

 private:
  void LookUpProxyForURLInternal(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<::network::mojom::ProxyLookupClient>
          proxy_lookup_client) {
    mojo::Remote<::network::mojom::ProxyLookupClient> client(
        std::move(proxy_lookup_client));
    if (proxy_presence_table_.count(url) == 0) {
      client->OnProxyLookupComplete(net::ERR_FAILED,
                                    /*proxy_info=*/std::nullopt);
      return;
    }
    net::ProxyInfo proxy_info;
    if (proxy_presence_table_[url]) {
      proxy_info.UseNamedProxy("named.proxy.com");
    } else {
      proxy_info.UseDirect();
    }
    client->OnProxyLookupComplete(net::OK, std::move(proxy_info));
  }

  std::map<GURL, bool> proxy_presence_table_;
};
}  // namespace

class AttestationCAClientTest : public ::testing::Test {
 public:
  AttestationCAClientTest()
      : test_shared_url_loader_factory_(
            test_url_loader_factory_.GetSafeWeakWrapper()),
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
      "enroll", base::BindOnce(&AttestationCAClientTest::DataCallback,
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
      "certificate", base::BindOnce(&AttestationCAClientTest::DataCallback,
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
      "certificate", base::BindOnce(&AttestationCAClientTest::DataCallback,
                                    base::Unretained(this)));
  SendResponse(net::ERR_FAILED, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_FALSE(result_);
  EXPECT_EQ("", data_);
}

TEST_F(AttestationCAClientTest, CertificateRequestHttpError) {
  AttestationCAClient client;
  client.SendCertificateRequest(
      "certificate", base::BindOnce(&AttestationCAClientTest::DataCallback,
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
      base::BindOnce(&AttestationCAClientTest::DeleteClientDataCallback,
                     base::Unretained(this), client));
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
  client.SendEnrollRequest(
      "enroll", base::BindOnce(&AttestationCAClientTest::DataCallback,
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
      "certificate", base::BindOnce(&AttestationCAClientTest::DataCallback,
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
  client.SendEnrollRequest(
      "enroll", base::BindOnce(&AttestationCAClientTest::DataCallback,
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
      "certificate", base::BindOnce(&AttestationCAClientTest::DataCallback,
                                    base::Unretained(this)));
  CheckURLAndSendResponse(GURL("https://asbestos-qa.corp.google.com/sign"),
                          net::OK, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("certificate_response", data_);
}

TEST_F(AttestationCAClientAttestationServerTest, ProxyPresentForDefaultCA) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kAttestationServer, "default");
  StrictMock<MockNetworkContext> network_context;
  network_context.SetProxyPresence(GURL("https://chromeos-ca.gstatic.com"),
                                   true);
  EXPECT_CALL(network_context,
              LookUpProxyForURL(GURL("https://chromeos-ca.gstatic.com"), _, _));
  AttestationCAClient client;
  client.set_network_context_for_testing(&network_context);
  bool is_proxy_present = false;
  client.CheckIfAnyProxyPresent(base::BindOnce(
      [](bool* result, bool is_present) { *result = is_present; },
      &is_proxy_present));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_proxy_present);
}

TEST_F(AttestationCAClientAttestationServerTest, ProxyPresentForTestCA) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kAttestationServer, "test");
  StrictMock<MockNetworkContext> network_context;
  network_context.SetProxyPresence(GURL("https://asbestos-qa.corp.google.com"),
                                   true);
  EXPECT_CALL(
      network_context,
      LookUpProxyForURL(GURL("https://asbestos-qa.corp.google.com"), _, _));
  AttestationCAClient client;
  client.set_network_context_for_testing(&network_context);
  bool is_proxy_present = false;
  client.CheckIfAnyProxyPresent(base::BindOnce(
      [](bool* result, bool is_present) { *result = is_present; },
      &is_proxy_present));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_proxy_present);
}

TEST_F(AttestationCAClientAttestationServerTest, ProxyNotPresentForDefaultCA) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kAttestationServer, "default");
  StrictMock<MockNetworkContext> network_context;
  network_context.SetProxyPresence(GURL("https://chromeos-ca.gstatic.com"),
                                   false);
  EXPECT_CALL(network_context,
              LookUpProxyForURL(GURL("https://chromeos-ca.gstatic.com"), _, _));
  AttestationCAClient client;
  client.set_network_context_for_testing(&network_context);
  bool is_proxy_present = true;
  client.CheckIfAnyProxyPresent(base::BindOnce(
      [](bool* result, bool is_present) { *result = is_present; },
      &is_proxy_present));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(is_proxy_present);
}

TEST_F(AttestationCAClientAttestationServerTest, ProxyNotPresentForTestCA) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kAttestationServer, "test");
  StrictMock<MockNetworkContext> network_context;
  network_context.SetProxyPresence(GURL("https://asbestos-qa.corp.google.com"),
                                   false);
  EXPECT_CALL(
      network_context,
      LookUpProxyForURL(GURL("https://asbestos-qa.corp.google.com"), _, _));
  AttestationCAClient client;
  client.set_network_context_for_testing(&network_context);
  bool is_proxy_present = true;
  client.CheckIfAnyProxyPresent(base::BindOnce(
      [](bool* result, bool is_present) { *result = is_present; },
      &is_proxy_present));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(is_proxy_present);
}

TEST_F(AttestationCAClientAttestationServerTest,
       ProxyAssumedToBePresentUponError) {
  StrictMock<MockNetworkContext> network_context;
  EXPECT_CALL(network_context, LookUpProxyForURL(_, _, _));
  AttestationCAClient client;
  client.set_network_context_for_testing(&network_context);
  bool is_proxy_present = false;
  client.CheckIfAnyProxyPresent(base::BindOnce(
      [](bool* result, bool is_present) { *result = is_present; },
      &is_proxy_present));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_proxy_present);
}

TEST_F(AttestationCAClientAttestationServerTest, CheckProxyMultipleCalls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kAttestationServer, "default");
  StrictMock<MockNetworkContext> network_context;
  network_context.SetProxyPresence(GURL("https://chromeos-ca.gstatic.com"),
                                   true);
  EXPECT_CALL(network_context,
              LookUpProxyForURL(GURL("https://chromeos-ca.gstatic.com"), _, _))
      .Times(2);
  AttestationCAClient client;
  client.set_network_context_for_testing(&network_context);
  bool is_proxy_present1 = false;
  bool is_proxy_present2 = false;
  client.CheckIfAnyProxyPresent(base::BindOnce(
      [](bool* result, bool is_present) { *result = is_present; },
      &is_proxy_present1));
  client.CheckIfAnyProxyPresent(base::BindOnce(
      [](bool* result, bool is_present) { *result = is_present; },
      &is_proxy_present2));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_proxy_present1);
  EXPECT_TRUE(is_proxy_present2);
}

}  // namespace attestation
}  // namespace ash
