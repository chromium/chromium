// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.net.HttpURLConnection;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for the InfoBars. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class InfoBarTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final long MAX_TIMEOUT = 2000L;
    private static final int CHECK_INTERVAL = 500;
    private static final String POPUP_PAGE =
            "/chrome/test/data/popup_blocker/popup-window-open.html";
    private static final String HELLO_WORLD_URL = UrlUtils.encodeHtmlDataUri("<html>"
            + "<head><title>Hello, World!</title></head>"
            + "<body>Hello, World!</body>"
            + "</html>");
    private static final String SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION =
            "displayed_data_reduction_promo_version";
    private static final String M51_VERSION = "Chrome 51.0.2704.0";

    private EmbeddedTestServer mTestServer;
    private InfoBarTestAnimationListener mListener;

    private static class TestInfoBar extends InfoBar {
        private boolean mCompact;

        private TestInfoBar(String message) {
            super(0, 0, message, null);
        }

        @Override
        protected boolean usesCompactLayout() {
            return mCompact;
        }

        void setUsesCompactLayout(boolean compact) {
            mCompact = compact;
        }
    }

    private static class TestInfoBarWithAccessibilityMessage extends TestInfoBar {
        private CharSequence mAccessibilityMessage;

        private TestInfoBarWithAccessibilityMessage(String message) {
            super(message);
        }

        void setAccessibilityMessage(CharSequence accessibilityMessage) {
            mAccessibilityMessage = accessibilityMessage;
        }

        @Override
        protected CharSequence getAccessibilityMessage(CharSequence defaultMessage) {
            return mAccessibilityMessage;
        }
    }

    private void waitUntilDataReductionPromoInfoBarAppears() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<InfoBar> infobars = mActivityTestRule.getInfoBars();
                return infobars.size() == 1 && infobars.get(0) instanceof DataReductionPromoInfoBar;
            }
        });
    }

    private TabWebContentsDelegateAndroid getTabWebContentsDelegate() {
        return TabTestUtils.getTabWebContentsDelegate(
                mActivityTestRule.getActivity().getActivityTab());
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        // Register for animation notifications
        CriteriaHelper.pollInstrumentationThread(
                () -> mActivityTestRule.getInfoBarContainer() != null);
        mListener =  new InfoBarTestAnimationListener();
        mActivityTestRule.getInfoBarContainer().addAnimationListener(mListener);

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        Context context = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                          .getTargetContext()
                                                          .getApplicationContext());
        ContextUtils.initApplicationContextForTests(context);
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    /**
     * Verify getAccessibilityMessage().
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testGetAccessibilityMessage() {
        TestInfoBar infoBarNoMessage = new TestInfoBar(null);
        infoBarNoMessage.setContext(ContextUtils.getApplicationContext());
        Assert.assertEquals("Infobar shouldn't have accessibility message before createView()", "",
                infoBarNoMessage.getAccessibilityText());
        infoBarNoMessage.createView();
        Assert.assertEquals("Infobar should have accessibility message after createView()",
                ContextUtils.getApplicationContext().getString(R.string.bottom_bar_screen_position),
                infoBarNoMessage.getAccessibilityText());

        TestInfoBar infoBarCompact = new TestInfoBar(null);
        infoBarCompact.setContext(ContextUtils.getApplicationContext());
        infoBarCompact.setUsesCompactLayout(true);
        Assert.assertEquals("Infobar shouldn't have accessibility message before createView()", "",
                infoBarCompact.getAccessibilityText());
        infoBarCompact.createView();
        Assert.assertEquals("Infobar should have accessibility message after createView()",
                ContextUtils.getApplicationContext().getString(R.string.bottom_bar_screen_position),
                infoBarCompact.getAccessibilityText());

        String message = "Hello world";
        TestInfoBar infoBarWithMessage = new TestInfoBar(message);
        infoBarWithMessage.setContext(ContextUtils.getApplicationContext());
        Assert.assertEquals("Infobar shouldn't have accessibility message before createView()", "",
                infoBarWithMessage.getAccessibilityText());
        infoBarWithMessage.createView();
        Assert.assertEquals("Infobar should have accessibility message after createView()",
                message + " "
                        + ContextUtils.getApplicationContext().getString(
                                  R.string.bottom_bar_screen_position),
                infoBarWithMessage.getAccessibilityText());
    }

    /**
     * Verify getAccessibilityMessage() for infobar with customized accessibility message.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testInfobarGetCustomizedAccessibilityMessage() {
        String message = "Hello world";
        String customizedAccessibilityMessage = "Customized";

        TestInfoBarWithAccessibilityMessage infoBarWithAccessibilityMessage =
                new TestInfoBarWithAccessibilityMessage(message);
        infoBarWithAccessibilityMessage.setContext(ContextUtils.getApplicationContext());
        infoBarWithAccessibilityMessage.setAccessibilityMessage(customizedAccessibilityMessage);
        Assert.assertEquals("Infobar shouldn't have accessibility message before createView()", "",
                infoBarWithAccessibilityMessage.getAccessibilityText());
        infoBarWithAccessibilityMessage.createView();
        Assert.assertEquals(
                "Infobar should have customized accessibility message after createView()",
                customizedAccessibilityMessage + " "
                        + ContextUtils.getApplicationContext().getString(
                                  R.string.bottom_bar_screen_position),
                infoBarWithAccessibilityMessage.getAccessibilityText());

        TestInfoBarWithAccessibilityMessage infoBarCompactWithAccessibilityMessage =
                new TestInfoBarWithAccessibilityMessage(message);
        infoBarCompactWithAccessibilityMessage.setContext(ContextUtils.getApplicationContext());
        infoBarCompactWithAccessibilityMessage.setUsesCompactLayout(true);
        infoBarCompactWithAccessibilityMessage.setAccessibilityMessage(
                customizedAccessibilityMessage);
        Assert.assertEquals("Infobar shouldn't have accessibility message before createView()", "",
                infoBarCompactWithAccessibilityMessage.getAccessibilityText());
        infoBarCompactWithAccessibilityMessage.createView();
        Assert.assertEquals(
                "Infobar should have customized accessibility message after createView()",
                customizedAccessibilityMessage + " "
                        + ContextUtils.getApplicationContext().getString(
                                  R.string.bottom_bar_screen_position),
                infoBarCompactWithAccessibilityMessage.getAccessibilityText());
    }

    /**
     * Verify PopUp InfoBar.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @DisabledTest(message = "crbug.com/593003")
    public void testInfoBarForPopUp() throws TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(POPUP_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not added");

        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        Assert.assertEquals("Wrong infobar count", 1, infoBars.size());
        Assert.assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));
        InfoBarUtil.clickPrimaryButton(infoBars.get(0));
        mListener.removeInfoBarAnimationFinished("InfoBar not removed.");
        Assert.assertEquals("Wrong infobar count", 0, infoBars.size());
        Assert.assertNotNull(infoBars.get(0).getSnackbarManager());

        // A second load should not show the infobar.
        mActivityTestRule.loadUrl(mTestServer.getURL(POPUP_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar added when it should not");
    }

    /**
     * Verify Popups create an InfoBar and that it's destroyed when navigating back.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    @RetryOnFailure
    public void testInfoBarForGeolocationDisappearsOnBack() throws TimeoutException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        mActivityTestRule.loadUrl(HELLO_WORLD_URL);
        mActivityTestRule.loadUrl(mTestServer.getURL(POPUP_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not added.");

        Assert.assertEquals("Wrong infobar count", 1, mActivityTestRule.getInfoBars().size());

        // Navigate back and ensure the InfoBar has been removed.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().getActivityTab().goBack();
            }
        });
        InfoBarUtil.waitUntilNoInfoBarsExist(mActivityTestRule.getInfoBars());
        mListener.removeInfoBarAnimationFinished("InfoBar not removed.");
    }

    /**
     * Verify the Data Reduction Promo infobar is shown and clicking the primary button dismisses
     * it.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled")
    @EnableFeatures("DataReductionProxyEnabledWithNetworkService")
    @Feature({"Browser", "Main"})
    @RetryOnFailure
    public void testDataReductionPromoInfoBar() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Data Reduction Proxy enabled",
                    DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
            // Fake the FRE or second run promo being shown in M51.
            DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putString(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, M51_VERSION)
                    .apply();
            // Add an infobar.
            Assert.assertTrue(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                    mActivityTestRule.getActivity(), mActivityTestRule.getWebContents(),
                    "http://google.com", false, false, HttpURLConnection.HTTP_OK));
        });

        waitUntilDataReductionPromoInfoBarAppears();
        final List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        Assert.assertTrue("InfoBar does not have primary button",
                InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        Assert.assertTrue("InfoBar does not have secondary button",
                InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> InfoBarUtil.clickPrimaryButton(infoBars.get(0)));

        // The renderer should have been killed and the infobar removed.
        InfoBarUtil.waitUntilNoInfoBarsExist(mActivityTestRule.getInfoBars());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Data Reduction Proxy not enabled",
                    DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
            // Turn Data Saver off so the promo can be reshown.
            DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                    mActivityTestRule.getActivity(), false);
            // Try to add an infobar. Infobar should not be added since it has already been
            // shown.
            Assert.assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                    mActivityTestRule.getActivity(), mActivityTestRule.getWebContents(),
                    "http://google.com", false, false, HttpURLConnection.HTTP_OK));
        });
    }

    /**
     * Verify the Data Reduction Promo infobar is shown and clicking the secondary button dismisses
     * it.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled")
    @Feature({"Browser", "Main"})
    @RetryOnFailure
    public void testDataReductionPromoInfoBarDismissed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Data Reduction Proxy enabled",
                    DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
            // Fake the first run experience or second run promo being shown in M51.
            DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putString(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, M51_VERSION)
                    .apply();
            // Add an infobar.
            Assert.assertTrue(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                    mActivityTestRule.getActivity(), mActivityTestRule.getWebContents(),
                    "http://google.com", false, false, HttpURLConnection.HTTP_OK));
        });

        waitUntilDataReductionPromoInfoBarAppears();
        final List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        Assert.assertTrue("InfoBar does not have primary button",
                InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        Assert.assertTrue("InfoBar does not have secondary button",
                InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> InfoBarUtil.clickSecondaryButton(infoBars.get(0)));

        // The renderer should have been killed and the infobar removed.
        InfoBarUtil.waitUntilNoInfoBarsExist(mActivityTestRule.getInfoBars());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Data Reduction Proxy enabled",
                    DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
            // Try to add an infobar. Infobar should not be added since the user clicked
            // dismiss.
            Assert.assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                    mActivityTestRule.getActivity(), mActivityTestRule.getWebContents(),
                    "http://google.com", false, false, HttpURLConnection.HTTP_OK));
        });
    }

    /**
     * Verify the Data Reduction Promo infobar is not shown when the fre or second run promo version
     * was not stored and the package was installed after M48.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled")
    @Feature({"Browser", "Main"})
    public void testDataReductionPromoInfoBarPostM48Install() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse("Data Reduction Proxy enabled",
                        DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
                // Fake the first run experience or second run promo being shown.
                DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
                // Remove the version. Versions prior to M51 will not have the version pref.
                ContextUtils.getAppSharedPreferences()
                        .edit()
                        .putString(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, "")
                        .apply();
                // Add an infobar. Infobar should not be added since the first run experience
                // or second run promo version was not shown and the package was installed
                // after M48.
                Assert.assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                        mActivityTestRule.getActivity(), mActivityTestRule.getWebContents(),
                        "http://google.com", false, false, HttpURLConnection.HTTP_OK));
            }
        });
    }

    /**
     * Verify that the Data Reduction Promo infobar is not shown if the first run experience or
     * Infobar promo hasn't been shown or if it hasn't been two versions since the promo was shown.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled")
    @Feature({"Browser", "Main"})
    @RetryOnFailure
    public void testDataReductionPromoInfoBarFreOptOut() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Try to add an infobar. Infobar should not be added since the first run
                // experience or second run promo hasn't been shown.
                Assert.assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                        mActivityTestRule.getActivity(), mActivityTestRule.getWebContents(),
                        "http://google.com", false, false, HttpURLConnection.HTTP_OK));

                // Fake showing the FRE.
                DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();

                // Try to add an infobar. Infobar should not be added since the
                // first run experience was just shown.
                Assert.assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                        mActivityTestRule.getActivity(), mActivityTestRule.getWebContents(),
                        "http://google.com", false, false, HttpURLConnection.HTTP_OK));

                // Fake the first run experience or second run promo being shown in M51.
                DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
                ContextUtils.getAppSharedPreferences()
                        .edit()
                        .putString(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, M51_VERSION)
                        .apply();
                DataReductionPromoUtils.saveFrePromoOptOut(true);

                // Try to add an infobar. Infobar should not be added since the user opted
                // out on the first run experience.
                Assert.assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                        mActivityTestRule.getActivity(), mActivityTestRule.getWebContents(),
                        "http://google.com", false, false, HttpURLConnection.HTTP_OK));
            }
        });
    }

    /**
     * Verifies the unresponsive renderer notification creates an InfoBar.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @RetryOnFailure
    public void testInfoBarForHungRenderer() throws TimeoutException {
        mActivityTestRule.loadUrl(HELLO_WORLD_URL);

        // Fake an unresponsive renderer signal.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            CommandLine.getInstance().appendSwitch(ChromeSwitches.ENABLE_HUNG_RENDERER_INFOBAR);
            getTabWebContentsDelegate().rendererUnresponsive();
        });
        mListener.addInfoBarAnimationFinished("InfoBar not added");

        // Make sure it has Kill/Wait buttons.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        Assert.assertEquals("Wrong infobar count", 1, infoBars.size());
        Assert.assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        Assert.assertTrue(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        // Fake a responsive renderer signal.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> { getTabWebContentsDelegate().rendererResponsive(); });
        mListener.removeInfoBarAnimationFinished("InfoBar not removed.");
        Assert.assertTrue("Wrong infobar count", mActivityTestRule.getInfoBars().isEmpty());
    }

    /**
     * Verifies the hung renderer InfoBar can kill the hung renderer.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @RetryOnFailure
    public void testInfoBarForHungRendererCanKillRenderer() throws TimeoutException {
        mActivityTestRule.loadUrl(HELLO_WORLD_URL);

        // Fake an unresponsive renderer signal.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            CommandLine.getInstance().appendSwitch(ChromeSwitches.ENABLE_HUNG_RENDERER_INFOBAR);
            getTabWebContentsDelegate().rendererUnresponsive();
        });
        mListener.addInfoBarAnimationFinished("InfoBar not added");

        // Make sure it has Kill/Wait buttons.
        final List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        Assert.assertEquals("Wrong infobar count", 1, infoBars.size());
        Assert.assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        Assert.assertTrue(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        // Activate the Kill button.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> { InfoBarUtil.clickPrimaryButton(infoBars.get(0)); });

        // The renderer should have been killed and the InfoBar removed.
        mListener.removeInfoBarAnimationFinished("InfoBar not removed.");
        Assert.assertTrue("Wrong infobar count", mActivityTestRule.getInfoBars().isEmpty());
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return SadTab.isShowing(mActivityTestRule.getActivity().getActivityTab());
            }
        }, MAX_TIMEOUT, CHECK_INTERVAL);
    }

    /**
     * Verify InfoBarContainers swap the WebContents they are monitoring properly.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @RetryOnFailure
    public void testInfoBarContainerSwapsWebContents() throws TimeoutException {
        // Add an infobar.
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        mActivityTestRule.loadUrl(mTestServer.getURL(POPUP_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not added");
        Assert.assertEquals("Wrong infobar count", 1, mActivityTestRule.getInfoBars().size());

        // Swap out the WebContents and send the user somewhere so that the InfoBar gets removed.
        InfoBarTestAnimationListener removeListener = new InfoBarTestAnimationListener();
        mActivityTestRule.getInfoBarContainer().addAnimationListener(removeListener);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            WebContents newContents = WebContentsFactory.createWebContents(false, false);
            TabTestUtils.swapWebContents(
                    mActivityTestRule.getActivity().getActivityTab(), newContents, false, false);
        });
        mActivityTestRule.loadUrl(HELLO_WORLD_URL);
        removeListener.removeInfoBarAnimationFinished("InfoBar not removed.");
        Assert.assertEquals("Wrong infobar count", 0, mActivityTestRule.getInfoBars().size());

        // Revisiting the original page should make the InfoBar reappear.
        InfoBarTestAnimationListener addListener = new InfoBarTestAnimationListener();
        mActivityTestRule.getInfoBarContainer().addAnimationListener(addListener);
        mActivityTestRule.loadUrl(mTestServer.getURL(POPUP_PAGE));
        addListener.addInfoBarAnimationFinished("InfoBar not added");
        Assert.assertEquals("Wrong infobar count", 1, mActivityTestRule.getInfoBars().size());
    }
}
