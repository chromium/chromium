// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_protection;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.app.Activity;
import android.view.WindowManager;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ChromeTabbedActivityEntryPoints;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.net.test.EmbeddedTestServer;

/** Tabbed activity integration tests for {@link ScreenshotProtectionController} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    // Can't use the @Policies annotation here as DataControlsRules is not allowed as a machine
    // policy. Rule must contain the entire prefix path to match.
    "enable-chrome-browser-cloud-management",
    "policy={\"DataControlsRules\":[{\"restrictions\":"
            + "[{\"class\":\"SCREENSHOT\",\"level\":\"BLOCK\"}],"
            + "\"sources\":{\"urls\":[\"*/chrome/test/data/android/google.html\"]}}]}"
})
@EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_ENTERPRISE_SCREENSHOT_PROTECTION)
@DoNotBatch(reason = "Tests window flags and features which are activity-global")
public class ScreenshotProtectionControllerPTTest {

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private String mRestrictedUrl;
    private String mUnrestrictedUrl;

    @Before
    public void setUp() {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        mRestrictedUrl = testServer.getURL("/chrome/test/data/android/google.html");
        mUnrestrictedUrl = testServer.getURL("/chrome/test/data/android/about.html");
    }

    private static boolean isWindowSecure(Activity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int flags = activity.getWindow().getAttributes().flags;
                    return (flags & WindowManager.LayoutParams.FLAG_SECURE) != 0;
                });
    }

    @Test
    @LargeTest
    public void testNTPToRestrictedPageAndBack() {
        RegularNewTabPageStation ntp =
                ChromeTabbedActivityEntryPoints.startOnNtp(mActivityTestRule);
        assertFalse("NTP should not be secure", isWindowSecure(mActivityTestRule.getActivity()));

        WebPageStation restrictedPage = ntp.loadWebPageProgrammatically(mRestrictedUrl);
        assertTrue(
                "Restricted page should be secure after nav",
                isWindowSecure(mActivityTestRule.getActivity()));

        restrictedPage.loadPageProgrammatically(
                getOriginalNativeNtpUrl(), RegularNewTabPageStation.newBuilder());
        assertFalse(
                "NTP should not be secure after nav",
                isWindowSecure(mActivityTestRule.getActivity()));
    }

    @Test
    @LargeTest
    public void testRestrictedUnrestrictedTabSwitch() {
        WebPageStation restrictedPage =
                ChromeTabbedActivityEntryPoints.startOnUrl(mActivityTestRule, mRestrictedUrl);
        Tab restrictedTab = restrictedPage.getTab();
        assertTrue(
                "Restricted page should be secure",
                isWindowSecure(mActivityTestRule.getActivity()));

        WebPageStation unrestrictedPage =
                restrictedPage.openNewTabFast().loadWebPageProgrammatically(mUnrestrictedUrl);
        assertFalse(
                "Unrestricted page should not be secure",
                isWindowSecure(mActivityTestRule.getActivity()));

        WebPageStation restrictedPageAgain =
                unrestrictedPage.selectTabFast(restrictedTab, WebPageStation::newBuilder);
        assertTrue(
                "Restricted page should be secure again",
                isWindowSecure(mActivityTestRule.getActivity()));
    }

    @Test
    @LargeTest
    public void testTabSwitcherSecureWhenManaged() {
        RegularNewTabPageStation ntp =
                ChromeTabbedActivityEntryPoints.startOnNtp(mActivityTestRule);
        WebPageStation restrictedPage = ntp.loadWebPageProgrammatically(mRestrictedUrl);
        assertTrue(
                "Restricted page should be secure",
                isWindowSecure(mActivityTestRule.getActivity()));

        RegularTabSwitcherStation tabSwitcher = restrictedPage.openRegularTabSwitcher();
        assertTrue(
                "Tab switcher should be secure when managed",
                isWindowSecure(mActivityTestRule.getActivity()));

        WebPageStation unrestrictedPage =
                tabSwitcher.openNewTab().loadWebPageProgrammatically(mUnrestrictedUrl);
        tabSwitcher = unrestrictedPage.openRegularTabSwitcher();
        assertTrue(
                "Tab switcher should be secure when managed",
                isWindowSecure(mActivityTestRule.getActivity()));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testIncognitoScreenshotFeatureFlag() {
        WebPageStation standardPage =
                ChromeTabbedActivityEntryPoints.startOnUrl(mActivityTestRule, mUnrestrictedUrl);
        assertFalse(
                "Standard unrestricted page should not be secure",
                isWindowSecure(standardPage.getActivity()));

        IncognitoNewTabPageStation incognitoNtp = standardPage.openNewIncognitoTabOrWindowFast();
        WebPageStation restrictedIncognito =
                incognitoNtp.loadWebPageProgrammatically(mRestrictedUrl);
        assertTrue(
                "Restricted incognito page should be secure",
                isWindowSecure(restrictedIncognito.getActivity()));

        WebPageStation unrestrictedIncognito =
                restrictedIncognito
                        .openNewIncognitoTabFast()
                        .loadWebPageProgrammatically(mUnrestrictedUrl);
        assertFalse(
                "Unrestricted incognito page should not be secure with feature flag enabled",
                isWindowSecure(unrestrictedIncognito.getActivity()));

        IncognitoTabSwitcherStation incognitoTabSwitcher =
                unrestrictedIncognito.openIncognitoTabSwitcher();
        assertTrue(
                "Incognito hub should be secure",
                isWindowSecure(incognitoTabSwitcher.getActivity()));
    }
}
