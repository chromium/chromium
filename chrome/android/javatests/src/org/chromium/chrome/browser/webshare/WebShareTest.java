// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

/** Test suite for Web Share (navigator.share) functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Device(DeviceFormFactor.TABLET) // crbug.com/41486142
public class WebShareTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TEST_FILE = "/content/test/data/android/webshare.html";
    private static final String TEST_FILE_APK = "/content/test/data/android/webshare-apk.html";
    private static final String TEST_FILE_DEX = "/content/test/data/android/webshare-dex.html";
    private static final String TEST_FILE_MANY = "/content/test/data/android/webshare-many.html";
    private static final String TEST_FILE_LARGE = "/content/test/data/android/webshare-large.html";
    private static final String TEST_FILE_SEPARATOR =
            "/content/test/data/android/webshare-separator.html";
    private static final String TEST_LONG_TEXT = "/content/test/data/android/webshare-long.html";

    private EmbeddedTestServer mTestServer;

    private Tab mTab;
    private WebShareUpdateWaiter mUpdateWaiter;

    private Intent mReceivedIntent;

    /** Waits until the JavaScript code supplies a result. */
    private static class WebShareUpdateWaiter extends EmptyTabObserver {
        private final CallbackHelper mCallbackHelper;
        private String mStatus;

        public WebShareUpdateWaiter() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String title = sActivityTestRule.getActivity().getActivityTab().getTitle();
            // Wait until the title indicates either success or failure.
            if (!title.equals("Success") && !title.startsWith("Fail:")) return;
            mStatus = title;
            mCallbackHelper.notifyCalled();
        }

        public String waitForUpdate() throws Exception {
            mCallbackHelper.waitForCallback(0);
            return mStatus;
        }
    }

    @Before
    public void setUp() throws Exception {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        mTab = sActivityTestRule.getActivity().getActivityTab();
        mUpdateWaiter = new WebShareUpdateWaiter();
        ThreadUtils.runOnUiThreadBlocking(() -> mTab.addObserver(mUpdateWaiter));

        mReceivedIntent = null;
    }

    @After
    public void tearDown() {
        if (mTab != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mTab.removeObserver(mUpdateWaiter));
        }
    }

    /** Verify that WebShare fails if called without a user gesture. */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareNoUserGesture() throws Exception {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE));
        sActivityTestRule.runJavaScriptCodeInCurrentTab("initiate_share()");
        Assert.assertEquals(
                "Fail: NotAllowedError: Failed to execute 'share' on 'Navigator': "
                        + "Must be handling a user gesture to perform a share request.",
                mUpdateWaiter.waitForUpdate());
    }

    /** Verify WebShare fails if share of .apk is called from a user gesture. */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareApk() throws Exception {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_APK));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: Permission denied", mUpdateWaiter.waitForUpdate());
    }

    /** Verify WebShare fails if share of .dex is called from a user gesture. */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareDex() throws Exception {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_DEX));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: Permission denied", mUpdateWaiter.waitForUpdate());
    }

    /** Verify WebShare fails if share of many files is called from a user gesture. */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareMany() throws Exception {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_MANY));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: "
                        + "Failed to execute 'share' on 'Navigator': Permission denied",
                mUpdateWaiter.waitForUpdate());
    }

    /** Verify WebShare fails if share of large files is called from a user gesture. */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareLarge() throws Exception {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_LARGE));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: "
                        + "Failed to execute 'share' on 'Navigator': Permission denied",
                mUpdateWaiter.waitForUpdate());
    }

    /** Verify WebShare fails if share of long text is called from a user gesture. */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareLongText() throws Exception {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_LONG_TEXT));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: "
                        + "Failed to execute 'share' on 'Navigator': Permission denied",
                mUpdateWaiter.waitForUpdate());
    }

    /** Verify WebShare fails if share of file name '/' is called from a user gesture. */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareSeparator() throws Exception {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_SEPARATOR));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: "
                        + "Failed to execute 'share' on 'Navigator': Permission denied",
                mUpdateWaiter.waitForUpdate());
    }
}
