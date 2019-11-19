// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.util.Base64;

import androidx.annotation.IntDef;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.X509Util;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.util.CertTestUtil;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.Callable;

/** Tests for the Captive portal interstitial. */
@RunWith(ChromeJUnit4ClassRunner.class)
@MediumTest
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// clang-format off
@ParameterizedCommandLineFlags({
  @Switches(),
  @Switches("enable-features=" + ChromeFeatureList.CAPTIVE_PORTAL_CERTIFICATE_LIST),
})
// clang-format on
public class CaptivePortalTest {
    private static final String CAPTIVE_PORTAL_INTERSTITIAL_TITLE_PREFIX = "Connect to";
    private static final String SSL_INTERSTITIAL_TITLE = "Privacy error";
    private static final int INTERSTITIAL_TITLE_UPDATE_TIMEOUT_SECONDS = 5;

    // UMA events copied from ssl_error_handler.h.
    @IntDef({UMAEvent.HANDLE_ALL, UMAEvent.SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE,
            UMAEvent.SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE,
            UMAEvent.SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE,
            UMAEvent.SHOW_SSL_INTERSTITIAL_OVERRIDABLE, UMAEvent.WWW_MISMATCH_FOUND,
            UMAEvent.WWW_MISMATCH_URL_AVAILABLE, UMAEvent.WWW_MISMATCH_URL_NOT_AVAILABLE,
            UMAEvent.SHOW_BAD_CLOCK, UMAEvent.CAPTIVE_PORTAL_CERT_FOUND,
            UMAEvent.WWW_MISMATCH_FOUND_IN_SAN, UMAEvent.SHOW_MITM_SOFTWARE_INTERSTITIAL,
            UMAEvent.OS_REPORTS_CAPTIVE_PORTAL})
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
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mServer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityFromLauncher();
        mServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getContext(), ServerCertificate.CERT_MISMATCHED_NAME);

        CaptivePortalHelper.setOSReportsCaptivePortalForTesting(false);
        CaptivePortalHelper.setCaptivePortalCertificateForTesting("sha256/test");
    }

    @After
    public void tearDown() {
        mServer.stopAndDestroyServer();
    }

    private void waitForInterstitial(final WebContents webContents, final boolean shouldBeShown) {
        CriteriaHelper.pollUiThread(Criteria.equals(shouldBeShown, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return webContents.isShowingInterstitialPage();
            }
        }));
    }

    /** Navigate the tab to an interstitial with a name mismatch error and check if this
    /*  results in a captive portal interstitial.
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
        }
                .waitForTitleUpdate(INTERSTITIAL_TITLE_UPDATE_TIMEOUT_SECONDS);

        Assert.assertEquals(0, tab.getTitle().indexOf(CAPTIVE_PORTAL_INTERSTITIAL_TITLE_PREFIX));
    }

    @Test
    public void testCaptivePortalCertificateListFeature() throws Exception {
        // Add the SPKI of the root cert to captive portal certificate list.
        byte[] rootCertSPKI = CertTestUtil.getPublicKeySha256(X509Util.createCertificateFromBytes(
                CertTestUtil.pemToDer(mServer.getRootCertPemPath())));
        Assert.assertTrue(rootCertSPKI != null);
        CaptivePortalHelper.setCaptivePortalCertificateForTesting(
                "sha256/" + Base64.encodeToString(rootCertSPKI, Base64.NO_WRAP));

        navigateAndCheckCaptivePortalInterstitial();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.HANDLE_ALL));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("interstitial.ssl_error_handler",
                        UMAEvent.SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.CAPTIVE_PORTAL_CERT_FOUND));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.OS_REPORTS_CAPTIVE_PORTAL));
    }

    @Test
    public void testOSReportsCaptivePortal() throws Exception {
        CaptivePortalHelper.setOSReportsCaptivePortalForTesting(true);
        navigateAndCheckCaptivePortalInterstitial();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.HANDLE_ALL));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("interstitial.ssl_error_handler",
                        UMAEvent.SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.CAPTIVE_PORTAL_CERT_FOUND));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.OS_REPORTS_CAPTIVE_PORTAL));
    }

    /** When CaptivePortalInterstitial feature is disabled, the result of OS captive portal
     *  APIs should be ignored, and a generic SSL interstitial should be displayed.
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

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.HANDLE_ALL));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("interstitial.ssl_error_handler",
                        UMAEvent.SHOW_SSL_INTERSTITIAL_OVERRIDABLE));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.CAPTIVE_PORTAL_CERT_FOUND));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "interstitial.ssl_error_handler", UMAEvent.OS_REPORTS_CAPTIVE_PORTAL));
    }
}
