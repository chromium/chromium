// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.clipboard;

import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Looper;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileProviderUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.share.ClipboardImageFileProvider;
import org.chromium.ui.base.Clipboard;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests of {@link ClipboardImageFileProvider}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ClipboardImageFileProviderTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final long WAIT_TIMEOUT_SECONDS = 30L;
    private static final String TEST_PNG_IMAGE_FILE_EXTENSION = ".png";

    private byte[] mTestImageData;

    private static class AsyncTaskRunnableHelper extends CallbackHelper implements Runnable {
        @Override
        public void run() {
            notifyCalled();
        }
    }

    private void waitForAsync() throws TimeoutException {
        try {
            AsyncTaskRunnableHelper runnableHelper = new AsyncTaskRunnableHelper();
            AsyncTask.SERIAL_EXECUTOR.execute(runnableHelper);
            runnableHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (TimeoutException ex) {
        }
    }

    @Before
    public void setUp() {
        Looper.prepare();
        // Clear the clipboard.
        Clipboard.getInstance().setText("");

        Bitmap bitmap =
                Bitmap.createBitmap(/* width= */ 10, /* height= */ 10, Bitmap.Config.ARGB_8888);
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, /*quality = (0-100) */ 100, baos);
        mTestImageData = baos.toByteArray();

        mActivityTestRule.startMainActivityFromLauncher();
        FileProviderUtils.setFileProviderUtil(new FileProviderHelper());
    }

    @After
    public void tearDown() throws TimeoutException {
        // Clear the clipboard.
        Clipboard.getInstance().setText("");
    }

    @Test
    @SmallTest
    public void testClipboardSetImage() throws TimeoutException, IOException {
        Clipboard.getInstance().setImageFileProvider(new ClipboardImageFileProvider());
        Clipboard.getInstance().setImage(mTestImageData, TEST_PNG_IMAGE_FILE_EXTENSION);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            Clipboard.getInstance().getImageUri(), Matchers.notNullValue());
                    Criteria.checkThat(
                            Clipboard.getInstance().getImageUriIfSharedByThisApp(),
                            Matchers.is(Clipboard.getInstance().getImageUri()));
                });

        Uri uri = Clipboard.getInstance().getImageUri();
        Assert.assertNotNull(uri);
        Bitmap bitmap =
                ApiCompatibilityUtils.getBitmapByUri(
                        ContextUtils.getApplicationContext().getContentResolver(), uri);
        Assert.assertNotNull(bitmap);

        // Wait for the above check to complete.
        waitForAsync();
    }
}
