// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.infobars.InfoBar;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests for the InfoBars. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE})
public class InfoBarTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

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

    private static EmbeddedTestServer sTestServer = sActivityTestRule.getTestServer();
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

    private TabWebContentsDelegateAndroid getTabWebContentsDelegate() {
        return TabTestUtils.getTabWebContentsDelegate(
                sActivityTestRule.getActivity().getActivityTab());
    }

    @Before
    public void setUp() throws Exception {
        // Register for animation notifications
        CriteriaHelper.pollInstrumentationThread(
                () -> sActivityTestRule.getInfoBarContainer() != null);
        mListener =  new InfoBarTestAnimationListener();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getInfoBarContainer().addAnimationListener(mListener));

        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        Context context = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                          .getTargetContext()
                                                          .getApplicationContext());
        ContextUtils.initApplicationContextForTests(context);
    }

    @After
    public void tearDown() {
        // Unregister animation notifications
        InfoBarContainer container = sActivityTestRule.getInfoBarContainer();
        if (container != null) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> container.removeAnimationListener(mListener));
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
    @DisabledTest(message = "https://crbug.com/1269025")
    public void testInfoBarForPopUp() throws TimeoutException, ExecutionException {
        sActivityTestRule.loadUrl(sTestServer.getURL(POPUP_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not added");

        List<InfoBar> infoBars = sActivityTestRule.getInfoBars();
        Assert.assertEquals("Wrong infobar count", 1, infoBars.size());
        Assert.assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> InfoBarUtil.clickPrimaryButton(infoBars.get(0)));
        InfoBarUtil.waitUntilNoInfoBarsExist(sActivityTestRule.getInfoBars());
        mListener.removeInfoBarAnimationFinished("InfoBar not removed.");

        // A second load should open a popup and should not show the infobar.
        int tabCount = sActivityTestRule.tabsCount(false);
        sActivityTestRule.loadUrl(sTestServer.getURL(POPUP_PAGE));
        CriteriaHelper.pollUiThread(
                () -> { return sActivityTestRule.tabsCount(false) > tabCount; });
        Assert.assertEquals("Wrong infobar count", 0, infoBars.size());
    }

    /**
     * Verify Popups create an InfoBar and that it's destroyed when navigating back.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarForPopUpDisappearsOnBack() throws TimeoutException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        sActivityTestRule.loadUrl(HELLO_WORLD_URL);
        sActivityTestRule.loadUrl(sTestServer.getURL(POPUP_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not added.");

        Assert.assertEquals("Wrong infobar count", 1, sActivityTestRule.getInfoBars().size());

        // Navigate back and ensure the InfoBar has been removed.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                sActivityTestRule.getActivity().getActivityTab().goBack();
            }
        });
        InfoBarUtil.waitUntilNoInfoBarsExist(sActivityTestRule.getInfoBars());
        mListener.removeInfoBarAnimationFinished("InfoBar not removed.");
    }

    /**
     * Verify InfoBarContainers swap the WebContents they are monitoring properly.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testInfoBarContainerSwapsWebContents() throws TimeoutException {
        // Add an infobar.
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        sActivityTestRule.loadUrl(sTestServer.getURL(POPUP_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not added");
        Assert.assertEquals("Wrong infobar count", 1, sActivityTestRule.getInfoBars().size());

        // Swap out the WebContents and send the user somewhere so that the InfoBar gets removed.
        InfoBarTestAnimationListener removeListener = new InfoBarTestAnimationListener();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getInfoBarContainer().addAnimationListener(removeListener));
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            WebContents newContents = WebContentsFactory.createWebContents(
                    Profile.getLastUsedRegularProfile(), false);
            TabTestUtils.swapWebContents(
                    sActivityTestRule.getActivity().getActivityTab(), newContents, false, false);
        });
        sActivityTestRule.loadUrl(HELLO_WORLD_URL);
        removeListener.removeInfoBarAnimationFinished("InfoBar not removed.");
        Assert.assertEquals("Wrong infobar count", 0, sActivityTestRule.getInfoBars().size());

        // Revisiting the original page should make the InfoBar reappear.
        InfoBarTestAnimationListener addListener = new InfoBarTestAnimationListener();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getInfoBarContainer().addAnimationListener(addListener));
        sActivityTestRule.loadUrl(sTestServer.getURL(POPUP_PAGE));
        addListener.addInfoBarAnimationFinished("InfoBar not added");
        Assert.assertEquals("Wrong infobar count", 1, sActivityTestRule.getInfoBars().size());
    }
}
