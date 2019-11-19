// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.app.AlertDialog;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.InputStream;
import java.util.ArrayList;

/** Test suite for Web Share (navigator.share) functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebShareTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    private static final String TEST_FILE = "/content/test/data/android/webshare.html";
    private static final String TEST_FILE_APK = "/content/test/data/android/webshare-apk.html";
    private static final String TEST_FILE_BMP = "/content/test/data/android/webshare-bmp.html";
    private static final String TEST_FILE_CSV = "/content/test/data/android/webshare-csv.html";
    private static final String TEST_FILE_DEX = "/content/test/data/android/webshare-dex.html";
    private static final String TEST_FILE_OGG = "/content/test/data/android/webshare-ogg.html";
    private static final String TEST_FILE_MANY = "/content/test/data/android/webshare-many.html";
    private static final String TEST_FILE_LARGE = "/content/test/data/android/webshare-large.html";

    private EmbeddedTestServer mTestServer;

    private Tab mTab;
    private WebShareUpdateWaiter mUpdateWaiter;

    private Intent mReceivedIntent;

    /** Waits until the JavaScript code supplies a result. */
    private class WebShareUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private String mStatus;

        public WebShareUpdateWaiter() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String title = mActivityTestRule.getActivity().getActivityTab().getTitle();
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
        mActivityTestRule.startMainActivityOnBlankPage();
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        mTab = mActivityTestRule.getActivity().getActivityTab();
        mUpdateWaiter = new WebShareUpdateWaiter();
        mTab.addObserver(mUpdateWaiter);

        mReceivedIntent = null;
    }

    @After
    public void tearDown() {
        if (mTab != null) mTab.removeObserver(mUpdateWaiter);
        if (mTestServer != null) mTestServer.stopAndDestroyServer();

        // Clean up some state that might have been changed by tests.
        ShareHelper.setForceCustomChooserForTesting(false);
        ShareHelper.setFakeIntentReceiverForTesting(null);
    }

    /**
     * Verify that WebShare fails if called without a user gesture.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareNoUserGesture() throws Exception {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE));
        mActivityTestRule.runJavaScriptCodeInCurrentTab("initiate_share()");
        Assert.assertEquals("Fail: NotAllowedError: "
                        + "Must be handling a user gesture to perform a share request.",
                mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare fails if share is called from a user gesture, and canceled.
     * This test tests functionality that is only available post Lollipop MR1.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    public void testWebShareCancel() throws Exception {
        // Set up ShareHelper to ignore the intent (without showing a picker). This simulates the
        // user canceling the dialog.
        ShareHelper.setFakeIntentReceiverForTesting(new FakeIntentReceiverPostLMR1(false));

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals("Fail: AbortError: Share canceled", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare succeeds if share is called from a user gesture, and app chosen.
     * This test tests functionality that is only available post Lollipop MR1.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    public void testWebShareSuccess() throws Exception {
        // Set up ShareHelper to immediately succeed (without showing a picker).
        ShareHelper.setFakeIntentReceiverForTesting(new FakeIntentReceiverPostLMR1(true));

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());

        // The actual intent to be delivered to the target is in the EXTRA_INTENT of the chooser
        // intent.
        Assert.assertNotNull(mReceivedIntent);
        Assert.assertTrue(mReceivedIntent.hasExtra(Intent.EXTRA_INTENT));
        verifyDeliveredIntent(mReceivedIntent.getParcelableExtra(Intent.EXTRA_INTENT));
    }

    /**
     * Verify WebShare of .ogg file succeeds if share is called from a user gesture, and app chosen.
     * This test tests functionality that is only available post Lollipop MR1.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    public void testWebShareOgg() throws Exception {
        // Set up ShareHelper to immediately succeed (without showing a picker).
        ShareHelper.setFakeIntentReceiverForTesting(new FakeIntentReceiverPostLMR1(true));

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_OGG));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());

        // The actual intent to be delivered to the target is in the EXTRA_INTENT of the chooser
        // intent.
        Assert.assertNotNull(mReceivedIntent);
        Assert.assertTrue(mReceivedIntent.hasExtra(Intent.EXTRA_INTENT));
        verifyDeliveredOggIntent(mReceivedIntent.getParcelableExtra(Intent.EXTRA_INTENT));
    }

    /**
     * Verify WebShare of .bmp files succeeds if share is called from a user gesture, and app
     * chosen. This test tests functionality that is only available post Lollipop MR1.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    public void testWebShareBmp() throws Exception {
        // Set up ShareHelper to immediately succeed (without showing a picker).
        ShareHelper.setFakeIntentReceiverForTesting(new FakeIntentReceiverPostLMR1(true));

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_BMP));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());

        // The actual intent to be delivered to the target is in the EXTRA_INTENT of the chooser
        // intent.
        Assert.assertNotNull(mReceivedIntent);
        Assert.assertTrue(mReceivedIntent.hasExtra(Intent.EXTRA_INTENT));
        verifyDeliveredBmpIntent(mReceivedIntent.getParcelableExtra(Intent.EXTRA_INTENT));
    }

    /**
     * Verify WebShare fails if share of .apk is called from a user gesture.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareApk() throws Exception {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_APK));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: Permission denied", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare fails if share of .dex is called from a user gesture.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareDex() throws Exception {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_DEX));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: Permission denied", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare fails if share of many files is called from a user gesture.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareMany() throws Exception {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_MANY));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: Permission denied", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare fails if share of large files is called from a user gesture.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareLarge() throws Exception {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_LARGE));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals(
                "Fail: NotAllowedError: Permission denied", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare fails if share is called from a user gesture, and canceled.
     *
     * Simulates pre-Lollipop-LMR1 system (different intent picker).
     *
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareCancelPreLMR1() throws Exception {
        ShareHelper.setFakeIntentReceiverForTesting(new FakeIntentReceiverPreLMR1(false));

        ShareHelper.setForceCustomChooserForTesting(true);

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals("Fail: AbortError: Share canceled", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify WebShare succeeds if share is called from a user gesture, and app chosen.
     *
     * Simulates pre-Lollipop-LMR1 system (different intent picker).
     *
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareSuccessPreLMR1() throws Exception {
        ShareHelper.setFakeIntentReceiverForTesting(new FakeIntentReceiverPreLMR1(true));

        ShareHelper.setForceCustomChooserForTesting(true);
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
        verifyDeliveredIntent(mReceivedIntent);
    }

    /**
     * Verify WebShare of .ogg succeeds if share is called from a user gesture, and app chosen.
     *
     * Simulates pre-Lollipop-LMR1 system (different intent picker).
     *
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareOggPreLMR1() throws Exception {
        ShareHelper.setFakeIntentReceiverForTesting(new FakeIntentReceiverPreLMR1(true));

        ShareHelper.setForceCustomChooserForTesting(true);
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_OGG));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
        verifyDeliveredOggIntent(mReceivedIntent);
    }

    /**
     * Verify WebShare of .csv files succeeds if share is called from a user gesture, and app
     * chosen.
     *
     * Simulates pre-Lollipop-LMR1 system (different intent picker).
     *
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"WebShare"})
    public void testWebShareCsvPreLMR1() throws Exception {
        ShareHelper.setFakeIntentReceiverForTesting(new FakeIntentReceiverPreLMR1(true));

        ShareHelper.setForceCustomChooserForTesting(true);
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_CSV));
        // Click (instead of directly calling the JavaScript function) to simulate a user gesture.
        TouchCommon.singleClickView(mTab.getView());
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
        verifyDeliveredCsvIntent(mReceivedIntent);
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
                InstrumentationRegistry.getContext().getContentResolver().openInputStream(fileUri);
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
        Assert.assertEquals(Intent.FLAG_GRANT_READ_URI_PERMISSION,
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
        Assert.assertEquals(Intent.FLAG_GRANT_READ_URI_PERMISSION,
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
        Assert.assertEquals(Intent.FLAG_GRANT_READ_URI_PERMISSION,
                intent.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION);
        Uri fileUri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
        Assert.assertEquals("contents", getFileContents(fileUri));
    }

    // Uses intent picker functionality that is only available since Lollipop MR1.
    private class FakeIntentReceiverPostLMR1 implements ShareHelper.FakeIntentReceiver {
        private final boolean mProceed;
        private Intent mIntentToSendBack;

        FakeIntentReceiverPostLMR1(boolean proceed) {
            Assert.assertTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1);
            mProceed = proceed;
        }

        @Override
        public void setIntentToSendBack(Intent intent) {
            mIntentToSendBack = intent;
        }

        @Override
        public void onCustomChooserShown(AlertDialog dialog) {}

        @Override
        public void fireIntent(Context context, Intent intent) {
            mReceivedIntent = intent;

            if (!mProceed) {
                // Click again to start another share, which cancels the current share.
                // This is necessary to work around https://crbug.com/636274 (callback
                // is not canceled until next share is initiated).
                // This also serves as a regression test for https://crbug.com/640324.
                TouchCommon.singleClickView(mTab.getView());
                return;
            }

            if (context == null) return;

            // Send the intent back, which indicates that the user made a choice. (Normally,
            // this would have EXTRA_CHOSEN_COMPONENT set, but for the test, we do not set any
            // chosen target app.)
            context.sendBroadcast(mIntentToSendBack);
        }
    }

    // Uses intent picker functionality that is available before Lollipop MR1.
    private class FakeIntentReceiverPreLMR1 implements ShareHelper.FakeIntentReceiver {
        private final boolean mProceed;

        FakeIntentReceiverPreLMR1(boolean proceed) {
            mProceed = proceed;
        }

        @Override
        public void setIntentToSendBack(Intent intent) {}

        @Override
        public void onCustomChooserShown(AlertDialog dialog) {
            if (!mProceed) {
                // Cancel the chooser dialog.
                dialog.dismiss();
                return;
            }

            // Click on an app (it doesn't matter which, because we will hook the intent).
            Assert.assertTrue(dialog.getListView().getCount() > 0);
            dialog.getListView().performItemClick(
                    null, 0, dialog.getListView().getItemIdAtPosition(0));
        }

        @Override
        public void fireIntent(Context context, Intent intent) {
            mReceivedIntent = intent;
        }
    }
}
