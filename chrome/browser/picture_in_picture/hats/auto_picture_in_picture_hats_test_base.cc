// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_test_base.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"

AutoPictureInPictureHatsTestBase::AutoPictureInPictureHatsTestBase()
    : ChromeRenderViewHostTestHarness(
          content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

AutoPictureInPictureHatsTestBase::~AutoPictureInPictureHatsTestBase() = default;

void AutoPictureInPictureHatsTestBase::SetUp() {
  feature_list_.InitWithFeatures(GetEnabledFeatures(), GetDisabledFeatures());
  ChromeRenderViewHostTestHarness::SetUp();

  // Inject the MockHatsService.
  mock_hats_service_ = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
}

void AutoPictureInPictureHatsTestBase::TearDown() {
  mock_hats_service_ = nullptr;
  ChromeRenderViewHostTestHarness::TearDown();
}

std::vector<base::test::FeatureRef>
AutoPictureInPictureHatsTestBase::GetEnabledFeatures() {
  return {media::kAutoPictureInPictureSurveys};
}

std::vector<base::test::FeatureRef>
AutoPictureInPictureHatsTestBase::GetDisabledFeatures() {
  return {};
}
