// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

MockPrivacySandboxService::MockPrivacySandboxService() = default;

MockPrivacySandboxService::~MockPrivacySandboxService() = default;

std::unique_ptr<KeyedService> BuildMockPrivacySandboxService(
    content::BrowserContext* context) {
  return std::make_unique<::testing::NiceMock<MockPrivacySandboxService>>();
}
