// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.memory_leaks;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.base.test.util.LeakCanaryChecker.EnableLeakChecks;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.CctTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests the behavior of {@link ChromeFeatureList} in instrumentation tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@ImportantFormFactors(DeviceFormFactor.TABLET_OR_DESKTOP)
@Batch(Batch.PER_CLASS)
@EnableLeakChecks
public class PublicTransitLeakTest {
    @Rule
    public FreshCtaTransitTestRule mChromeTabbedActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public CctTransitTestRule mCustomTabActivityTestRule = new CctTransitTestRule();

    @After
    public void tearDown() {
        mChromeTabbedActivityTestRule.closeAllWindowsAndDeleteInstanceAndTabState();
    }

    @Test
    @LargeTest
    public void basicChromeActivityTest() {
        mChromeTabbedActivityTestRule.startOnBlankPage();
    }

    @Test
    @LargeTest
    public void settingsActivityTest() {
        WebPageStation page = mChromeTabbedActivityTestRule.startOnBlankPage();
        page.openRegularTabAppMenu()
                .openSettings()
                .pressBackTo()
                .arriveAt(WebPageStation.newBuilder().initFrom(page).build());
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE)
    public void bookmarksActivityTest() {
        WebPageStation page = mChromeTabbedActivityTestRule.startOnBlankPage();
        page.openRegularTabAppMenu()
                .openBookmarksPhone()
                .pressBackTo()
                .arriveAt(WebPageStation.newBuilder().initFrom(page).build());
    }

    @Test
    @LargeTest
    // For some reason, this test is flaky on desktop.
    @DisableIf.Device(DeviceFormFactor.DESKTOP)
    public void searchActivityTest() {
        var page = mChromeTabbedActivityTestRule.startOnBlankPage();
        var activity = mChromeTabbedActivityTestRule.getActivity();
        page.openRegularTabSwitcher()
                .openTabSwitcherSearch()
                .pressBackToRegularTabSwitcher(activity);
    }

    @Test
    @LargeTest
    public void customTabActivityTest() throws TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        mChromeTabbedActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/google.html")));
    }

    @Test
    @LargeTest
    public void omniboxTest() {
        var page = mChromeTabbedActivityTestRule.startOnBlankPage();
        var omnibox = page.openOmnibox();
        omnibox.pressBackTo().exitFacility();
    }

    @Test
    @LargeTest
    public void ntpAndIncognitoNtpTest() {
        var page = mChromeTabbedActivityTestRule.startOnBlankPage();
        var ntp = page.openNewTabFast();
        ntp.openNewIncognitoTabOrWindowFast();
    }
}
