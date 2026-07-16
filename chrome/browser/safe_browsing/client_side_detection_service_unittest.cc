// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_service.h"

#include "base/test/run_until.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_service_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSideDetectionServiceTest : public testing::Test {
 protected:
  void DidSendClientReportPhishingRequest(
      ClientSideDetectionServiceBase* service,
      std::unique_ptr<ClientPhishingRequest> request,
      const std::string& access_token) {
    service->DidSendClientReportPhishingRequest(std::move(request),
                                                access_token);
  }

  void DidReceiveClientPhishingResponse(
      ClientSideDetectionServiceBase* service,
      const ClientPhishingResponse& response) {
    service->DidReceiveClientPhishingResponse(response);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ClientSideDetectionServiceTest, DiagnosticLogging) {
  auto service = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(&profile_),
      /*opt_guide=*/nullptr);
  ClientSideDetectionServiceBase* base_service = service.get();

  // Enable listener for testing, otherwise the logs are dropped immediately.
  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();

  // Test DidSendClientReportPhishingRequest
  {
    auto request = std::make_unique<ClientPhishingRequest>();
    request->set_url("http://phishing.com/");
    request->set_client_score(0.8f);

    DidSendClientReportPhishingRequest(base_service, std::move(request),
                                       "token");
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return WebUIContentInfoSingleton::GetInstance()
                 ->client_phishing_requests_sent()
                 .size() == 1U;
    }));

    const auto& requests = WebUIContentInfoSingleton::GetInstance()
                               ->client_phishing_requests_sent();
    ASSERT_EQ(requests.size(), 1U);
    EXPECT_EQ(requests[0].request.url(), "http://phishing.com/");
    EXPECT_FLOAT_EQ(requests[0].request.client_score(), 0.8f);
    EXPECT_EQ(requests[0].token, "token");
  }

  // Test DidReceiveClientPhishingResponse
  {
    ClientPhishingResponse response;
    response.set_phishy(true);

    DidReceiveClientPhishingResponse(base_service, response);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return WebUIContentInfoSingleton::GetInstance()
                 ->client_phishing_responses_received()
                 .size() == 1U;
    }));

    const auto& responses = WebUIContentInfoSingleton::GetInstance()
                                ->client_phishing_responses_received();
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_TRUE(responses[0]->phishy());
  }

  WebUIContentInfoSingleton::GetInstance()->ClearListenerForTesting();
}

}  // namespace safe_browsing
