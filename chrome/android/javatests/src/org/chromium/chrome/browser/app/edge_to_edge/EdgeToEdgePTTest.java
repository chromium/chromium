// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.edge_to_edge;

import static org.chromium.chrome.test.transit.edge_to_edge.ViewportFitCoverPageStation.loadViewportFitCoverPage;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.SmallTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.edge_to_edge.ViewportFitCoverPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.UiRestriction;

/** Tests for edge to edge using public transit. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features="
            + ChromeFeatureList.BOTTOM_BROWSER_CONTROLS_REFACTOR
            + "<Study,DynamicSafeAreaInsets,DynamicSafeAreaInsetsOnScroll,DrawCutoutEdgeToEdge,"
            + ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
    "force-fieldtrials=Study/Group",
    "force-fieldtrial-params=Study.Group:disable_bottom_controls_stacker_y_offset/false"
})
@Batch(Batch.PER_CLASS)
// Bots <= VERSION_CODES.S use 3-bottom nav bar. See crbug.com/352402600
@MinAndroidSdkLevel(VERSION_CODES.S_V2)
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
public class EdgeToEdgePTTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BatchedPublicTransitRule<WebPageStation> mBatchedRule =
            new BatchedPublicTransitRule<>(WebPageStation.class, /* expectResetByTest= */ false);

    ChromeTabbedActivityPublicTransitEntryPoints mEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(sActivityTestRule);

    @Test
    @SmallTest
    public void loadViewportFitCover() {
        WebPageStation blankPage = mEntryPoints.startOnBlankPage(mBatchedRule);
        ViewportFitCoverPageStation e2ePage =
                loadViewportFitCoverPage(sActivityTestRule, blankPage);
        TransitAsserts.assertFinalDestination(e2ePage);
    }
}
