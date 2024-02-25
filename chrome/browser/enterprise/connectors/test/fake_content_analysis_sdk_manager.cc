// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_manager.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_client.h"

#include <cstddef>

namespace enterprise_connectors {

FakeContentAnalysisSdkManager::FakeContentAnalysisSdkManager() {
  ContentAnalysisSdkManager::SetManagerForTesting(this);
}

FakeContentAnalysisSdkManager::~FakeContentAnalysisSdkManager() {
  ContentAnalysisSdkManager::SetManagerForTesting(nullptr);
}

void FakeContentAnalysisSdkManager::SetClientSendStatus(int status) {
  send_status_ = status;
}

void FakeContentAnalysisSdkManager::SetClientSendResponse(
    const content_analysis::sdk::ContentAnalysisResponse& response) {
  response_ = response;
}

void FakeContentAnalysisSdkManager::SetClientAckStatus(int status) {
  ack_status_ = status;
}

void FakeContentAnalysisSdkManager::SetClientCancelStatus(int status) {
  cancel_status_ = status;
}

std::unique_ptr<content_analysis::sdk::Client>
FakeContentAnalysisSdkManager::CreateClient(
    const content_analysis::sdk::Client::Config& config) {
  if (!can_create_client_) {
    return nullptr;
  }

  auto client = std::make_unique<FakeContentAnalysisSdkClient>(config);
  client->SetSendStatus(send_status_);
  client->SetSendResponse(response_);
  client->SetAckStatus(ack_status_);
  client->SetCancelStatus(cancel_status_);
  fake_clients_.insert(std::make_pair(std::move(config), client.get()));

  return client;
}

void FakeContentAnalysisSdkManager::ResetClient(
    const content_analysis::sdk::Client::Config& config) {
  ContentAnalysisSdkManager::ResetClient(config);
  fake_clients_.erase(config);
}

void FakeContentAnalysisSdkManager::ResetAllClients() {
  ContentAnalysisSdkManager::ResetAllClients();
  fake_clients_.clear();
}

void FakeContentAnalysisSdkManager::SetCreateClientAbility(
    bool can_create_client) {
  can_create_client_ = can_create_client;
}

bool FakeContentAnalysisSdkManager::NoConnectionEstablished() {
  return fake_clients_.empty();
}

FakeContentAnalysisSdkClient* FakeContentAnalysisSdkManager::GetFakeClient(
    const content_analysis::sdk::Client::Config& config) {
  auto it = fake_clients_.find(config);
  if (it != fake_clients_.end())
    return it->second;
  return nullptr;
}

}  // namespace enterprise_connectors
