// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_CONTENT_ANALYSIS_SDK_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_CONTENT_ANALYSIS_SDK_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_client.h"
#include "third_party/content_analysis_sdk/src/browser/include/content_analysis/sdk/analysis_client.h"

namespace enterprise_connectors {
// A derivative of ContentAnalysisSdkManager that creates fake SDK clients
// in order to not depend on having a real service provider agent running.
class FakeContentAnalysisSdkManager final : public ContentAnalysisSdkManager {
 public:
  FakeContentAnalysisSdkManager();

  ~FakeContentAnalysisSdkManager();

  std::unique_ptr<content_analysis::sdk::Client> CreateClient(
      const content_analysis::sdk::Client::Config& config) override;

  void ResetClient(
      const content_analysis::sdk::Client::Config& config) override;

  void ResetAllClients() override;

  void SetClientSendStatus(int status);

  void SetClientSendResponse(
      const content_analysis::sdk::ContentAnalysisResponse& response);

  void SetClientAckStatus(int status);

  void SetClientCancelStatus(int status);

  void SetCreateClientAbility(bool can_create_client);

  bool NoConnectionEstablished();

  FakeContentAnalysisSdkClient* GetFakeClient(
      const content_analysis::sdk::Client::Config& config);

 private:
  int send_status_ = 0;
  int ack_status_ = 0;
  int cancel_status_ = 0;
  bool can_create_client_ = true;
  content_analysis::sdk::ContentAnalysisResponse response_;

  constexpr static auto CompareConfig =
      [](const content_analysis::sdk::Client::Config& c1,
         const content_analysis::sdk::Client::Config& c2) {
        return (c1.name < c2.name) ||
               (c1.name == c2.name && !c1.user_specific && c2.user_specific);
      };

  std::map<content_analysis::sdk::Client::Config,
           raw_ptr<FakeContentAnalysisSdkClient, CtnExperimental>,
           decltype(CompareConfig)>
      fake_clients_{CompareConfig};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_CONTENT_ANALYSIS_SDK_MANAGER_H_
