// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thumbnail.generator;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.thumbnail.generator.ThumbnailProvider.ThumbnailRequest;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;

/** Instrumentation test for {@link ThumbnailProviderImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ThumbnailProviderImplTest {
    private static final String TEST_DIRECTORY =
            "chrome/browser/thumbnail/generator/test/data/android/";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static ThumbnailProviderImpl sThumbnailProvider;
    private static DiscardableReferencePool sReferencePool;

    @BeforeClass
    public static void setUp() throws Exception {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    sReferencePool = new DiscardableReferencePool();
                    sThumbnailProvider =
                            new ThumbnailProviderImpl(
                                    sReferencePool,
                                    ThumbnailProviderImpl.ClientType.NTP_SUGGESTIONS);
                });
    }

    @AfterClass
    public static void tearDown() {
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, sThumbnailProvider::destroy);
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testBothThumbnailSidesSmallerThanRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_10x10.jpg");
        final int requiredSize = 20;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request =
                new TestThumbnailRequest(
                        testFilePath, requiredSize, thumbnailRetrievedCallbackHelper, "a");

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    sThumbnailProvider.getThumbnail(request);
                });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(10, 10, request);
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testBothThumbnailSidesEqualToRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_10x10.jpg");
        final int requiredSize = 10;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request =
                new TestThumbnailRequest(
                        testFilePath, requiredSize, thumbnailRetrievedCallbackHelper, "b");

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    sThumbnailProvider.getThumbnail(request);
                });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(requiredSize, requiredSize, request);
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testBothThumbnailSidesBiggerThanRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_20x20.jpg");
        final int requiredSize = 10;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request =
                new TestThumbnailRequest(
                        testFilePath, requiredSize, thumbnailRetrievedCallbackHelper, "c");

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    sThumbnailProvider.getThumbnail(request);
                });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(requiredSize, requiredSize, request);
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testThumbnailWidthEqualToRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_10x20.jpg");
        final int requiredSize = 10;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request =
                new TestThumbnailRequest(
                        testFilePath, requiredSize, thumbnailRetrievedCallbackHelper, "d");

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    sThumbnailProvider.getThumbnail(request);
                });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(requiredSize, 20, request);
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testThumbnailHeightEqualToRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_20x10.jpg");
        final int requiredSize = 10;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request =
                new TestThumbnailRequest(
                        testFilePath, requiredSize, thumbnailRetrievedCallbackHelper, "e");

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    sThumbnailProvider.getThumbnail(request);
                });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(20, requiredSize, request);
    }

    private void checkResult(
            int expectedWidth, int expectedHeight, final TestThumbnailRequest request) {
        Assert.assertEquals(expectedWidth, request.getRetrievedThumbnail().getWidth());
        Assert.assertEquals(expectedHeight, request.getRetrievedThumbnail().getHeight());
    }

    private static class TestThumbnailRequest implements ThumbnailRequest {
        private final String mTestFilePath;
        private final int mRequiredSize;
        private Bitmap mRetrievedThumbnail;
        private CallbackHelper mThumbnailRetrievedCallbackHelper;
        private String mContentId;

        TestThumbnailRequest(
                String filepath,
                int requiredSize,
                CallbackHelper thumbnailRetrievedCallbackHelperHelper,
                String contentId) {
            mTestFilePath = filepath;
            mRequiredSize = requiredSize;
            mThumbnailRetrievedCallbackHelper = thumbnailRetrievedCallbackHelperHelper;
            mContentId = contentId;
        }

        @Override
        public @Nullable String getFilePath() {
            return mTestFilePath;
        }

        @Override
        public String getMimeType() {
            return null;
        }

        @Override
        public @Nullable String getContentId() {
            // Non-null and unique value for ThumbnailProviderImpl to work to ensure
            // results are not cached and reused across batched tests (which leads to erroneous
            // results)
            return mContentId;
        }

        @Override
        public void onThumbnailRetrieved(@NonNull String contentId, @Nullable Bitmap thumbnail) {
            mRetrievedThumbnail = thumbnail;
            mThumbnailRetrievedCallbackHelper.notifyCalled();
        }

        @Override
        public int getIconSize() {
            return mRequiredSize;
        }

        Bitmap getRetrievedThumbnail() {
            return mRetrievedThumbnail;
        }
    }
}
