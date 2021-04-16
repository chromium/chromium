// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemSchedule;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.UUID;

/**
 * Test class to validate that the {@link DownloadInfoBarController} correctly represents the state
 * of the downloads in the current chrome session.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(ChromeFeatureList.DOWNLOAD_PROGRESS_INFOBAR)
@Batch(Batch.PER_CLASS)
@Batch.SplitByFeature
public class DownloadInfoBarControllerTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final String MESSAGE_SPEEDING_UP = "Speeding up your download.";
    private static final String MESSAGE_DOWNLOADING_FILE = "Downloading file.";
    private static final String MESSAGE_DOWNLOADING_TWO_FILES = "Downloading 2 files.";
    private static final String MESSAGE_TWO_DOWNLOAD_COMPLETE = "2 downloads complete.";
    private static final String MESSAGE_DOWNLOAD_FAILED = "1 download failed.";
    private static final String MESSAGE_TWO_DOWNLOAD_FAILED = "2 downloads failed.";
    private static final String MESSAGE_DOWNLOAD_PENDING = "1 download pending.";
    private static final String MESSAGE_TWO_DOWNLOAD_PENDING = "2 downloads pending.";
    private static final String MESSAGE_DOWNLOAD_SCHEDULED_WIFI =
            "You’ll see a notification when this download starts on Wi-Fi.";
    private static final String MESSAGE_TWO_DOWNLOAD_SCHEDULED = "2 downloads scheduled.";

    private static final String TEST_FILE_NAME = "TestFile";
    private static final String MESSAGE_SINGLE_DOWNLOAD_COMPLETE = "TestFile.";
    private static final long TEST_TO_NEXT_STEP_DELAY = 100;

    private TestDownloadInfoBarController mTestController;

    @Before
    public void before() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTestController = new TestDownloadInfoBarController(); });
    }

    static class TestDownloadInfoBarController extends DownloadInfoBarController {
        private DownloadProgressInfoBarData mInfo;

        public TestDownloadInfoBarController() {
            super(/*otrProfileID=*/null);
        }

        @Override
        protected void showInfoBar(
                @DownloadInfoBarState int state, DownloadProgressInfoBarData info) {
            mInfo = info;
            super.showInfoBar(state, info);
        }

        @Override
        protected void closePreviousInfoBar() {
            mInfo = null;
            super.closePreviousInfoBar();
        }

        @Override
        protected long getDelayToNextStep(boolean showAccelerating, int resultState) {
            return TEST_TO_NEXT_STEP_DELAY;
        }

        @Override
        protected boolean isSpeedingUpMessageEnabled() {
            return true;
        }

        public void onItemUpdated(OfflineItem item) {
            super.onItemUpdated(item.clone(), null);
        }

        void verify(String message) {
            Assert.assertNotNull(mInfo);
            Assert.assertEquals(message, mInfo.message);
        }

        void verifyInfoBarClosed() {
            Assert.assertNull(mInfo);
        }
    }

    private static DownloadItem createDownloadItem(OfflineItem offlineItem) {
        DownloadInfo downloadInfo = DownloadInfo.fromOfflineItem(offlineItem, null);
        return new DownloadItem(false, downloadInfo);
    }

    private static OfflineItem createOfflineItem(@OfflineItemState int state) {
        OfflineItem item = new OfflineItem();
        String uuid = UUID.randomUUID().toString();
        item.id = LegacyHelpers.buildLegacyContentId(true, uuid);
        item.state = state;
        if (item.state == OfflineItemState.COMPLETE) {
            markItemComplete(item);
        }
        return item;
    }

    private static void markItemComplete(OfflineItem item) {
        item.state = OfflineItemState.COMPLETE;
        item.title = TEST_FILE_NAME;
        item.receivedBytes = 10L;
        item.totalSizeBytes = 10L;
    }

    private void waitForMessage(String message) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(mTestController.mInfo, Matchers.notNullValue());
            Criteria.checkThat(mTestController.mInfo.message, Matchers.is(message));
        });
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testOfflinePageDownloadStarted() {
        mTestController.onDownloadStarted();
        mTestController.verify(MESSAGE_DOWNLOADING_FILE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    @Features.DisableFeatures(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)
    public void testAccelerated() {
        OfflineItem offlineItem = createOfflineItem(OfflineItemState.IN_PROGRESS);
        offlineItem.isAccelerated = true;
        mTestController.onDownloadItemUpdated(createDownloadItem(offlineItem));
        mTestController.verify(MESSAGE_SPEEDING_UP);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    @Features.DisableFeatures(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)
    public void testMultipleDownloadInProgress() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onDownloadItemUpdated(createDownloadItem(item1));
        mTestController.verify(MESSAGE_DOWNLOADING_FILE);

        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onDownloadItemUpdated(createDownloadItem(item2));
        mTestController.verify(MESSAGE_DOWNLOADING_TWO_FILES);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    @Features.DisableFeatures(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)
    public void testAcceleratedChangesToDownloadingAfterDelay() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        item1.isAccelerated = true;
        mTestController.onDownloadItemUpdated(createDownloadItem(item1));
        mTestController.verify(MESSAGE_SPEEDING_UP);

        waitForMessage(MESSAGE_DOWNLOADING_FILE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleOfflineItemComplete() {
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE);

        markItemComplete(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemComplete() {
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE);

        markItemComplete(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE);

        OfflineItem item2 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_COMPLETE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleOfflineItemFailed() {
        OfflineItem item = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemFailed() {
        OfflineItem item = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED);

        OfflineItem item2 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_FAILED);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleOfflineItemPending() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemPending() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING);

        OfflineItem item2 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_PENDING);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleOfflineItemScheduled() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        item.schedule = new OfflineItemSchedule(true, 0);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_SCHEDULED_WIFI);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemScheduled() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        item1.schedule = new OfflineItemSchedule(true, 0);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_SCHEDULED_WIFI);

        OfflineItem item2 = createOfflineItem(OfflineItemState.PENDING);
        item2.schedule = new OfflineItemSchedule(true, 0);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_SCHEDULED);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testNewDownloadShowsUpImmediately() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE);

        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        item2.isAccelerated = true;
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SPEEDING_UP);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testPausedDownloadsAreIgnored() {
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE);

        item.state = OfflineItemState.PAUSED;
        mTestController.onItemUpdated(item);
        mTestController.verifyInfoBarClosed();

        item.state = OfflineItemState.IN_PROGRESS;
        mTestController.onItemUpdated(item);
        mTestController.verifyInfoBarClosed();

        markItemComplete(item);
        mTestController.onItemUpdated(item);
        mTestController.verifyInfoBarClosed();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testOnItemRemoved() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item1);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOADING_TWO_FILES);

        mTestController.onItemRemoved(item1.id);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE);

        mTestController.onItemRemoved(item2.id);
        mTestController.verifyInfoBarClosed();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testCancelledItemWillCloseInfoBar() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING);

        item.state = OfflineItemState.CANCELLED;
        mTestController.onItemUpdated(item);
        mTestController.verifyInfoBarClosed();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testCompleteFailedComplete() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE);

        OfflineItem item2 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE);

        OfflineItem item3 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_COMPLETE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testPendingFailedPending() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING);

        OfflineItem item2 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING);

        OfflineItem item3 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_PENDING);

        waitForMessage(MESSAGE_DOWNLOAD_FAILED);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testProgressCompleteFailedProgress() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE);

        OfflineItem item2 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE);

        OfflineItem item3 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE);

        waitForMessage(MESSAGE_DOWNLOAD_FAILED);
        waitForMessage(MESSAGE_DOWNLOADING_FILE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testCompleteShowsUpImmediately() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED);

        OfflineItem item2 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED);

        markItemComplete(item2);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testResumeFromPendingShowsUpImmediately() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING);

        OfflineItem item2 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_PENDING);

        item2.state = OfflineItemState.IN_PROGRESS;
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING);

        item1.state = OfflineItemState.IN_PROGRESS;
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOADING_TWO_FILES);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testPausedAfterPendingWillCloseInfoBar() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING);

        item.state = OfflineItemState.PAUSED;
        mTestController.onItemUpdated(item);
        mTestController.verifyInfoBarClosed();

        item.state = OfflineItemState.IN_PROGRESS;
        mTestController.onItemUpdated(item);
        mTestController.verifyInfoBarClosed();
    }
}
