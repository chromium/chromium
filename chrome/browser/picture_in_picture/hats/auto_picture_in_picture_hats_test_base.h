// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_HATS_AUTO_PICTURE_IN_PICTURE_HATS_TEST_BASE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_HATS_AUTO_PICTURE_IN_PICTURE_HATS_TEST_BASE_H_

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutoPictureInPictureHatsTestBase : public testing::Test {
 public:
  AutoPictureInPictureHatsTestBase();

  ~AutoPictureInPictureHatsTestBase() override;

  void SetUp() override;
  void TearDown() override;

  virtual std::vector<base::test::FeatureRef> GetEnabledFeatures() = 0;
  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() = 0;

  TestingProfile* profile() { return profile_.get(); }
  MockHatsService* mock_hats_service() { return mock_hats_service_; }

 protected:
  base::test::ScopedFeatureList feature_list_;

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MockHatsService> mock_hats_service_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_HATS_AUTO_PICTURE_IN_PICTURE_HATS_TEST_BASE_H_
