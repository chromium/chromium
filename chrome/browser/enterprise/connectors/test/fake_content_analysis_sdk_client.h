// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_CONTENT_ANALYSIS_SDK_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_CONTENT_ANALYSIS_SDK_CLIENT_H_

#include "base/synchronization/lock.h"
#include "third_party/content_analysis_sdk/src/browser/include/content_analysis/sdk/analysis_client.h"

namespace enterprise_connectors {
// A derivative of content analysis SDK client that creates fake clients
// not dependent on having a real service provider agent running.
class FakeContentAnalysisSdkClient : public content_analysis::sdk::Client {
 public:
  explicit FakeContentAnalysisSdkClient(
      const content_analysis::sdk::Client::Config& config);

  ~FakeContentAnalysisSdkClient() override;

  // content_analysis::sdk::Client:
  const content_analysis::sdk::Client::Config& GetConfig() const override;
  int Send(content_analysis::sdk::ContentAnalysisRequest request,
           content_analysis::sdk::ContentAnalysisResponse* response) override;
  int Acknowledge(const content_analysis::sdk::ContentAnalysisAcknowledgement&
                      ack) override;
  int CancelRequests(const content_analysis::sdk::ContentAnalysisCancelRequests&
                         cancel) override;
  const content_analysis::sdk::AgentInfo& GetAgentInfo() const override;

  // Get the latest request client receives.
  content_analysis::sdk::ContentAnalysisRequest GetRequest(
      const std::string& request_token);

  // Get the latest cancel requests receives.
  const content_analysis::sdk::ContentAnalysisCancelRequests&
  GetCancelRequests();

  // Configure response acknowledgement status.
  void SetAckStatus(int status);

  // Configure analysis request sending status.
  void SetSendStatus(int status);

  // Configure cancel requests status.
  void SetCancelStatus(int status);

  // Configure agent response.
  void SetSendResponse(
      const content_analysis::sdk::ContentAnalysisResponse& response);

  // Configure agent info.
  void SetAgentInfo(const content_analysis::sdk::AgentInfo& agent_info);

 private:
  mutable base::Lock lock_;
  content_analysis::sdk::Client::Config config_;
  content_analysis::sdk::ContentAnalysisResponse response_template_;
  std::map<std::string, content_analysis::sdk::ContentAnalysisRequest>
      requests_;
  content_analysis::sdk::ContentAnalysisCancelRequests cancel_;
  int send_status_ = 0;
  int ack_status_ = 0;
  int cancel_status_ = 0;
  content_analysis::sdk::AgentInfo agent_info_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_CONTENT_ANALYSIS_SDK_CLIENT_H_
