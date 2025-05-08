// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.tab_activity_glue.PopupCreator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.CctPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.PopupOnClickPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests whether popup windows appear as CCTs. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
@DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
@Restriction({DeviceFormFactor.TABLET, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
public class PopupMultiwindowPTTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mEntryPage;

    @Before
    public void setUp() {
        mEntryPage = mCtaTestRule.startOnBlankPage();
        PopupCreator.setArePopupsEnabledForTesting(true);
    }

    @Test
    @MediumTest
    public void testBasic() {
        PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        int initialTabCount = mCtaTestRule.tabsCount(/* incognito= */ false);

        CctPageStation popup = page.clickLinkToOpenPopupWithBoundsExpectNewWindow();

        // Assert that no tab has been created in lieu of a window
        assertEquals(
                "The number of tabs in the original window is unexpected",
                initialTabCount,
                mCtaTestRule.tabsCount(/* incognito= */ false));

        TransitAsserts.assertFinalDestinations(page, popup);
    }
}
