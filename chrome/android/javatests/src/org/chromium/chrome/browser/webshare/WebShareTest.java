// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import android.content.Intent;
import android.net.Uri;

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

import java.io.InputStream;
import java.util.ArrayList;

/** Test suite for Web Share (navigator.share) functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
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
        private CallbackHelper mCallbackHelper;
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

    private static void verifyDeliveredIntent(Intent intent) {
        Assert.assertNotNull(intent);
        Assert.assertEquals(Intent.ACTION_SEND, intent.getAction());
        Assert.assertTrue(intent.hasExtra(Intent.EXTRA_SUBJECT));
        Assert.assertEquals("Test Title", intent.getStringExtra(Intent.EXTRA_SUBJECT));
        Assert.assertTrue(intent.hasExtra(Intent.EXTRA_TEXT));
        Assert.assertEquals(
                "Test Text https://test.url/", intent.getStringExtra(Intent.EXTRA_TEXT));
    }

    private static String getFileContents(Uri fileUri) throws Exception {
        InputStream inputStream =
                ApplicationProvider.getApplicationContext()
                        .getContentResolver()
                        .openInputStream(fileUri);
        byte[] buffer = new byte[1024];
        int position = 0;
        int read;
        while ((read = inputStream.read(buffer, position, buffer.length - position)) > 0) {
            position += read;
        }
        return new String(buffer, 0, position, "UTF-8");
    }

    private static void verifyDeliveredBmpIntent(Intent intent) throws Exception {
        Assert.assertNotNull(intent);
        Assert.assertEquals(Intent.ACTION_SEND_MULTIPLE, intent.getAction());
        Assert.assertEquals("image/*", intent.getType());
        Assert.assertEquals(
                Intent.FLAG_GRANT_READ_URI_PERMISSION,
                intent.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION);

        ArrayList<Uri> fileUris = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
        Assert.assertEquals(2, fileUris.size());
        Assert.assertEquals("B", getFileContents(fileUris.get(0)));
        Assert.assertEquals("MP", getFileContents(fileUris.get(1)));
    }

    private static void verifyDeliveredCsvIntent(Intent intent) throws Exception {
        Assert.assertNotNull(intent);
        Assert.assertEquals(Intent.ACTION_SEND_MULTIPLE, intent.getAction());
        Assert.assertEquals("text/*", intent.getType());
        Assert.assertEquals(
                Intent.FLAG_GRANT_READ_URI_PERMISSION,
                intent.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION);

        ArrayList<Uri> fileUris = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
        Assert.assertEquals(2, fileUris.size());
        Assert.assertEquals("1,2", getFileContents(fileUris.get(0)));
        Assert.assertEquals("1,2,3\n4,5,6\n", getFileContents(fileUris.get(1)));
    }

    private static void verifyDeliveredOggIntent(Intent intent) throws Exception {
        Assert.assertNotNull(intent);
        Assert.assertEquals(Intent.ACTION_SEND, intent.getAction());
        Assert.assertEquals("video/ogg", intent.getType());
        Assert.assertEquals(
                Intent.FLAG_GRANT_READ_URI_PERMISSION,
                intent.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION);
        Uri fileUri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
        Assert.assertEquals("contents", getFileContents(fileUri));
    }
}
