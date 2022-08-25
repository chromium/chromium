// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_manager.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_client.h"

#include <cstddef>

namespace enterprise_connectors {

FakeContentAnalysisSdkManager::FakeContentAnalysisSdkManager() {
  ContentAnalysisSdkManager::SetManagerForTesting(this);
}

FakeContentAnalysisSdkManager::~FakeContentAnalysisSdkManager() {
  ContentAnalysisSdkManager::SetManagerForTesting(nullptr);
}

// TODO(b/226679912): May need to add setter methods to access fake client.
std::unique_ptr<content_analysis::sdk::Client>
FakeContentAnalysisSdkManager::CreateClient(
    const content_analysis::sdk::Client::Config& config) {
  return std::make_unique<FakeContentAnalysisSdkClient>(config);
}

}  // namespace enterprise_connectors
