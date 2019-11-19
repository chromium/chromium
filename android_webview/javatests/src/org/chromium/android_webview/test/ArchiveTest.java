// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test suite for the WebView.saveWebArchive feature.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class ArchiveTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final long TEST_TIMEOUT = scaleTimeout(20000L);

    private static final String TEST_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html><head></head><body>test</body></html>");

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private AwTestContainerView mTestContainerView;

    @Before
    public void setUp() {
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
    }

    private void deleteFile(String path) {
        File file = new File(path);
        if (file.exists()) Assert.assertTrue(file.delete());
        Assert.assertFalse(file.exists());
    }

    private void doArchiveTest(final AwContents contents, final String path,
            final boolean autoName, String expectedPath) throws InterruptedException {
        if (expectedPath != null) {
            deleteFile(expectedPath);
        }

        // Set up a handler to handle the completion callback
        final Semaphore s = new Semaphore(0);
        final AtomicReference<String> msgPath = new AtomicReference<String>();
        final Callback<String> callback = path1 -> {
            msgPath.set(path1);
            s.release();
        };

        // Generate MHTML and wait for completion
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> contents.saveWebArchive(path, autoName, callback));
        Assert.assertTrue(s.tryAcquire(TEST_TIMEOUT, TimeUnit.MILLISECONDS));

        Assert.assertEquals(expectedPath, msgPath.get());
        if (expectedPath != null) {
            File file = new File(expectedPath);
            Assert.assertTrue(file.exists());
            Assert.assertTrue(file.length() > 0);
        } else {
            // A path was provided, but the expected path was null. This means the save should have
            // failed, and so there shouldn't be a file path path.
            if (path != null) {
                Assert.assertFalse(new File(path).exists());
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testExplicitGoodPath() throws Throwable {
        final String path = new File(mActivityTestRule.getActivity().getFilesDir(), "test.mht")
                                    .getAbsolutePath();
        deleteFile(path);

        mActivityTestRule.loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        doArchiveTest(mTestContainerView.getAwContents(), path, false, path);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAutoGoodPath() throws Throwable {
        final String path = mActivityTestRule.getActivity().getFilesDir().getAbsolutePath() + "/";

        mActivityTestRule.loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        // Create the first archive
        {
            String expectedPath = path + "index.mht";
            doArchiveTest(mTestContainerView.getAwContents(), path, true, expectedPath);
        }

        // Create a second archive, making sure that the second archive's name is auto incremented.
        {
            String expectedPath = path + "index-1.mht";
            doArchiveTest(mTestContainerView.getAwContents(), path, true, expectedPath);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testExplicitBadPath() throws Throwable {
        final String path = new File("/foo/bar/baz.mht").getAbsolutePath();
        deleteFile(path);

        mActivityTestRule.loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        doArchiveTest(mTestContainerView.getAwContents(), path, false, null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAutoBadPath() throws Throwable {
        final String path = new File("/foo/bar/").getAbsolutePath();
        deleteFile(path);

        mActivityTestRule.loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        doArchiveTest(mTestContainerView.getAwContents(), path, true, null);
    }

    /**
     * Ensure passing a null callback to saveWebArchive doesn't cause a crash.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullCallbackNullPath() throws Throwable {
        mActivityTestRule.loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        saveWebArchiveAndWaitForUiPost(null, false /* autoname */, null /* callback */);
    }

    /**
     * Ensure passing a null callback to saveWebArchive doesn't cause a crash.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullCallbackGoodPath() throws Throwable {
        final String path = new File(mActivityTestRule.getActivity().getFilesDir(), "test.mht")
                .getAbsolutePath();
        deleteFile(path);

        mActivityTestRule.loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        saveWebArchiveAndWaitForUiPost(path, false /* autoname */, null /* callback */);
    }

    private void saveWebArchiveAndWaitForUiPost(
            final String path, boolean autoname, final Callback<String> callback) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> mTestContainerView.getAwContents().saveWebArchive(path, false, callback));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                        // Just wait for this task to having been posted on the UI thread.
                        // This ensures that if the implementation of saveWebArchive posts a
                        // task to the UI
                        // thread we will allow that task to run before finishing our test.
                      });
    }
}
