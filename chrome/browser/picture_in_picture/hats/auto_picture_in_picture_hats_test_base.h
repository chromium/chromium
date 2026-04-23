// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_HATS_AUTO_PICTURE_IN_PICTURE_HATS_TEST_BASE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_HATS_AUTO_PICTURE_IN_PICTURE_HATS_TEST_BASE_H_

#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

class AutoPictureInPictureHatsTestBase
    : public ChromeRenderViewHostTestHarness {
 public:
  AutoPictureInPictureHatsTestBase();

  ~AutoPictureInPictureHatsTestBase() override;

  void SetUp() override;
  void TearDown() override;

  virtual std::vector<base::test::FeatureRef> GetEnabledFeatures() = 0;
  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() = 0;

  MockHatsService* mock_hats_service() { return mock_hats_service_; }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockHatsService> mock_hats_service_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_HATS_AUTO_PICTURE_IN_PICTURE_HATS_TEST_BASE_H_
