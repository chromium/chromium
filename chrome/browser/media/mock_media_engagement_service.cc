// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/mock_media_engagement_service.h"

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

using ::testing::NiceMock;

MockMediaEngagementService::MockMediaEngagementService(Profile* profile)
    : MediaEngagementService(profile) {}

MockMediaEngagementService::~MockMediaEngagementService() = default;

std::unique_ptr<KeyedService> BuildMockMediaEngagementService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockMediaEngagementService>>(
      static_cast<Profile*>(context));
}
