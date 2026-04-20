// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.net.Uri;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.ssl.AskBeforeHttpDialogFacility;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
})
@EnableFeatures({"HttpsUpgrades", "HttpsFirstDialogUi"})
public class AskBeforeHttpDialogTest {

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private RegularNewTabPageStation mStartStation;
    private EmbeddedTestServer mServer;

    @Before
    public void setUp() {
        mStartStation = mActivityTestRule.startOnNtp();
        mActivityTestRule.waitForActivityCompletelyLoaded();

        mServer = mActivityTestRule.getTestServer();

        int httpPort = Uri.parse(mServer.getURL("/")).getPort();
        int httpsPort = 1234; // Closed port to trigger ERR_CONNECTION_REFUSED

        HttpsUpgradesInterceptor.setHttpPortForTesting(httpPort);
        HttpsUpgradesInterceptor.setHttpsPortForTesting(httpsPort);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.HTTPS_ONLY_MODE_ENABLED, true);
                });
    }

    @After
    public void tearDown() {
        HttpsUpgradesInterceptor.setHttpPortForTesting(0);
        HttpsUpgradesInterceptor.setHttpsPortForTesting(0);
        // Unsets the HFM preference, which also clears the allowlist.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.HTTPS_ONLY_MODE_ENABLED, false);
                });
    }

    @Test
    @LargeTest
    public void testDialogIsShownAndGoBackWorks() {
        // Expect "0 = SHOWN" once and "2 = DONT_PROCEED" once.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("interstitial.https_first_mode.decision", 0)
                        .expectIntRecord("interstitial.https_first_mode.decision", 2)
                        .build();

        String httpUrl = mServer.getURLWithHostName("bad-https.test", "/simple.html");

        // Load an initial page so there is history to go back to.
        String initialUrl = mServer.getURL("/simple.html");
        WebPageStation initialPage =
                mStartStation.loadPageProgrammatically(initialUrl, WebPageStation.newBuilder());

        // Load the HTTP URL to trigger the dialog.
        AskBeforeHttpDialogFacility dialog = new AskBeforeHttpDialogFacility();
        initialPage.loadUrlTo(httpUrl).enterFacility(dialog);

        // Click the "Go back" button (positive button).
        WebPageStation finalPage = dialog.clickGoBack(initialPage, initialUrl);

        // Verify that the browser navigated back.
        assertEquals(initialUrl, finalPage.getTab().getUrl().getSpec());

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testDialogIsShownAndContinueToSiteWorks() {
        // Expect "0 = SHOWN" once and "1 = PROCEED" once.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("interstitial.https_first_mode.decision", 0)
                        .expectIntRecord("interstitial.https_first_mode.decision", 1)
                        .build();

        String httpUrl = mServer.getURLWithHostName("bad-https.test", "/simple.html");

        // Load the HTTP URL to trigger the dialog.
        AskBeforeHttpDialogFacility dialog = new AskBeforeHttpDialogFacility();
        mStartStation.loadUrlTo(httpUrl).enterFacility(dialog);

        // Click the "Continue to site" button (negative button).
        WebPageStation fallbackPage = dialog.clickContinue(mStartStation, httpUrl);

        // Verify that the page loads the HTTP URL.
        assertEquals(httpUrl, fallbackPage.getTab().getUrl().getSpec());

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testDialogIsDismissedOnBackPress() {
        // Expect "0 = SHOWN" once and "2 = DONT_PROCEED" once.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("interstitial.https_first_mode.decision", 0)
                        .expectIntRecord("interstitial.https_first_mode.decision", 2)
                        .build();

        String httpUrl = mServer.getURLWithHostName("bad-https.test", "/simple.html");

        // Load an initial page so there is history to go back to.
        String initialUrl = mServer.getURL("/simple.html");
        WebPageStation initialPage =
                mStartStation.loadPageProgrammatically(initialUrl, WebPageStation.newBuilder());

        // Load the HTTP URL to trigger the dialog.
        AskBeforeHttpDialogFacility dialog = new AskBeforeHttpDialogFacility();
        initialPage.loadUrlTo(httpUrl).enterFacility(dialog);

        // Simulate a system back press.
        WebPageStation finalPage = dialog.pressBack(initialPage, initialUrl);

        // Verify that the "Go back" action is triggered, returning to the original URL.
        assertEquals(initialUrl, finalPage.getTab().getUrl().getSpec());

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testDialogIsShownAndClickLearnMoreWorks() {
        // Expect 0 = "TOTAL_VISITS" and 4 = "SHOW_LEARN_MORE" once, and 0 = "SHOW" once.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("interstitial.https_first_mode.interaction", 0)
                        .expectIntRecord("interstitial.https_first_mode.interaction", 4)
                        .expectIntRecord("interstitial.https_first_mode.decision", 0)
                        .build();

        String httpUrl = mServer.getURLWithHostName("bad-https.test", "/simple.html");

        // Load the HTTP URL to trigger the dialog.
        AskBeforeHttpDialogFacility dialog = new AskBeforeHttpDialogFacility();
        mStartStation.loadUrlTo(httpUrl).enterFacility(dialog);

        // Click the learn more link.
        WebPageStation newTab = dialog.clickLearnMore(mStartStation);

        // Verify that the new tab navigated to the help center article.
        assertTrue(newTab.getTab().getUrl().getSpec().contains("p=first_mode"));

        // Close the new tab, which should bring us back to the original tab and resume the dialog.
        ChromeTabUtils.closeCurrentTab(
                androidx.test.platform.app.InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity());

        // Wait for the dialog to be shown again on the original tab.
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getModalDialogManager().isShowing());
        org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkCurrentPresenter(
                mActivityTestRule.getActivity().getModalDialogManager(),
                org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType.TAB);

        // We should not log a user decision or double count SHOW, as we treat the dialog as still
        // being present.
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testDirectHttpsNavigationFailed_NoWarningShown() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("interstitial.https_first_mode.decision")
                        .build();

        String httpsUrl = "https://bad-https.test:1234/simple.html";

        // Load the HTTPS URL directly. This should not trigger the ABH dialog,
        // but instead show a net-error (connection refused).
        WebPageStation errorPage =
                mStartStation.loadPageProgrammatically(httpsUrl, WebPageStation.newBuilder());

        // Check the URL remains HTTPS.
        assertEquals(httpsUrl, errorPage.getTab().getUrl().getSpec());

        // Check no ABH warning was shown.
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testFailedUpgrade_WarningShown_BackForwardNavigations() {
        // Expect "0 = SHOWN" twice and "2 = DONT_PROCEED" once.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes("interstitial.https_first_mode.decision", 0, 2)
                        .expectIntRecord("interstitial.https_first_mode.decision", 2)
                        .build();

        String httpUrl = mServer.getURLWithHostName("bad-https.test", "/simple.html");

        // Load an initial page so there is history to go back to.
        String initialUrl = mServer.getURL("/simple.html");
        WebPageStation initialPage =
                mStartStation.loadPageProgrammatically(initialUrl, WebPageStation.newBuilder());

        // Load the HTTP URL to trigger the dialog.
        AskBeforeHttpDialogFacility dialog = new AskBeforeHttpDialogFacility();
        initialPage.loadUrlTo(httpUrl).enterFacility(dialog);

        // Press the browser back button by simulating a system back press.
        WebPageStation finalPage = dialog.pressBack(initialPage, initialUrl);

        assertEquals(initialUrl, finalPage.getTab().getUrl().getSpec());

        // Press the forward button. Wait for the dialog to reappear.
        ThreadUtils.runOnUiThreadBlocking(() -> finalPage.getTab().goForward());

        // Wait for the dialog to be shown again.
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getModalDialogManager().isShowing());
        org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkCurrentPresenter(
                mActivityTestRule.getActivity().getModalDialogManager(),
                org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType.TAB);

        // Verify we navigated to the http url that is displaying the dialog
        assertEquals(httpUrl, finalPage.getTab().getUrl().getSpec());

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testFailedUpgrade_WarningShown_CloseTab() {
        // Expect "0 = SHOWN" once and "2 = DONT_PROCEED" once.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("interstitial.https_first_mode.decision", 0)
                        .expectIntRecord("interstitial.https_first_mode.decision", 2)
                        .build();

        String httpUrl = mServer.getURLWithHostName("bad-https.test", "/simple.html");

        // Load the HTTP URL to trigger the dialog.
        AskBeforeHttpDialogFacility dialog = new AskBeforeHttpDialogFacility();
        mStartStation.loadUrlTo(httpUrl).enterFacility(dialog);

        // Close the tab.
        ChromeTabUtils.closeCurrentTab(
                androidx.test.platform.app.InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity());

        histogramWatcher.assertExpected();
    }
}
