// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_client.h"

namespace enterprise_connectors {

FakeContentAnalysisSdkClient::FakeContentAnalysisSdkClient(
    const content_analysis::sdk::Client::Config& config)
    : config_(config) {}

FakeContentAnalysisSdkClient::~FakeContentAnalysisSdkClient() = default;

const content_analysis::sdk::Client::Config&
FakeContentAnalysisSdkClient::GetConfig() const {
  return config_;
}

int FakeContentAnalysisSdkClient::Send(
    content_analysis::sdk::ContentAnalysisRequest request,
    content_analysis::sdk::ContentAnalysisResponse* response) {
  base::AutoLock al(lock_);

  *response = response_template_;
  // To correlate request and response, just like what the real agent should do.
  response->set_request_token(request.request_token());

  requests_[request.request_token()] = std::move(request);

  return send_status_;
}

int FakeContentAnalysisSdkClient::Acknowledge(
    const content_analysis::sdk::ContentAnalysisAcknowledgement& ack) {
  base::AutoLock al(lock_);
  return ack_status_;
}

int FakeContentAnalysisSdkClient::CancelRequests(
    const content_analysis::sdk::ContentAnalysisCancelRequests& cancel) {
  base::AutoLock al(lock_);
  cancel_ = cancel;
  return cancel_status_;
}

const content_analysis::sdk::AgentInfo&
FakeContentAnalysisSdkClient::GetAgentInfo() const {
  base::AutoLock al(lock_);
  return agent_info_;
}

content_analysis::sdk::ContentAnalysisRequest
FakeContentAnalysisSdkClient::GetRequest(const std::string& request_token) {
  base::AutoLock al(lock_);
  auto it = requests_.find(request_token);
  return it == requests_.end() ? content_analysis::sdk::ContentAnalysisRequest()
                               : it->second;
}

const content_analysis::sdk::ContentAnalysisCancelRequests&
FakeContentAnalysisSdkClient::GetCancelRequests() {
  base::AutoLock al(lock_);
  return cancel_;
}

void FakeContentAnalysisSdkClient::SetAckStatus(int status) {
  base::AutoLock al(lock_);
  ack_status_ = status;
}

void FakeContentAnalysisSdkClient::SetSendStatus(int status) {
  base::AutoLock al(lock_);
  send_status_ = status;
}

void FakeContentAnalysisSdkClient::SetCancelStatus(int status) {
  base::AutoLock al(lock_);
  cancel_status_ = status;
}

void FakeContentAnalysisSdkClient::SetSendResponse(
    const content_analysis::sdk::ContentAnalysisResponse& response) {
  base::AutoLock al(lock_);
  response_template_ = response;
}

void FakeContentAnalysisSdkClient::SetAgentInfo(
    const content_analysis::sdk::AgentInfo& agent_info) {
  base::AutoLock al(lock_);
  agent_info_ = agent_info;
}

}  // namespace enterprise_connectors
