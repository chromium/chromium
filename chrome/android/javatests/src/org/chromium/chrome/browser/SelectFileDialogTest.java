// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;

import androidx.annotation.RequiresApi;
import androidx.core.content.ContextCompat;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.TestContentProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.SelectFileDialog;

import java.io.File;

/** Integration test for select file dialog used for <input type="file" /> */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SelectFileDialogTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String DATA_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<html><head><meta name=\"viewport\"content=\"width=device-width,"
                        + " initial-scale=2.0, maximum-scale=2.0\" /></head><body><form"
                        + " action=\"about:blank\"><input id=\"input_file\" type=\"file\""
                        + " /><br/><input id=\"input_text\" type=\"file\" accept=\"text/plain\""
                        + " /><br/><input id=\"input_any\" type=\"file\" accept=\"*/*\""
                        + " /><br/><input id=\"input_file_multiple\" type=\"file\" multiple /><br"
                        + " /><input id=\"input_image\" type=\"file\" accept=\"image/*\" capture"
                        + " /><br/><input id=\"input_audio\" type=\"file\" accept=\"audio/*\""
                        + " capture /></form></body></html>");

    private static class ActivityWindowAndroidForTest extends ActivityWindowAndroid {
        public Intent lastIntent;
        public IntentCallback lastCallback;

        public ActivityWindowAndroidForTest(Activity activity) {
            super(
                    activity,
                    /* listenToActivityState= */ true,
                    IntentRequestTracker.createFromActivity(activity));
        }

        @Override
        public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
            lastIntent = intent;
            lastCallback = callback;
            return 1;
        }

        @Override
        public boolean canResolveActivity(Intent intent) {
            return true;
        }
    }

    private void verifyIntentSent() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "SelectFileDialog never sent an intent.",
                            mActivityWindowAndroidForTest.lastIntent,
                            Matchers.notNullValue());
                });
    }

    private WebContents mWebContents;
    private ActivityWindowAndroidForTest mActivityWindowAndroidForTest;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(DATA_URL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityWindowAndroidForTest =
                            new ActivityWindowAndroidForTest(mActivityTestRule.getActivity());
                    SelectFileDialog.setWindowAndroidForTests(mActivityWindowAndroidForTest);

                    mWebContents = mActivityTestRule.getActivity().getCurrentWebContents();
                });
        DOMUtils.waitForNonZeroNodeBounds(mWebContents, "input_file");
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityWindowAndroidForTest.destroy();
                });
    }

    /** Tests that clicks on <input type="file" /> trigger intent calls to ActivityWindowAndroid. */
    @Test
    @RequiresApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    @MediumTest
    @Feature({"TextInput", "Main"})
    @DisabledTest(message = "https://crbug.com/724163")
    public void testSelectFileAndCancelRequest() throws Throwable {
        // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
        // Wait for page scale will timeout and causing the test to fail.
        mActivityTestRule.assertWaitForPageScaleFactorMatch(2);
        {
            DOMUtils.clickNode(mWebContents, "input_file");
            verifyIntentSent();
            Assert.assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent)
                            mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                                    Intent.EXTRA_INTENT);
            Assert.assertNotNull(contentIntent);
            Assert.assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            resetActivityWindowAndroidForTest();
        }

        {
            DOMUtils.clickNode(mWebContents, "input_text");
            verifyIntentSent();
            Assert.assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent)
                            mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                                    Intent.EXTRA_INTENT);
            Assert.assertNotNull(contentIntent);
            Assert.assertTrue(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            resetActivityWindowAndroidForTest();
        }

        {
            DOMUtils.clickNode(mWebContents, "input_any");
            verifyIntentSent();
            Assert.assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent)
                            mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                                    Intent.EXTRA_INTENT);
            Assert.assertNotNull(contentIntent);
            Assert.assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            resetActivityWindowAndroidForTest();
        }

        {
            DOMUtils.clickNode(mWebContents, "input_file_multiple");
            verifyIntentSent();
            Assert.assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent)
                            mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                                    Intent.EXTRA_INTENT);
            Assert.assertNotNull(contentIntent);
            Assert.assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                Assert.assertTrue(contentIntent.hasExtra(Intent.EXTRA_ALLOW_MULTIPLE));
            }
            resetActivityWindowAndroidForTest();
        }

        DOMUtils.clickNode(mWebContents, "input_image");
        verifyIntentSent();
        Assert.assertEquals(
                MediaStore.ACTION_IMAGE_CAPTURE,
                mActivityWindowAndroidForTest.lastIntent.getAction());
        resetActivityWindowAndroidForTest();

        DOMUtils.clickNode(mWebContents, "input_audio");
        verifyIntentSent();
        Assert.assertEquals(
                MediaStore.Audio.Media.RECORD_SOUND_ACTION,
                mActivityWindowAndroidForTest.lastIntent.getAction());
        resetActivityWindowAndroidForTest();
    }

    /** Tests that content URI resolving to local app dir is checked correctly. */
    @Test
    @MediumTest
    @RequiresApi(Build.VERSION_CODES.O)
    public void testIsContentUriUnderAppDir() throws Throwable {
        File dataDir = ContextCompat.getDataDir(ContextUtils.getApplicationContext());
        File childDir = new File(dataDir, "android");
        childDir.mkdirs();
        File temp = File.createTempFile("tmp", ".tmp", childDir);
        temp.deleteOnExit();
        TestContentProvider.resetResourceRequestCounts(ContextUtils.getApplicationContext());
        TestContentProvider.setDataFilePath(
                ContextUtils.getApplicationContext(), dataDir.getPath());
        Assert.assertEquals(
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.O,
                SelectFileDialog.isContentUriUnderAppDir(
                        Uri.parse(TestContentProvider.createContentUrl(temp.getName())),
                        ContextUtils.getApplicationContext()));
        temp.delete();
        childDir.delete();
    }

    private void resetActivityWindowAndroidForTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityWindowAndroidForTest.lastCallback.onIntentCompleted(
                                Activity.RESULT_CANCELED, null));
        mActivityWindowAndroidForTest.lastCallback = null;
        mActivityWindowAndroidForTest.lastIntent = null;
    }
}
