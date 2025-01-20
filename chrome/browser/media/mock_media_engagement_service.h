// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MOCK_MEDIA_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_MEDIA_MOCK_MEDIA_ENGAGEMENT_SERVICE_H_

#include <memory>

#include "chrome/browser/media/media_engagement_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

class KeyedService;
class Profile;

class MockMediaEngagementService : public MediaEngagementService {
 public:
  explicit MockMediaEngagementService(Profile* profile);
  ~MockMediaEngagementService() override;

  MOCK_METHOD(bool,
              HasHighEngagement,
              (const url::Origin& origin),
              (const override));
};

std::unique_ptr<KeyedService> BuildMockMediaEngagementService(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_MEDIA_MOCK_MEDIA_ENGAGEMENT_SERVICE_H_
