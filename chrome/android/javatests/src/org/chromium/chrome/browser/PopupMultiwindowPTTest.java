// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.tab_activity_glue.PopupCreator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.CctPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.PopupOnClickPageStation;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests whether popup windows appear as CCTs. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DoNotBatch(reason = "Safer to not batch as we are using multiple Android tasks")
@EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
@DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
@Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
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

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.ONLY_TABLET)
    public void testBasicIncognito() {
        final IncognitoNewTabPageStation incognitoEntryPoint =
                mEntryPage.openNewIncognitoTabOrWindowFast();

        final PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), incognitoEntryPoint);
        assertTrue(page.isIncognito());

        final int initialTabCount = mCtaTestRule.tabsCount(/* incognito= */ true);

        final CctPageStation popup = page.clickLinkToOpenPopupWithBoundsExpectNewWindow();

        // Assert that no tab has been created in lieu of a window
        assertEquals(
                "The number of tabs in the original window is unexpected",
                initialTabCount,
                mCtaTestRule.tabsCount(/* incognito= */ true));

        if (page.getActivity().isIncognitoWindow()) {
            TransitAsserts.assertFinalDestinations(mEntryPage, page, popup);
        } else {
            TransitAsserts.assertFinalDestinations(page, popup);
        }
        assertTrue(popup.isIncognito());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP)
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_RESIZE_AFTER_SPAWN)
    public void testLaunchBoundsSize() throws Exception {
        PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);
        CctPageStation popup = page.clickLinkToOpenPopupWithBoundsExpectNewWindow();

        TransitAsserts.assertFinalDestinations(page, popup);

        // from /chrome/test/data/android/popup_on_click.html
        final int expectedWidthDp = 800;
        final int expectedHeightDp = 600;

        final int receivedWidthDp =
                Integer.parseInt(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                popup.webContentsElement.value(), "window.innerWidth"));
        final int receivedHeightDp =
                Integer.parseInt(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                popup.webContentsElement.value(), "window.innerHeight"));

        assertTrue(
                "Inner width of the popup window is invalid, expected: "
                        + expectedWidthDp
                        + "+-1, got: "
                        + receivedWidthDp,
                expectedWidthDp - 1 <= receivedWidthDp && receivedWidthDp <= expectedWidthDp + 1);
        assertTrue(
                "Inner height of the popup window is invalid, expected: "
                        + expectedHeightDp
                        + "+-1, got: "
                        + receivedHeightDp,
                expectedHeightDp - 1 <= receivedHeightDp
                        && receivedHeightDp <= expectedHeightDp + 1);
    }
}
