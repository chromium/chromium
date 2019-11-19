// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/availability/availability_prober.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

void WaitForCompletedProbe(AvailabilityProber* prober) {
  while (true) {
    if (prober->LastProbeWasSuccessful().has_value())
      return;
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

class TestDelegate : public AvailabilityProber::Delegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() = default;

  bool ShouldSendNextProbe() override { return should_send_next_probe_; }

  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override {
    got_head_ = head;
    return net_error == net::OK && head &&
           head->headers->response_code() == net::HTTP_OK;
  }

  void set_should_send_next_probe(bool should_send_next_probe) {
    should_send_next_probe_ = should_send_next_probe;
  }

  bool got_head() const { return got_head_; }

 private:
  bool should_send_next_probe_ = true;
  bool got_head_ = false;
};

class AvailabilityProberBrowserTest : public InProcessBrowserTest {
 public:
  AvailabilityProberBrowserTest() = default;
  ~AvailabilityProberBrowserTest() override = default;

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &AvailabilityProberBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII("host-rules", "MAP test.com 127.0.0.1");
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  GURL TestURLWithPath(const std::string& path) const {
    return https_server_->GetURL("test.com", path);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::string path = request.GetURL().path();
    if (path == "/ok") {
      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      return response;
    }

    if (path == "/timeout") {
      std::unique_ptr<net::test_server::HungResponse> response =
          std::make_unique<net::test_server::HungResponse>();
      return response;
    }

    NOTREACHED() << path << " is not handled";
    return nullptr;
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  DISALLOW_COPY_AND_ASSIGN(AvailabilityProberBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AvailabilityProberBrowserTest, OK) {
  GURL url = TestURLWithPath("/ok");
  TestDelegate delegate;
  net::HttpRequestHeaders headers;
  AvailabilityProber::RetryPolicy retry_policy;
  AvailabilityProber::TimeoutPolicy timeout_policy;

  AvailabilityProber prober(
      &delegate, browser()->profile()->GetURLLoaderFactory(),
      browser()->profile()->GetPrefs(),
      AvailabilityProber::ClientName::kLitepages, url,
      AvailabilityProber::HttpMethod::kGet, headers, retry_policy,
      timeout_policy, TRAFFIC_ANNOTATION_FOR_TESTS, 1,
      base::TimeDelta::FromDays(1));
  prober.SendNowIfInactive(false);
  WaitForCompletedProbe(&prober);

  EXPECT_TRUE(prober.LastProbeWasSuccessful().value());
}

IN_PROC_BROWSER_TEST_F(AvailabilityProberBrowserTest, Timeout) {
  GURL url = TestURLWithPath("/timeout");
  TestDelegate delegate;
  net::HttpRequestHeaders headers;

  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 0;

  AvailabilityProber::TimeoutPolicy timeout_policy;
  timeout_policy.base_timeout = base::TimeDelta::FromMilliseconds(1);

  AvailabilityProber prober(
      &delegate, browser()->profile()->GetURLLoaderFactory(),
      browser()->profile()->GetPrefs(),
      AvailabilityProber::ClientName::kLitepages, url,
      AvailabilityProber::HttpMethod::kGet, headers, retry_policy,
      timeout_policy, TRAFFIC_ANNOTATION_FOR_TESTS, 1,
      base::TimeDelta::FromDays(1));
  prober.SendNowIfInactive(false);
  WaitForCompletedProbe(&prober);

  EXPECT_FALSE(prober.LastProbeWasSuccessful().value());
}

IN_PROC_BROWSER_TEST_F(AvailabilityProberBrowserTest, NetworkChange) {
  content::NetworkConnectionChangeSimulator().SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_2G);

  GURL url = TestURLWithPath("/ok");
  TestDelegate delegate;
  net::HttpRequestHeaders headers;
  AvailabilityProber::RetryPolicy retry_policy;
  AvailabilityProber::TimeoutPolicy timeout_policy;

  AvailabilityProber prober(
      &delegate, browser()->profile()->GetURLLoaderFactory(),
      browser()->profile()->GetPrefs(),
      AvailabilityProber::ClientName::kLitepages, url,
      AvailabilityProber::HttpMethod::kGet, headers, retry_policy,
      timeout_policy, TRAFFIC_ANNOTATION_FOR_TESTS, 1,
      base::TimeDelta::FromDays(1));

  content::NetworkConnectionChangeSimulator().SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  WaitForCompletedProbe(&prober);

  EXPECT_TRUE(prober.LastProbeWasSuccessful().value());
}

IN_PROC_BROWSER_TEST_F(AvailabilityProberBrowserTest, BadServer) {
  GURL url("https://invalid.com");
  TestDelegate delegate;
  net::HttpRequestHeaders headers;
  AvailabilityProber::RetryPolicy retry_policy;
  AvailabilityProber::TimeoutPolicy timeout_policy;

  AvailabilityProber prober(
      &delegate, browser()->profile()->GetURLLoaderFactory(),
      browser()->profile()->GetPrefs(),
      AvailabilityProber::ClientName::kLitepages, url,
      AvailabilityProber::HttpMethod::kGet, headers, retry_policy,
      timeout_policy, TRAFFIC_ANNOTATION_FOR_TESTS, 1,
      base::TimeDelta::FromDays(1));
  prober.SendNowIfInactive(false);
  WaitForCompletedProbe(&prober);

  EXPECT_FALSE(delegate.got_head());
  EXPECT_FALSE(prober.LastProbeWasSuccessful().value());
}
