// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_strip_ntb_webui_host.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "cc/slim/layer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android {
namespace {

using TabStripNtbWebUiHostTest = ChromeRenderViewHostTestHarness;

TEST_F(TabStripNtbWebUiHostTest, ResetOnTabModelRemovedUnblocksOtrDestruction) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto otr_tab_model = std::make_unique<OwningTestTabModel>(otr_profile);

  TabStripNtbWebUiHost host(otr_tab_model.get(), base::TimeTicks(),
                            /*java_coordinator=*/nullptr);
  ASSERT_NE(nullptr, host.GetWebContents());
  content::WebContentsDestroyedWatcher watcher(host.GetWebContents());

  // Removing an unrelated TabModel is a no-op.
  {
    OwningTestTabModel other_tab_model(profile());
  }
  EXPECT_NE(nullptr, host.GetWebContents());

  // Destroying the observed TabModel removes it from TabModelList, which tears
  // down the WebContents and service so the TabModel and OTR Profile can be
  // destroyed before the host itself.
  otr_tab_model.reset();
  EXPECT_TRUE(watcher.IsDestroyed());
  EXPECT_EQ(nullptr, host.GetWebContents());
  EXPECT_EQ(nullptr, host.GetLayer());

  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile);
  EXPECT_FALSE(profile()->HasPrimaryOTRProfile());

  // Subsequent calls and host destruction are safe no-ops.
  host.Resize(32, 32);
}

TEST_F(TabStripNtbWebUiHostTest, RecordsActivityCreationToFirstPaintOnce) {
  base::HistogramTester histogram_tester;
  OwningTestTabModel tab_model(profile());
  const base::TimeTicks activity_creation_time =
      base::TimeTicks() + base::Seconds(10);

  TabStripNtbWebUiHost host(&tab_model, activity_creation_time,
                            /*java_coordinator=*/nullptr);
  host.OnFirstContentfulPaintInPrimaryMainFrame(activity_creation_time +
                                                base::Milliseconds(125));
  histogram_tester.ExpectUniqueTimeSample(
      "Android.TabStrip.NewTabButton.ActivityCreationToFirstPaint",
      base::Milliseconds(125), 1);

  // Subsequent FCP notifications do not record a second sample.
  host.OnFirstContentfulPaintInPrimaryMainFrame(activity_creation_time +
                                                base::Milliseconds(250));
  histogram_tester.ExpectTotalCount(
      "Android.TabStrip.NewTabButton.ActivityCreationToFirstPaint", 1);
}

}  // namespace
}  // namespace android
