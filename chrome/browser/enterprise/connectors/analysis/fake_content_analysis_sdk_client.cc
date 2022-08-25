// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_client.h"

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
    const content_analysis::sdk::ContentAnalysisRequest& request,
    content_analysis::sdk::ContentAnalysisResponse* response) {
  *response = response_;
  return send_status_;
}

int FakeContentAnalysisSdkClient::Acknowledge(
    const content_analysis::sdk::ContentAnalysisAcknowledgement& ack) {
  return ack_status_;
}

void FakeContentAnalysisSdkClient::SetSendStatus(int status) {
  send_status_ = status;
}

void FakeContentAnalysisSdkClient::SetSendResponse(
    const content_analysis::sdk::ContentAnalysisResponse& response) {
  response_ = response;
}

void FakeContentAnalysisSdkClient::SetAckStatus(int status) {
  ack_status_ = status;
}

}  // namespace enterprise_connectors
