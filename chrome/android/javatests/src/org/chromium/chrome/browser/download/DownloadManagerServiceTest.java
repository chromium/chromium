// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.annotation.IntDef;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.util.Log;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.download.DownloadInfo.Builder;
import org.chromium.chrome.browser.download.DownloadManagerServiceTest.MockDownloadNotifier.MethodID;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.ConnectionType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashSet;
import java.util.Queue;
import java.util.UUID;
import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * Test for DownloadManagerService.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DownloadManagerServiceTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final int UPDATE_DELAY_FOR_TEST = 1;
    private static final int DELAY_BETWEEN_CALLS = 10;
    private static final int LONG_UPDATE_DELAY_FOR_TEST = 500;

    /**
     * The MockDownloadNotifier. Currently there is no support for creating mock objects this is a
     * simple mock object that provides testing support for checking a sequence of calls.
     */
    static class MockDownloadNotifier extends SystemDownloadNotifier2 {
        /**
         * The Ids of different methods in this mock object.
         */
        @IntDef({MethodID.DOWNLOAD_SUCCESSFUL, MethodID.DOWNLOAD_FAILED, MethodID.DOWNLOAD_PROGRESS,
                MethodID.DOWNLOAD_PAUSED, MethodID.DOWNLOAD_INTERRUPTED,
                MethodID.CANCEL_DOWNLOAD_ID, MethodID.CLEAR_PENDING_DOWNLOADS})
        @Retention(RetentionPolicy.SOURCE)
        public @interface MethodID {
            int DOWNLOAD_SUCCESSFUL = 0;
            int DOWNLOAD_FAILED = 1;
            int DOWNLOAD_PROGRESS = 2;
            int DOWNLOAD_PAUSED = 3;
            int DOWNLOAD_INTERRUPTED = 4;
            int CANCEL_DOWNLOAD_ID = 5;
            int CLEAR_PENDING_DOWNLOADS = 6;
        }

        // Use MethodID for Integer values.
        private final Queue<Pair<Integer, Object>> mExpectedCalls =
                new ConcurrentLinkedQueue<Pair<Integer, Object>>();

        public MockDownloadNotifier() {
            expect(MethodID.CLEAR_PENDING_DOWNLOADS, null);
        }

        /** @deprecated Use constructor with no arguments instead. */
        public MockDownloadNotifier(Context context) {
            this();
        }

        public MockDownloadNotifier expect(@MethodID int method, Object param) {
            mExpectedCalls.clear();
            mExpectedCalls.add(getMethodSignature(method, param));
            return this;
        }

        public void waitTillExpectedCallsComplete() {
            CriteriaHelper.pollInstrumentationThread(
                    new Criteria("Failed while waiting for all calls to complete.") {
                        @Override
                        public boolean isSatisfied() {
                            return mExpectedCalls.isEmpty();
                        }
                    });
        }

        public MockDownloadNotifier andThen(@MethodID int method, Object param) {
            mExpectedCalls.add(getMethodSignature(method, param));
            return this;
        }

        static Pair<Integer, Object> getMethodSignature(@MethodID int methodId, Object param) {
            return new Pair<Integer, Object>(methodId, param);
        }

        void assertCorrectExpectedCall(@MethodID int methodId, Object param, boolean matchParams) {
            Log.w("MockDownloadNotifier", "Called: " + methodId);
            Assert.assertFalse("Unexpected call:, no call expected, but got: " + methodId,
                    mExpectedCalls.isEmpty());
            Pair<Integer, Object> actual = getMethodSignature(methodId, param);
            Pair<Integer, Object> expected = mExpectedCalls.poll();
            Assert.assertEquals("Unexpected call", expected.first, actual.first);
            if (matchParams) {
                Assert.assertTrue(
                        "Incorrect arguments", MatchHelper.macthes(expected.second, actual.second));
            }
        }

        @Override
        public void notifyDownloadSuccessful(DownloadInfo downloadInfo,
                long systemDownloadId, boolean canResolve, boolean isSupportedMimeType) {
            assertCorrectExpectedCall(MethodID.DOWNLOAD_SUCCESSFUL, downloadInfo, false);
            Assert.assertEquals("application/unknown", downloadInfo.getMimeType());
            super.notifyDownloadSuccessful(downloadInfo, systemDownloadId, canResolve,
                    isSupportedMimeType);
        }

        @Override
        public void notifyDownloadFailed(DownloadInfo downloadInfo) {
            assertCorrectExpectedCall(MethodID.DOWNLOAD_FAILED, downloadInfo, true);

        }

        @Override
        public void notifyDownloadProgress(
                DownloadInfo downloadInfo, long startTime, boolean canDownloadWhileMetered) {
            assertCorrectExpectedCall(MethodID.DOWNLOAD_PROGRESS, downloadInfo, true);
        }

        @Override
        public void notifyDownloadPaused(DownloadInfo downloadInfo) {
            assertCorrectExpectedCall(MethodID.DOWNLOAD_PAUSED, downloadInfo, true);
        }

        @Override
        public void notifyDownloadInterrupted(DownloadInfo downloadInfo, boolean isAutoResumable,
                @PendingState int pendingState) {
            assertCorrectExpectedCall(MethodID.DOWNLOAD_INTERRUPTED, downloadInfo, true);
        }

        @Override
        public void notifyDownloadCanceled(ContentId id) {
            assertCorrectExpectedCall(MethodID.CANCEL_DOWNLOAD_ID, id, true);
        }

        @Override
        public void resumePendingDownloads() {}
    }

    /**
     * Mock implementation of the DownloadSnackbarController.
     */
    static class MockDownloadSnackbarController extends DownloadSnackbarController {
        private boolean mSucceeded;
        private boolean mFailed;

        public void waitForSnackbarControllerToFinish(final boolean success) {
            CriteriaHelper.pollInstrumentationThread(
                    new Criteria("Failed while waiting for all calls to complete.") {
                        @Override
                        public boolean isSatisfied() {
                            return success ? mSucceeded : mFailed;
                        }
                    });
        }

        @Override
        public void onDownloadSucceeded(
                DownloadInfo downloadInfo, int notificationId, long downloadId,
                boolean canBeResolved, boolean usesAndroidDownloadManager) {
            mSucceeded = true;
        }

        @Override
        public void onDownloadFailed(String errorMessage, boolean showAllDownloads) {
            mFailed = true;
        }
    }

    /**
     * A set that each object can be matched ^only^ once. Once matched, the object
     * will be removed from the set. This is useful to write expectations
     * for a sequence of calls where order of calls is not defined. Client can
     * do the following. OneTimeMatchSet matchSet = new OneTimeMatchSet(possibleValue1,
     * possibleValue2, possibleValue3); mockObject.expect(method1, matchSet).andThen(method1,
     * matchSet).andThen(method3, matchSet); .... Some work.
     * mockObject.waitTillExpectedCallsComplete(); assertTrue(matchSet.mMatches.empty());
     */
    private static class OneTimeMatchSet {
        private final HashSet<Object> mMatches;

        OneTimeMatchSet(Object... params) {
            mMatches = new HashSet<Object>();
            Collections.addAll(mMatches, params);
        }

        public boolean matches(Object obj) {
            if (obj == null) return false;
            if (this == obj) return true;
            if (!mMatches.contains(obj)) return false;

            // Remove the object since it has been matched.
            mMatches.remove(obj);
            return true;
        }
    }

    /**
     * Class that helps matching 2 objects with either of them may be a OneTimeMatchSet object.
     */
    private static class MatchHelper {
        public static boolean macthes(Object obj1, Object obj2) {
            if (obj1 == null) return obj2 == null;
            if (obj1.equals(obj2)) return true;
            if (obj1 instanceof OneTimeMatchSet) {
                return ((OneTimeMatchSet) obj1).matches(obj2);
            } else if (obj2 instanceof OneTimeMatchSet) {
                return ((OneTimeMatchSet) obj2).matches(obj1);
            }
            return false;
        }
    }

    private static class DownloadManagerServiceForTest extends DownloadManagerService {
        boolean mResumed;
        DownloadSnackbarController mDownloadSnackbarController;

        public DownloadManagerServiceForTest(
                MockDownloadNotifier mockNotifier, long updateDelayInMillis) {
            super(mockNotifier, getTestHandler(), updateDelayInMillis);
        }

        @Override
        protected boolean addCompletedDownload(DownloadItem downloadItem) {
            downloadItem.setSystemDownloadId(1L);
            return true;
        }

        @Override
        protected void init() {}

        @Override
        public void resumeDownload(ContentId id, DownloadItem item, boolean hasUserGesture) {
            mResumed = true;
        }

        @Override
        protected void scheduleUpdateIfNeeded() {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> DownloadManagerServiceForTest.super.scheduleUpdateIfNeeded());
        }

        @Override
        protected void setDownloadSnackbarController(
                DownloadSnackbarController downloadSnackbarController) {
            mDownloadSnackbarController = downloadSnackbarController;
            super.setDownloadSnackbarController(downloadSnackbarController);
        }

        @Override
        protected void onDownloadFailed(DownloadItem downloadItem, int reason) {
            mDownloadSnackbarController.onDownloadFailed("", false);
        }
    }

    private DownloadManagerServiceForTest mService;

    @Before
    public void setUp() throws Exception {
        RecordHistogram.setDisabledForTests(true);
    }

    @After
    public void tearDown() throws Exception {
        mService = null;
        RecordHistogram.setDisabledForTests(false);
    }

    private static Handler getTestHandler() {
        HandlerThread handlerThread = new HandlerThread("handlerThread");
        handlerThread.start();
        return new Handler(handlerThread.getLooper());
    }

    private DownloadInfo getDownloadInfo() {
        return new Builder()
                .setBytesReceived(100)
                .setDownloadGuid(UUID.randomUUID().toString())
                .build();
    }

    private void createDownloadManagerService(MockDownloadNotifier notifier, int delayForTest) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mService = new DownloadManagerServiceForTest(notifier, delayForTest);
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"Download"})
    @RetryOnFailure
    public void testAllDownloadProgressIsCalledForSlowUpdates() throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        createDownloadManagerService(notifier, UPDATE_DELAY_FOR_TEST);
        DownloadInfo downloadInfo = getDownloadInfo();

        notifier.expect(MethodID.DOWNLOAD_PROGRESS, downloadInfo);
        mService.onDownloadUpdated(downloadInfo);
        notifier.waitTillExpectedCallsComplete();

        // Now post multiple download updated calls and make sure all are received.
        DownloadInfo update1 =
                Builder.fromDownloadInfo(downloadInfo)
                        .setProgress(new Progress(10, 100L, OfflineItemProgressUnit.PERCENTAGE))
                        .build();
        DownloadInfo update2 =
                Builder.fromDownloadInfo(downloadInfo)
                        .setProgress(new Progress(30, 100L, OfflineItemProgressUnit.PERCENTAGE))
                        .build();
        DownloadInfo update3 =
                Builder.fromDownloadInfo(downloadInfo)
                        .setProgress(new Progress(30, 100L, OfflineItemProgressUnit.PERCENTAGE))
                        .build();
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, update1)
                .andThen(MethodID.DOWNLOAD_PROGRESS, update2)
                .andThen(MethodID.DOWNLOAD_PROGRESS, update3);

        mService.onDownloadUpdated(update1);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        mService.onDownloadUpdated(update2);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        mService.onDownloadUpdated(update3);
        notifier.waitTillExpectedCallsComplete();
    }

    @Test
    @MediumTest
    @Feature({"Download"})
    public void testOnlyTwoProgressForFastUpdates() throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        createDownloadManagerService(notifier, LONG_UPDATE_DELAY_FOR_TEST);
        DownloadInfo downloadInfo = getDownloadInfo();
        DownloadInfo update1 =
                Builder.fromDownloadInfo(downloadInfo)
                        .setProgress(new Progress(10, 100L, OfflineItemProgressUnit.PERCENTAGE))
                        .build();
        DownloadInfo update2 =
                Builder.fromDownloadInfo(downloadInfo)
                        .setProgress(new Progress(10, 100L, OfflineItemProgressUnit.PERCENTAGE))
                        .build();
        DownloadInfo update3 =
                Builder.fromDownloadInfo(downloadInfo)
                        .setProgress(new Progress(10, 100L, OfflineItemProgressUnit.PERCENTAGE))
                        .build();

        // Should get 2 update calls, the first and the last. The 2nd update will be merged into
        // the last one.
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, update1)
                .andThen(MethodID.DOWNLOAD_PROGRESS, update3);
        mService.onDownloadUpdated(update1);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        mService.onDownloadUpdated(update2);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        mService.onDownloadUpdated(update3);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        notifier.waitTillExpectedCallsComplete();
    }

    @Test
    @MediumTest
    @Feature({"Download"})
    @RetryOnFailure
    public void testDownloadCompletedIsCalled() throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        MockDownloadSnackbarController snackbarController = new MockDownloadSnackbarController();
        createDownloadManagerService(notifier, UPDATE_DELAY_FOR_TEST);
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> DownloadManagerService.setDownloadManagerService(mService));
        mService.setDownloadSnackbarController(snackbarController);
        // Try calling download completed directly.
        DownloadInfo successful = getDownloadInfo();
        notifier.expect(MethodID.DOWNLOAD_SUCCESSFUL, successful);

        mService.onDownloadCompleted(successful);
        notifier.waitTillExpectedCallsComplete();
        snackbarController.waitForSnackbarControllerToFinish(true);

        // Now check that a successful notification appears after a download progress.
        DownloadInfo progress = getDownloadInfo();
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, progress)
                .andThen(MethodID.DOWNLOAD_SUCCESSFUL, progress);
        mService.onDownloadUpdated(progress);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        mService.onDownloadCompleted(progress);
        notifier.waitTillExpectedCallsComplete();
        snackbarController.waitForSnackbarControllerToFinish(true);
    }

    @Test
    @MediumTest
    @Feature({"Download"})
    public void testDownloadFailedIsCalled() throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        MockDownloadSnackbarController snackbarController = new MockDownloadSnackbarController();
        createDownloadManagerService(notifier, UPDATE_DELAY_FOR_TEST);
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> DownloadManagerService.setDownloadManagerService(mService));
        mService.setDownloadSnackbarController(snackbarController);
        // Check that if an interrupted download cannot be resumed, it will trigger a download
        // failure.
        DownloadInfo failure =
                Builder.fromDownloadInfo(getDownloadInfo()).setIsResumable(false).build();
        notifier.expect(MethodID.DOWNLOAD_FAILED, failure);
        mService.onDownloadInterrupted(failure, false);
        notifier.waitTillExpectedCallsComplete();
        snackbarController.waitForSnackbarControllerToFinish(false);
    }

    @Test
    @MediumTest
    @Feature({"Download"})
    public void testDownloadPausedIsCalled() throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        createDownloadManagerService(notifier, UPDATE_DELAY_FOR_TEST);
        DownloadManagerService.disableNetworkListenerForTest();
        DownloadInfo interrupted =
                Builder.fromDownloadInfo(getDownloadInfo()).setIsResumable(true).build();
        notifier.expect(MethodID.DOWNLOAD_INTERRUPTED, interrupted);
        mService.onDownloadInterrupted(interrupted, true);
        notifier.waitTillExpectedCallsComplete();
    }

    @Test
    @MediumTest
    @Feature({"Download"})
    @RetryOnFailure
    public void testMultipleDownloadProgress() {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        createDownloadManagerService(notifier, UPDATE_DELAY_FOR_TEST);

        DownloadInfo download1 = getDownloadInfo();
        DownloadInfo download2 = getDownloadInfo();
        DownloadInfo download3 = getDownloadInfo();
        OneTimeMatchSet matchSet = new OneTimeMatchSet(download1, download2, download3);
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, matchSet)
                .andThen(MethodID.DOWNLOAD_PROGRESS, matchSet)
                .andThen(MethodID.DOWNLOAD_PROGRESS, matchSet);
        mService.onDownloadUpdated(download1);
        mService.onDownloadUpdated(download2);
        mService.onDownloadUpdated(download3);

        notifier.waitTillExpectedCallsComplete();
        Assert.assertTrue("All downloads should be updated.", matchSet.mMatches.isEmpty());
    }

    @Test
    @MediumTest
    @Feature({"Download"})
    @RetryOnFailure
    public void testInterruptedDownloadAreAutoResumed() throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        createDownloadManagerService(notifier, UPDATE_DELAY_FOR_TEST);
        DownloadManagerService.disableNetworkListenerForTest();
        DownloadInfo interrupted =
                Builder.fromDownloadInfo(getDownloadInfo()).setIsResumable(true).build();
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, interrupted)
                .andThen(MethodID.DOWNLOAD_INTERRUPTED, interrupted);
        mService.onDownloadUpdated(interrupted);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        mService.onDownloadInterrupted(interrupted, true);
        notifier.waitTillExpectedCallsComplete();
        int resumableIdCount = mService.mAutoResumableDownloadIds.size();
        mService.onConnectionTypeChanged(ConnectionType.CONNECTION_WIFI);
        Assert.assertEquals(resumableIdCount - 1, mService.mAutoResumableDownloadIds.size());
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mService.mResumed;
            }
        });
    }

    @Test
    //@MediumTest
    //@Feature({"Download"})
    @DisabledTest // crbug.com/789931
    public void testInterruptedUnmeteredDownloadCannotAutoResumeOnMeteredNetwork()
            throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        createDownloadManagerService(notifier, UPDATE_DELAY_FOR_TEST);
        DownloadManagerService.disableNetworkListenerForTest();
        DownloadInfo interrupted =
                Builder.fromDownloadInfo(getDownloadInfo()).setIsResumable(true).build();
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, interrupted)
                .andThen(MethodID.DOWNLOAD_INTERRUPTED, interrupted);
        mService.onDownloadUpdated(interrupted);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        mService.onDownloadInterrupted(interrupted, true);
        notifier.waitTillExpectedCallsComplete();
        DownloadManagerService.setIsNetworkMeteredForTest(true);
        int resumableIdCount = mService.mAutoResumableDownloadIds.size();
        mService.onConnectionTypeChanged(ConnectionType.CONNECTION_2G);
        Assert.assertEquals(resumableIdCount, mService.mAutoResumableDownloadIds.size());
    }

    /**
     * Test to make sure {@link DownloadManagerService#shouldOpenAfterDownload}
     * returns the right result for varying MIME types and Content-Dispositions.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testShouldOpenAfterDownload() {
        // Should not open any download type MIME types.
        Assert.assertFalse(DownloadManagerService.shouldOpenAfterDownload(
                new DownloadInfo.Builder()
                        .setMimeType("application/download")
                        .setHasUserGesture(true)
                        .build()));
        Assert.assertFalse(DownloadManagerService.shouldOpenAfterDownload(
                new DownloadInfo.Builder()
                        .setMimeType("application/x-download")
                        .setHasUserGesture(true)
                        .build()));
        Assert.assertFalse(DownloadManagerService.shouldOpenAfterDownload(
                new DownloadInfo.Builder()
                        .setMimeType("application/octet-stream")
                        .setHasUserGesture(true)
                        .build()));

        // Should open PDFs.
        Assert.assertTrue(DownloadManagerService.shouldOpenAfterDownload(
                new DownloadInfo.Builder()
                        .setMimeType("application/pdf")
                        .setHasUserGesture(true)
                        .build()));
        Assert.assertTrue(DownloadManagerService.shouldOpenAfterDownload(
                new DownloadInfo.Builder()
                        .setContentDisposition("filename=test.pdf")
                        .setMimeType("application/pdf")
                        .setHasUserGesture(true)
                        .build()));

        // Require user gesture.
        Assert.assertFalse(DownloadManagerService.shouldOpenAfterDownload(
                new DownloadInfo.Builder()
                        .setMimeType("application/pdf")
                        .setHasUserGesture(false)
                        .build()));
        Assert.assertFalse(DownloadManagerService.shouldOpenAfterDownload(
                new DownloadInfo.Builder()
                        .setContentDisposition("filename=test.pdf")
                        .setMimeType("application/pdf")
                        .setHasUserGesture(false)
                        .build()));
    }
}
