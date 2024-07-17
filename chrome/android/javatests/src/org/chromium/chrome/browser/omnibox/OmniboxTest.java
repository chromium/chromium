// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.annotation.SuppressLint;
import android.view.KeyEvent;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.SkipCommandLineParameterization;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.EnormousTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.List;

/**
 * Tests of the Omnibox.
 *
 * <p>TODO(yolandyan): Replace the ParameterizedCommandLineFlags with new JUnit4 parameterized
 * framework once it supports Test Rule Parameterization.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@SuppressLint("SetTextI18n")
@Batch(Batch.PER_CLASS)
public class OmniboxTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private void clearUrlBar() {
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    urlBar.setText("");
                });
    }

    private static final OnSuggestionsReceivedListener sEmptySuggestionListener =
            (result, isFinal) -> {};

    /**
     * Sanity check of Omnibox. The problem in http://b/5021723 would cause this to fail (hang or
     * crash).
     */
    @Test
    @EnormousTest
    @Feature({"Omnibox"})
    public void testSimpleUse() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        OmniboxTestUtils omnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
        omnibox.requestFocus();
        omnibox.typeText("aaaaaaa", false);
        omnibox.checkSuggestionsShown();

        ChromeTabUtils.waitForTabPageLoadStart(
                mActivityTestRule.getActivity().getActivityTab(),
                null,
                () -> omnibox.sendKey(KeyEvent.KEYCODE_ENTER),
                20L);
    }

    // Sanity check that no text is displayed in the omnibox when on the NTP page and that the hint
    // text is correct.
    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testDefaultText() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);

        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);

        // Omnibox on NTP shows the hint text.
        Assert.assertNotNull(urlBar);
        Assert.assertEquals("Location bar has text.", "", urlBar.getText().toString());
        Assert.assertEquals(
                "Location bar has incorrect hint.",
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getString(R.string.omnibox_empty_hint),
                urlBar.getHint().toString());

        // Type something in the omnibox.
        // Note that the TextView does not provide a way to test if the hint is showing, the API
        // documentation simply says it shows when the text is empty.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    urlBar.requestFocus();
                    urlBar.setText("G");
                });
        Assert.assertEquals("Location bar should have text.", "G", urlBar.getText().toString());
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testAltEnterOpensSearchResultInNewTab() {
        mActivityTestRule.startMainActivityOnBlankPage();
        int tabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Tab currentTab = mActivityTestRule.getActivity().getActivityTab();

        OmniboxTestUtils omnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
        omnibox.requestFocus();
        omnibox.typeText("hello", false);
        omnibox.checkSuggestionsShown();

        // Dispatch ALT + ENTER key event.
        omnibox.sendKey(KeyEvent.KEYCODE_ENTER, KeyEvent.META_ALT_ON);

        Tab resultTab = mActivityTestRule.getActivity().getActivityTab();
        Assert.assertNotEquals(
                "The result should be loaded in a new tab that is brought to the foreground.",
                currentTab,
                resultTab);
        Assert.assertEquals(
                "Tab count should reflect new tab.",
                tabCount + 1,
                ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity()));
    }

    /**
     * The following test is a basic way to assess how much instant slows down typing in the
     * omnibox. It is meant to be run manually for investigation purposes. When instant was enabled
     * for all suggestions (including searched), I would get a 40% increase in the average time on
     * this test. With instant off, it was almost identical. Marking the test disabled so it is not
     * picked up by our test runner, as it is supposed to be run manually.
     */
    public void manualTestTypingPerformance() throws InterruptedException {
        final String text = "searching for pizza";
        // Type 10 times something on the omnibox and get the average time with and without instant.
        long instantAverage = 0;
        long noInstantAverage = 0;

        for (int i = 0; i < 2; ++i) {
            boolean instantOn = (i == 1);
            mActivityTestRule.setNetworkPredictionEnabled(instantOn);

            for (int j = 0; j < 10; ++j) {
                long before = System.currentTimeMillis();
                mActivityTestRule.typeInOmnibox(text, true);
                if (instantOn) {
                    instantAverage += System.currentTimeMillis() - before;
                } else {
                    noInstantAverage += System.currentTimeMillis() - before;
                }
                clearUrlBar();
                InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            }
        }
        instantAverage /= 10;
        noInstantAverage /= 10;
        System.err.println("******************************************************************");
        System.err.println("**** Instant average=" + instantAverage);
        System.err.println("**** No instant average=" + noInstantAverage);
        System.err.println("******************************************************************");
    }

    /** Test to verify that the security icon is present when visiting http:// URLs. */
    @Test
    @MediumTest
    @SkipCommandLineParameterization
    public void testSecurityIconOnHTTP() {
        mActivityTestRule.startMainActivityOnBlankPage();
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        final String testUrl = testServer.getURL("/chrome/test/data/android/omnibox/one.html");
        mActivityTestRule.loadUrl(testUrl);
        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        StatusCoordinator statusCoordinator = locationBar.getStatusCoordinatorForTesting();
        boolean securityIcon = statusCoordinator.isSecurityViewShown();
        Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
        Assert.assertEquals(
                R.drawable.omnibox_info, statusCoordinator.getSecurityIconResourceIdForTesting());
    }

    /** Test to verify that the security icon is present when visiting https:// URLs. */
    @Test
    @MediumTest
    @SkipCommandLineParameterization
    public void testSecurityIconOnHTTPS() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        EmbeddedTestServer httpsTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        ApplicationProvider.getApplicationContext(), ServerCertificate.CERT_OK);
        CallbackHelper onSSLStateUpdatedCallbackHelper = new CallbackHelper();
        TabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onSSLStateUpdated(Tab tab) {
                        onSSLStateUpdatedCallbackHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getActivityTab().addObserver(observer));

        final String testHttpsUrl =
                httpsTestServer.getURL("/chrome/test/data/android/omnibox/one.html");
        ImageView securityView =
                (ImageView)
                        mActivityTestRule.getActivity().findViewById(R.id.location_bar_status_icon);
        mActivityTestRule.loadUrl(testHttpsUrl);
        onSSLStateUpdatedCallbackHelper.waitForCallback(0);
        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        StatusCoordinator statusCoordinator = locationBar.getStatusCoordinatorForTesting();
        boolean securityIcon = statusCoordinator.isSecurityViewShown();
        Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
        Assert.assertEquals(
                "location_bar_status_icon with wrong resource-id",
                R.id.location_bar_status_icon,
                securityView.getId());
        Assert.assertTrue(securityView.isShown());
        Assert.assertEquals(
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS)
                        ? R.drawable.omnibox_https_valid_refresh
                        : R.drawable.omnibox_https_valid,
                statusCoordinator.getSecurityIconResourceIdForTesting());
    }

    /**
     * Test to verify that the security icon is present after
     *
     * <ol>
     *   <li>visiting a https:// URL
     *   <li>focusing the url bar
     *   <li>pressing back
     * </ol>
     *
     * All while the search engine is not the default one. See https://crbug.com/1173447
     */
    @Test
    @MediumTest
    @SkipCommandLineParameterization
    public void testSecurityIconOnHTTPSFocusAndBack() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        setNonDefaultSearchEngine();

        EmbeddedTestServer httpsTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        ApplicationProvider.getApplicationContext(), ServerCertificate.CERT_OK);
        CallbackHelper onSSLStateUpdatedCallbackHelper = new CallbackHelper();
        TabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onSSLStateUpdated(Tab tab) {
                        onSSLStateUpdatedCallbackHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getActivityTab().addObserver(observer));

        try {
            final String testHttpsUrl =
                    httpsTestServer.getURL("/chrome/test/data/android/omnibox/one.html");

            ImageView securityView =
                    (ImageView)
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.location_bar_status_icon);

            mActivityTestRule.loadUrl(testHttpsUrl);
            onSSLStateUpdatedCallbackHelper.waitForCallback(0);
            final LocationBarLayout locationBar =
                    (LocationBarLayout)
                            mActivityTestRule.getActivity().findViewById(R.id.location_bar);
            final StatusCoordinator statusCoordinator =
                    locationBar.getStatusCoordinatorForTesting();
            final int firstIcon = statusCoordinator.getSecurityIconResourceIdForTesting();

            UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
            ThreadUtils.runOnUiThreadBlocking(() -> urlBar.requestFocus());
            CriteriaHelper.pollUiThread(
                    () -> statusCoordinator.getSecurityIconResourceIdForTesting() != firstIcon);
            final int secondIcon = statusCoordinator.getSecurityIconResourceIdForTesting();
            ThreadUtils.runOnUiThreadBlocking(() -> urlBar.clearFocus());
            CriteriaHelper.pollUiThread(
                    () -> statusCoordinator.getSecurityIconResourceIdForTesting() != secondIcon);

            boolean securityIcon = statusCoordinator.isSecurityViewShown();
            Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
            Assert.assertEquals(
                    "location_bar_status_icon with wrong resource-id",
                    R.id.location_bar_status_icon,
                    securityView.getId());
            Assert.assertTrue(securityView.isShown());
            Assert.assertEquals(
                    ChromeFeatureList.isEnabled(
                                    ChromeFeatureList
                                            .OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS)
                            ? R.drawable.omnibox_https_valid_refresh
                            : R.drawable.omnibox_https_valid,
                    statusCoordinator.getSecurityIconResourceIdForTesting());
        } finally {
            restoreDefaultSearchEngine();
        }
    }

    private void setNonDefaultSearchEngine() {
        TemplateUrlService templateUrlService =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TemplateUrlServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile()));
        ThreadUtils.runOnUiThreadBlocking(() -> templateUrlService.load());
        CriteriaHelper.pollUiThread(() -> templateUrlService.isLoaded());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TemplateUrl> searchEngines = templateUrlService.getTemplateUrls();
                    TemplateUrl defaultEngine =
                            templateUrlService.getDefaultSearchEngineTemplateUrl();

                    TemplateUrl notDefault = null;
                    for (TemplateUrl searchEngine : searchEngines) {
                        if (!searchEngine.equals(defaultEngine)) {
                            notDefault = searchEngine;
                            break;
                        }
                    }

                    Assert.assertNotNull(notDefault);

                    templateUrlService.setSearchEngine(notDefault.getKeyword());
                });
    }

    private void restoreDefaultSearchEngine() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TemplateUrlService service =
                            TemplateUrlServiceFactory.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                    TemplateUrl defaultEngine = service.getDefaultSearchEngineTemplateUrl();
                    service.setSearchEngine(defaultEngine.getKeyword());
                });
    }

    /** Test whether the color of the Location bar is correct for HTTPS scheme. */
    @Test
    @SmallTest
    @SkipCommandLineParameterization
    public void testHttpsLocationBarColor() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        CallbackHelper didThemeColorChangedCallbackHelper = new CallbackHelper();
        CallbackHelper onSSLStateUpdatedCallbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new TabModelSelectorTabObserver(
                            mActivityTestRule.getActivity().getTabModelSelector()) {
                        @Override
                        public void onSSLStateUpdated(Tab tab) {
                            onSSLStateUpdatedCallbackHelper.notifyCalled();
                        }
                    };

                    mActivityTestRule
                            .getActivity()
                            .getRootUiCoordinatorForTesting()
                            .getTopUiThemeColorProvider()
                            .addThemeColorObserver(
                                    new ThemeColorObserver() {
                                        @Override
                                        public void onThemeColorChanged(
                                                int color, boolean shouldAnimate) {
                                            didThemeColorChangedCallbackHelper.notifyCalled();
                                        }
                                    });
                });

        final String testHttpsUrl =
                testServer.getURL("/chrome/test/data/android/theme_color_test.html");
        mActivityTestRule.loadUrl(testHttpsUrl);
        // Tablets don't have website theme colors.
        if (!mActivityTestRule.getActivity().isTablet()) {
            didThemeColorChangedCallbackHelper.waitForCallback(0);
        }
        onSSLStateUpdatedCallbackHelper.waitForCallback(0);
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        ImageView securityView =
                (ImageView)
                        mActivityTestRule.getActivity().findViewById(R.id.location_bar_status_icon);
        boolean securityIcon =
                locationBarLayout.getStatusCoordinatorForTesting().isSecurityViewShown();
        Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
        Assert.assertEquals(
                "location_bar_status_icon with wrong resource-id",
                R.id.location_bar_status_icon,
                securityView.getId());
        if (mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(
                    mActivityTestRule
                            .getActivity()
                            .getToolbarManager()
                            .getLocationBarModelForTesting()
                            .shouldEmphasizeHttpsScheme());
        } else {
            Assert.assertFalse(
                    mActivityTestRule
                            .getActivity()
                            .getToolbarManager()
                            .getLocationBarModelForTesting()
                            .shouldEmphasizeHttpsScheme());
        }
    }
}
