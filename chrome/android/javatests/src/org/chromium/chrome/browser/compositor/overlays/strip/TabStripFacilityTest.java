// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tab_strip.TabStripFacility;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for {@link TabStripFacility}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
public class TabStripFacilityTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Test
    @MediumTest
    public void testOpenNewTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        RegularNewTabPageStation ntp = page.expectTabStrip().openNewTab();

        assertEquals(2, getTabCount(ntp.getTabModelSelector(), /* incognito= */ false));
    }

    @Test
    @MediumTest
    // With this feature, e.g. on desktop, incognito tabs open in a new window, not on this strip.
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOpenNewIncognitoTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        IncognitoNewTabPageStation incognitoNtp = page.openNewIncognitoTabFast();

        IncognitoNewTabPageStation secondIncognitoNtp =
                incognitoNtp.expectTabStrip().openNewIncognitoTab();

        assertEquals(
                2, getTabCount(secondIncognitoNtp.getTabModelSelector(), /* incognito= */ true));
    }

    @Test
    @MediumTest
    public void testSelectTab() {
        WebPageStation firstPage = mActivityTestRule.startOnBlankPage();
        Tab firstTab = firstPage.getTab();
        WebPageStation secondPage = firstPage.openFakeLinkToWebPage("about:blank");

        WebPageStation selectedPage =
                secondPage
                        .expectTabStrip()
                        .expectTab(firstTab.getId())
                        .select(WebPageStation.newBuilder());

        assertEquals(firstTab, selectedPage.getTab());
    }

    @Test
    @MediumTest
    public void testCloseTab() {
        WebPageStation firstPage = mActivityTestRule.startOnBlankPage();
        Tab firstTab = firstPage.getTab();
        WebPageStation secondPage = firstPage.openFakeLinkToWebPage("about:blank");

        secondPage.expectTabStrip().expectTab(firstTab.getId()).close();

        assertTrue(ThreadUtils.runOnUiThreadBlocking(firstTab::isClosing));
    }

    @Test
    @MediumTest
    // The model selector button is only shown when incognito tabs open in the same window.
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testSwitchModel() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab regularTab = page.getTab();
        IncognitoNewTabPageStation incognitoNtp = page.openNewIncognitoTabFast();

        WebPageStation regularPage =
                incognitoNtp.expectTabStrip().switchModel(WebPageStation.newBuilder());

        assertEquals(regularTab, regularPage.getTab());
        assertFalse(
                ThreadUtils.runOnUiThreadBlocking(
                        regularPage.getTabModelSelector()::isIncognitoSelected));
    }

    private static int getTabCount(TabModelSelector selector, boolean incognito) {
        return ThreadUtils.runOnUiThreadBlocking(() -> selector.getModel(incognito).getCount());
    }
}
