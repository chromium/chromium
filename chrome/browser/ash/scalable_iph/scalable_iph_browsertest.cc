// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using ScalableIphBrowserTest = ash::ScalableIphBrowserTestBase;

BASE_FEATURE(kScalableIphTest,
             "ScalableIphTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, RecordEvent) {
  EXPECT_CALL(*mock_tracker(), NotifyEvent("ScalableIphFiveMinTick"));

  scalable_iph::ScalableIph* scalable_iph =
      ash::ScalableIphFactory::GetForProfile(browser()->profile());
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, InvokeIph) {
  ON_CALL(*mock_tracker(), ShouldTriggerHelpUI)
      .WillByDefault([](const base::Feature& feature) {
        return &feature == &kScalableIphTest;
      });

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(kScalableIphTest)));

  scalable_iph::ScalableIphDelegate::BubbleParams expected_params;
  EXPECT_CALL(*mock_delegate(),
              ShowBubble(::testing::Eq(expected_params), ::testing::NotNull()))
      .WillOnce(
          [](const scalable_iph::ScalableIphDelegate::BubbleParams& params,
             std::unique_ptr<scalable_iph::IphSession> session) {
            // Simulate that an IPH gets dismissed.
            session.reset();
          });

  scalable_iph::ScalableIph* scalable_iph =
      ash::ScalableIphFactory::GetForProfile(browser()->profile());
  std::vector<const base::Feature*> features = {&kScalableIphTest};
  scalable_iph->OverrideFeatureListForTesting(features);

  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
}

// TODO(b/284053005): Add a test case for available profiles.
// TODO(b/284053005): Add a test case for invalid event name.
