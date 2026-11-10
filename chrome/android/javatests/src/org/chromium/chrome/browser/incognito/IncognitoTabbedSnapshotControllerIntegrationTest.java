// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.WindowManager;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** Integration tests for incognito screenshot protection in {@link ChromeTabbedActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests window flags and features which are activity-global")
public class IncognitoTabbedSnapshotControllerIntegrationTest {

    @Rule
    public final FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private boolean isWindowSecure(ChromeTabbedActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int flags = activity.getWindow().getAttributes().flags;
                    return (flags & WindowManager.LayoutParams.FLAG_SECURE) != 0;
                });
    }

    @Test
    @LargeTest
    @Feature({"Incognito"})
    @DisableFeatures({
        ChromeFeatureList.INCOGNITO_SCREENSHOT,
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
    })
    public void testSecureFlags_incognitoScreenshotDisabled() {
        WebPageStation regularPage = mCtaTestRule.startOnBlankPage();
        Tab regularTab = regularPage.getTab();
        assertFalse(
                "Window should not be secure in regular mode",
                isWindowSecure(regularPage.getActivity()));

        IncognitoNewTabPageStation incognitoNtp = regularPage.openNewIncognitoTabFast();
        assertTrue(
                "Window should be secure when incognito tab is showing",
                isWindowSecure(incognitoNtp.getActivity()));

        regularPage = incognitoNtp.selectTabFast(regularTab, WebPageStation::newBuilder);

        assertFalse(
                "Window should not be secure after switching back to regular",
                isWindowSecure(regularPage.getActivity()));
    }

    @Test
    @LargeTest
    @Feature({"Incognito"})
    @EnableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testSecureFlags_incognitoScreenshotEnabled() {
        WebPageStation regularPage = mCtaTestRule.startOnBlankPage();
        assertFalse(
                "Window should not be secure in regular mode",
                isWindowSecure(regularPage.getActivity()));

        IncognitoNewTabPageStation incognitoNtp = regularPage.openNewIncognitoTabOrWindowFast();
        assertFalse(
                "Window should not secure in incognito with INCOGNITO_SCREENSHOT feature flag",
                isWindowSecure(incognitoNtp.getActivity()));
    }

    @Test
    @LargeTest
    @Feature({"Incognito"})
    @DisableFeatures({
        ChromeFeatureList.INCOGNITO_SCREENSHOT,
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
    })
    public void testSecureFlags_incognitoOverviewScreenshotDisabled() {
        WebPageStation regularPage = mCtaTestRule.startOnBlankPage();
        assertFalse(
                "Window should not be secure in regular mode",
                isWindowSecure(regularPage.getActivity()));

        IncognitoNewTabPageStation incognitoNtp = regularPage.openNewIncognitoTabOrWindowFast();
        assertTrue(
                "Window should be secure when incognito tab is showing",
                isWindowSecure(incognitoNtp.getActivity()));

        IncognitoTabSwitcherStation incognitoTabSwitcher = incognitoNtp.openIncognitoTabSwitcher();
        assertTrue(
                "Window should be secure when incognito tab switcher is showing",
                isWindowSecure(incognitoTabSwitcher.getActivity()));

        RegularTabSwitcherStation regularTabSwitcher =
                incognitoTabSwitcher.closeTabAtIndex(0, RegularTabSwitcherStation.class);
        assertFalse(
                "Window should not be secure when regular tab switcher is showing",
                isWindowSecure(regularTabSwitcher.getActivity()));

        regularPage = regularTabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
        assertFalse(
                "Window should not be secure in regular mode",
                isWindowSecure(regularPage.getActivity()));
    }
}
