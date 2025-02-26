// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createTrustedWebActivityIntent;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.isTrustedWebActivity;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;

import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for launching {@link
 * org.chromium.chrome.browser.customtabs.CustomTabActivity} in Trusted Web Activity Mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "https://crbug.com/1454648")
public class TrustedWebActivityTest {
    // TODO(peconn): Add test for navigating away from the trusted origin.
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain()
                    .around(mCustomTabActivityTestRule)
                    .around(mEmbeddedTestServerRule);

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private String mTestPage;

    @Before
    public void setUp() {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized();

        mEmbeddedTestServerRule.setServerUsesHttps(true); // TWAs only work with HTTPS.
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);

        // Map non-localhost-URLs to localhost. Navigations to non-localhost URLs will throw a
        // certificate error.
        Uri mapToUri = Uri.parse(mEmbeddedTestServerRule.getServer().getURL("/"));
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
    }

    private void assertLaunchCauseMetrics(boolean launchedTWA) {
        assertEquals(
                launchedTWA ? 1 : 0,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.TWA));
        assertEquals(
                launchedTWA ? 0 : 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.CUSTOM_TAB));
    }

    @Test
    @MediumTest
    public void launchesTwa() throws TimeoutException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        launchCustomTabActivity(intent);

        assertTrue(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
        assertLaunchCauseMetrics(true);
    }

    @Test
    @MediumTest
    public void doesntLaunchTwa_WithoutFlag() throws TimeoutException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
        launchCustomTabActivity(intent);

        assertFalse(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
        assertLaunchCauseMetrics(false);
    }

    @Test
    @MediumTest
    public void leavesTwa_VerificationFailure() throws TimeoutException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertFalse(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
        assertLaunchCauseMetrics(true);
    }

    /**
     * Test that if the page provides a theme-color and the toolbar color was specified in the
     * intent that the page theme-color is used for the status bar.
     */
    @Test
    @MediumTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE})
    // Customizing status bar color is disallowed for tablets.
    public void testStatusBarColorHasPageThemeColor() throws ExecutionException, TimeoutException {
        final String pageWithThemeColor =
                mEmbeddedTestServerRule
                        .getServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");

        Intent intent = createTrustedWebActivityIntent(pageWithThemeColor);
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.GREEN);
        launchCustomTabActivity(intent);
        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        ThemeTestUtils.assertStatusBarColor(activity, Color.RED);
    }

    /**
     * Test that if the page does not provide a theme-color and the toolbar color was specified in
     * the intent that the toolbar color is used for the status bar.
     */
    @Test
    @MediumTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE})
    public void testStatusBarColorNoPageThemeColor() throws ExecutionException, TimeoutException {
        final String pageWithThemeColor =
                mEmbeddedTestServerRule
                        .getServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");
        final String pageWithoutThemeColor =
                mEmbeddedTestServerRule.getServer().getURL("/chrome/test/data/android/about.html");

        // Navigate to page with a theme color so that we can later wait for the status bar color to
        // change back to the intent color.
        Intent intent = createTrustedWebActivityIntent(pageWithThemeColor);
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.GREEN);
        launchCustomTabActivity(intent);
        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        ThemeTestUtils.assertStatusBarColor(activity, Color.RED);

        mCustomTabActivityTestRule.loadUrl(pageWithoutThemeColor);
        // Use longer-than-default timeout to give page time to finish loading.
        ThemeTestUtils.waitForThemeColor(activity, Color.GREEN, /* timeoutMs= */ 10000);
        ThemeTestUtils.assertStatusBarColor(activity, Color.GREEN);
    }

    /**
     * Test that if the page has a certificate error, that the system default color is used
     * (regardless of whether the page provided a theme-color or a toolbar color was specified in
     * the intent).
     */
    @Test
    @MediumTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest(message = "b/352624584")
    public void testStatusBarColorCertificateError() throws ExecutionException, TimeoutException {
        final String pageWithThemeColor =
                mEmbeddedTestServerRule
                        .getServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");
        final String pageWithThemeColorCertError =
                "https://certificateerror.com/chrome/test/data/android/theme_color_test2.html";

        // Initially don't set certificate error so that we can later wait for the status bar color
        // to change back to the default color.
        Intent intent = createTrustedWebActivityIntent(pageWithThemeColor);
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.GREEN);
        addTrustedOriginToIntent(intent, pageWithThemeColorCertError);
        launchCustomTabActivity(intent);
        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        ThemeTestUtils.assertStatusBarColor(activity, Color.RED);

        spoofVerification(PACKAGE_NAME, pageWithThemeColorCertError);
        ChromeTabUtils.loadUrlOnUiThread(activity.getActivityTab(), pageWithThemeColorCertError);

        int defaultColor =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> ThemeTestUtils.getDefaultThemeColor(activity.getActivityTab()));
        int expectedColor = defaultColor;
        // Use longer-than-default timeout to give page time to finish loading.
        ThemeTestUtils.waitForThemeColor(activity, defaultColor, /* timeoutMs= */ 10000);
        ThemeTestUtils.assertStatusBarColor(activity, expectedColor);
    }

    public void launchCustomTabActivity(Intent intent) throws TimeoutException {
        String url = intent.getData().toString();
        spoofVerification(PACKAGE_NAME, url);
        createSession(intent, PACKAGE_NAME);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    /**
     * Test that trusted web activities show the toolbar when the page has a certificate error (and
     * origin verification succeeds).
     */
    @Test
    @MediumTest
    public void testToolbarVisibleCertificateError() throws ExecutionException, TimeoutException {
        final String pageWithoutCertError =
                mEmbeddedTestServerRule.getServer().getURL("/chrome/test/data/android/about.html");
        final String pageWithCertError =
                "https://certificateerror.com/chrome/test/data/android/theme_color_test.html";

        // Initially don't set certificate error so that we can later wait for the toolbar to hide.
        Intent intent = createTrustedWebActivityIntent(pageWithoutCertError);
        addTrustedOriginToIntent(intent, pageWithCertError);
        launchCustomTabActivity(createTrustedWebActivityIntent(pageWithoutCertError));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        assertEquals(BrowserControlsState.HIDDEN, getBrowserControlConstraints(tab));

        spoofVerification(PACKAGE_NAME, pageWithCertError);
        ChromeTabUtils.loadUrlOnUiThread(tab, pageWithCertError);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            getBrowserControlConstraints(tab),
                            Matchers.is(BrowserControlsState.SHOWN));
                },
                10000,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    public void addTrustedOriginToIntent(Intent intent, String trustedOrigin) {
        ArrayList<String> additionalTrustedOrigins = new ArrayList<>();
        additionalTrustedOrigins.add(trustedOrigin);
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_ADDITIONAL_TRUSTED_ORIGINS,
                additionalTrustedOrigins);
    }

    private @BrowserControlsState int getBrowserControlConstraints(Tab tab) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> TabBrowserControlsConstraintsHelper.getConstraints(tab));
    }
}
