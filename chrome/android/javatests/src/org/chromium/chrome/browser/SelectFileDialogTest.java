// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.provider.MediaStore;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.SelectFileDialog;

/**
 * Integration test for select file dialog used for <input type="file" />
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SelectFileDialogTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=2.0, maximum-scale=2.0\" /></head>"
            + "<body><form action=\"about:blank\">"
            + "<input id=\"input_file\" type=\"file\" /><br/>"
            + "<input id=\"input_text\" type=\"file\" accept=\"text/plain\" /><br/>"
            + "<input id=\"input_any\" type=\"file\" accept=\"*/*\" /><br/>"
            + "<input id=\"input_file_multiple\" type=\"file\" multiple /><br />"
            + "<input id=\"input_image\" type=\"file\" accept=\"image/*\" capture /><br/>"
            + "<input id=\"input_audio\" type=\"file\" accept=\"audio/*\" capture />"
            + "</form>"
            + "</body></html>");

    private static class ActivityWindowAndroidForTest extends ActivityWindowAndroid {
        public Intent lastIntent;
        public IntentCallback lastCallback;
        /**
         * @param activity
         */
        public ActivityWindowAndroidForTest(Activity activity) {
            super(activity);
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

    private class IntentSentCriteria extends Criteria {
        public IntentSentCriteria() {
            super("SelectFileDialog never sent an intent.");
        }

        @Override
        public boolean isSatisfied() {
            return mActivityWindowAndroidForTest.lastIntent != null;
        }
    }

    private WebContents mWebContents;
    private ActivityWindowAndroidForTest mActivityWindowAndroidForTest;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(DATA_URL);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityWindowAndroidForTest =
                    new ActivityWindowAndroidForTest(mActivityTestRule.getActivity());
            SelectFileDialog.setWindowAndroidForTests(mActivityWindowAndroidForTest);

            mWebContents = mActivityTestRule.getActivity().getCurrentWebContents();
            // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
            mActivityTestRule.assertWaitForPageScaleFactorMatch(2);
        });
        DOMUtils.waitForNonZeroNodeBounds(mWebContents, "input_file");
    }

    /**
     * Tests that clicks on <input type="file" /> trigger intent calls to ActivityWindowAndroid.
     */
    @Test
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    @MediumTest
    @Feature({"TextInput", "Main"})
    @RetryOnFailure
    @DisabledTest(message = "https://crbug.com/724163")
    public void testSelectFileAndCancelRequest() throws Throwable {
        {
            DOMUtils.clickNode(mWebContents, "input_file");
            CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
            Assert.assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent) mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                            Intent.EXTRA_INTENT);
            Assert.assertNotNull(contentIntent);
            Assert.assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            resetActivityWindowAndroidForTest();
        }

        {
            DOMUtils.clickNode(mWebContents, "input_text");
            CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
            Assert.assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent) mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                            Intent.EXTRA_INTENT);
            Assert.assertNotNull(contentIntent);
            Assert.assertTrue(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            resetActivityWindowAndroidForTest();
        }

        {
            DOMUtils.clickNode(mWebContents, "input_any");
            CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
            Assert.assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent) mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                            Intent.EXTRA_INTENT);
            Assert.assertNotNull(contentIntent);
            Assert.assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            resetActivityWindowAndroidForTest();
        }

        {
            DOMUtils.clickNode(mWebContents, "input_file_multiple");
            CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
            Assert.assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent) mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                            Intent.EXTRA_INTENT);
            Assert.assertNotNull(contentIntent);
            Assert.assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                Assert.assertTrue(contentIntent.hasExtra(Intent.EXTRA_ALLOW_MULTIPLE));
            }
            resetActivityWindowAndroidForTest();
        }

        DOMUtils.clickNode(mWebContents, "input_image");
        CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
        Assert.assertEquals(MediaStore.ACTION_IMAGE_CAPTURE,
                mActivityWindowAndroidForTest.lastIntent.getAction());
        resetActivityWindowAndroidForTest();

        DOMUtils.clickNode(mWebContents, "input_audio");
        CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
        Assert.assertEquals(MediaStore.Audio.Media.RECORD_SOUND_ACTION,
                mActivityWindowAndroidForTest.lastIntent.getAction());
        resetActivityWindowAndroidForTest();
    }

    private void resetActivityWindowAndroidForTest() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityWindowAndroidForTest.lastCallback.onIntentCompleted(
                                mActivityWindowAndroidForTest, Activity.RESULT_CANCELED, null));
        mActivityWindowAndroidForTest.lastCallback = null;
        mActivityWindowAndroidForTest.lastIntent = null;
    }
}
