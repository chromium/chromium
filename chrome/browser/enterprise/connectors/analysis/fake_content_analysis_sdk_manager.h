// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FAKE_CONTENT_ANALYSIS_SDK_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FAKE_CONTENT_ANALYSIS_SDK_MANAGER_H_

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_client.h"
#include "third_party/content_analysis_sdk/src/browser/include/content_analysis/sdk/analysis_client.h"

namespace enterprise_connectors {
// A derivative of ContentAnalysisSdkManager that creates fake SDK clients
// in order to not depend on having a real service provider agent running.
class FakeContentAnalysisSdkManager : public ContentAnalysisSdkManager {
 protected:
  FakeContentAnalysisSdkManager();

  ~FakeContentAnalysisSdkManager();

  std::unique_ptr<content_analysis::sdk::Client> CreateClient(
      const content_analysis::sdk::Client::Config& config) override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FAKE_CONTENT_ANALYSIS_SDK_MANAGER_H_
