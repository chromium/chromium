// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/mock_dlp_content_manager.h"

#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager.h"

namespace policy {

MockDlpContentManager::MockDlpContentManager() = default;

MockDlpContentManager::~MockDlpContentManager() = default;

void MockDlpContentManager::Init() {
  SetReportingManagerForTesting(new DlpReportingManager());
}

}  // namespace policy
