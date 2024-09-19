// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import androidx.annotation.IntDef;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.security_interstitials.CaptivePortalHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Tests for the Captive portal interstitial. */
@RunWith(ChromeJUnit4ClassRunner.class)
@MediumTest
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@ParameterizedCommandLineFlags({
    @Switches(),
})
public class CaptivePortalTest {
    private static final String CAPTIVE_PORTAL_INTERSTITIAL_TITLE_PREFIX = "Connect to";
    private static final String SSL_INTERSTITIAL_TITLE = "Privacy error";
    private static final int INTERSTITIAL_TITLE_UPDATE_TIMEOUT_SECONDS = 5;

    // UMA events copied from ssl_error_handler.h.
    @IntDef({
        UMAEvent.HANDLE_ALL,
        UMAEvent.SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE,
        UMAEvent.SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE,
        UMAEvent.SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE,
        UMAEvent.SHOW_SSL_INTERSTITIAL_OVERRIDABLE,
        UMAEvent.WWW_MISMATCH_FOUND,
        UMAEvent.WWW_MISMATCH_URL_AVAILABLE,
        UMAEvent.WWW_MISMATCH_URL_NOT_AVAILABLE,
        UMAEvent.SHOW_BAD_CLOCK,
        UMAEvent.CAPTIVE_PORTAL_CERT_FOUND,
        UMAEvent.WWW_MISMATCH_FOUND_IN_SAN,
        UMAEvent.SHOW_MITM_SOFTWARE_INTERSTITIAL,
        UMAEvent.OS_REPORTS_CAPTIVE_PORTAL,
        UMAEvent.SHOW_BLOCKED_INTERCEPTION_INTERSTITIAL,
        UMAEvent.SHOW_LEGACY_TLS_INTERSTITIAL
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface UMAEvent {
        int HANDLE_ALL = 0;
        int SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE = 1;
        int SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE = 2;
        int SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE = 3;
        int SHOW_SSL_INTERSTITIAL_OVERRIDABLE = 4;
        int WWW_MISMATCH_FOUND = 5; // Deprecated in M59 by WWW_MISMATCH_FOUND_IN_SAN.
        int WWW_MISMATCH_URL_AVAILABLE = 6;
        int WWW_MISMATCH_URL_NOT_AVAILABLE = 7;
        int SHOW_BAD_CLOCK = 8;
        int CAPTIVE_PORTAL_CERT_FOUND = 9;
        int WWW_MISMATCH_FOUND_IN_SAN = 10;
        int SHOW_MITM_SOFTWARE_INTERSTITIAL = 11;
        int OS_REPORTS_CAPTIVE_PORTAL = 12;
        int SHOW_BLOCKED_INTERCEPTION_INTERSTITIAL = 13;
        int SHOW_LEGACY_TLS_INTERSTITIAL = 14; // Deprecated in M98.
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mServer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        ApplicationProvider.getApplicationContext(),
                        ServerCertificate.CERT_MISMATCHED_NAME);

        CaptivePortalHelper.setOSReportsCaptivePortalForTesting(false);
    }

    /**
     * Navigate the tab to an interstitial with a name mismatch error and check if this /* results
     * in a captive portal interstitial.
     */
    private void navigateAndCheckCaptivePortalInterstitial() throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.loadUrlOnUiThread(
                tab, mServer.getURL("/chrome/test/data/android/navigate/simple.html"));

        new TabTitleObserver(tab, CAPTIVE_PORTAL_INTERSTITIAL_TITLE_PREFIX) {
            @Override
            protected boolean doesTitleMatch(String expectedTitle, String actualTitle) {
                return actualTitle.indexOf(expectedTitle) == 0;
            }
        }.waitForTitleUpdate(INTERSTITIAL_TITLE_UPDATE_TIMEOUT_SECONDS);
        Assert.assertEquals(
                0,
                ChromeTabUtils.getTitleOnUiThread(tab)
                        .indexOf(CAPTIVE_PORTAL_INTERSTITIAL_TITLE_PREFIX));
    }

    @Test
    public void testOSReportsCaptivePortal() throws Exception {
        CaptivePortalHelper.setOSReportsCaptivePortalForTesting(true);
        navigateAndCheckCaptivePortalInterstitial();

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.HANDLE_ALL));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler",
                        UMAEvent.SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.CAPTIVE_PORTAL_CERT_FOUND));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.OS_REPORTS_CAPTIVE_PORTAL));
    }

    /**
     * When CaptivePortalInterstitial feature is disabled, the result of OS captive portal APIs
     * should be ignored, and a generic SSL interstitial should be displayed.
     */
    @Test
    @CommandLineFlags.Add({"disable-features=CaptivePortalInterstitial"})
    public void testOSReportsCaptivePortal_FeatureDisabled() throws Exception {
        CaptivePortalHelper.setOSReportsCaptivePortalForTesting(true);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.loadUrlOnUiThread(
                tab, mServer.getURL("/chrome/test/data/android/navigate/simple.html"));

        new TabTitleObserver(tab, SSL_INTERSTITIAL_TITLE)
                .waitForTitleUpdate(INTERSTITIAL_TITLE_UPDATE_TIMEOUT_SECONDS);

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.HANDLE_ALL));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler",
                        UMAEvent.SHOW_SSL_INTERSTITIAL_OVERRIDABLE));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.CAPTIVE_PORTAL_CERT_FOUND));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.OS_REPORTS_CAPTIVE_PORTAL));
    }
}
