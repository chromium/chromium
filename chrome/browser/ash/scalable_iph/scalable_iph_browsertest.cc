// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using ScalableIphBrowserTest = ash::ScalableIphBrowserTestBase;

}  // namespace

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, RecordEvent) {
  EXPECT_CALL(*mock_tracker(), NotifyEvent("ScalableIphFiveMinTick"));

  scalable_iph::ScalableIph* scalable_iph =
      ash::ScalableIphFactory::GetForProfile(browser()->profile());
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
}

// TODO(b/284053005): Add a test case for invalid event name.
// TODO(b/284053005): Add a test case for checking trigger condition.
