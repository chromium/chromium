// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.download.DownloadDangerType;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.UUID;

/**
 * Test class to validate that the {@link DownloadMessageUiControllerImpl} correctly represents the
 * state of the downloads in the current chrome session.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DownloadMessageUiControllerTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final String MESSAGE_DOWNLOADING_FILE = "Downloading file…";
    private static final String MESSAGE_DOWNLOADING_TWO_FILES = "Downloading 2 files…";
    private static final String MESSAGE_SINGLE_DOWNLOAD_COMPLETE = "File downloaded";
    private static final String MESSAGE_TWO_DOWNLOAD_COMPLETE = "2 downloads complete";
    private static final String MESSAGE_DOWNLOAD_FAILED = "1 download failed";
    private static final String MESSAGE_TWO_DOWNLOAD_FAILED = "2 downloads failed";
    private static final String MESSAGE_DOWNLOAD_PENDING = "1 download pending";
    private static final String MESSAGE_TWO_DOWNLOAD_PENDING = "2 downloads pending";
    private static final String MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED = "Dangerous download blocked";
    private static final String MESSAGE_TWO_DOWNLOAD_DANGEROUS_BLOCKED =
            "2 dangerous downloads blocked";

    private static final String DESCRIPTION_DOWNLOADING = "See notification for download status";
    private static final String DESCRIPTION_DOWNLOAD_COMPLETE = "(0.01 KB) www.example.com";

    private static final GURL LONG_URL_NEEDS_ELIDING =
            new GURL("https://veryveryveryverylongsubdomain.example.com/");
    private static final String DESCRIPTION_DOWNLOAD_COMPLETE_ELIDED = "(0.01 KB) example.com";
    private static final String DESCRIPTION_BLOCKED = "Blocked by your organization";
    private static final String DESCRIPTION_ONE_BLOCKED = "1 was blocked by your organization";

    private static final String TEST_FILE_NAME = "TestFile";
    private static final long TEST_TO_NEXT_STEP_DELAY = 100;

    private TestDownloadMessageUiController mTestController;

    @Before
    public void before() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestController = new TestDownloadMessageUiController();
                });
    }

    public void enableDangerousDownloadMessage() {
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.MALICIOUS_APK_DOWNLOAD_CHECK)
                .param(ChromeFeatureList.sMaliciousApkDownloadCheckTelemetryOnly.getName(), false)
                .apply();
    }

    static class TestDelegate implements DownloadMessageUiController.Delegate {
        @Override
        public @Nullable Context getContext() {
            return ApplicationProvider.getApplicationContext();
        }

        @Override
        public @Nullable MessageDispatcher getMessageDispatcher() {
            return null;
        }

        @Override
        public @Nullable ModalDialogManager getModalDialogManager() {
            return null;
        }

        @Override
        public boolean maybeSwitchToFocusedActivity() {
            return false;
        }

        @Override
        public void openDownloadsPage(OtrProfileId otrProfileId, int source) {}

        @Override
        public void openDownload(
                OfflineItem offlineItem, OtrProfileId otrProfileId, int source, Context context) {}

        @Override
        public void removeNotification(int notificationId, DownloadInfo downloadInfo) {}
    }

    static class TestDownloadMessageUiController extends DownloadMessageUiControllerImpl {
        private DownloadProgressMessageUiData mInfo;

        public TestDownloadMessageUiController() {
            super(new TestDelegate());
        }

        @Override
        protected void showMessage(@UiState int state, DownloadProgressMessageUiData info) {
            mInfo = info;
        }

        @Override
        protected void closePreviousMessage() {
            mInfo = null;
        }

        @Override
        protected long getDelayToNextStep(int resultState) {
            return TEST_TO_NEXT_STEP_DELAY;
        }

        public void onItemUpdated(OfflineItem item) {
            super.onItemUpdated(item.clone(), null);
        }

        void verify(String message, String description) {
            Assert.assertNotNull(mInfo);
            Assert.assertEquals(message, mInfo.message);
            Assert.assertEquals(description, mInfo.description);
        }

        void verifyMessageGone() {
            Assert.assertNull(mInfo);
        }

        void verifyIgnoreAction(boolean expected) {
            Assert.assertEquals(expected, mInfo.ignoreAction);
        }
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
        item.url = JUnitTestGURLs.EXAMPLE_URL;
        item.receivedBytes = 10L;
        item.totalSizeBytes = 10L;
    }

    private static void markItemDangerous(OfflineItem item) {
        item.isDangerous = true;
        item.dangerType = DownloadDangerType.DANGEROUS_CONTENT;
        item.title = TEST_FILE_NAME;
    }

    private static void markItemValidated(OfflineItem item) {
        item.isDangerous = false;
        item.dangerType = DownloadDangerType.USER_VALIDATED;
        item.title = TEST_FILE_NAME;
    }

    private void waitForMessage(String message) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(mTestController.mInfo, Matchers.notNullValue());
                    Criteria.checkThat(mTestController.mInfo.message, Matchers.is(message));
                });
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testOfflinePageDownloadStarted() {
        mTestController.onDownloadStarted();
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleOfflineItemComplete() {
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);

        markItemComplete(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemComplete() {
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);

        markItemComplete(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);

        OfflineItem item2 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_COMPLETE, null);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleOfflineItemFailed() {
        OfflineItem item = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED, null);
        mTestController.verifyIgnoreAction(false);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemFailed() {
        OfflineItem item = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED, null);
        mTestController.verifyIgnoreAction(false);

        OfflineItem item2 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_FAILED, null);
        mTestController.verifyIgnoreAction(false);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleOfflineItemBlocked() {
        OfflineItem item = createOfflineItem(OfflineItemState.FAILED);
        item.failState = FailState.FILE_BLOCKED;
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED, DESCRIPTION_BLOCKED);
        mTestController.verifyIgnoreAction(true);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemBlocked() {
        OfflineItem item = createOfflineItem(OfflineItemState.FAILED);
        item.failState = FailState.FILE_BLOCKED;
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED, DESCRIPTION_BLOCKED);
        mTestController.verifyIgnoreAction(true);

        OfflineItem item2 = createOfflineItem(OfflineItemState.FAILED);
        item2.failState = FailState.FILE_BLOCKED;
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_FAILED, DESCRIPTION_BLOCKED);
        mTestController.verifyIgnoreAction(true);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testOneOutOfMultipleOfflineItemBlocked() {
        OfflineItem item = createOfflineItem(OfflineItemState.FAILED);
        item.failState = FailState.FILE_BLOCKED;
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED, DESCRIPTION_BLOCKED);
        mTestController.verifyIgnoreAction(true);

        OfflineItem item2 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_FAILED, DESCRIPTION_ONE_BLOCKED);
        mTestController.verifyIgnoreAction(false);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemCompleteAndBlocked() {
        OfflineItem item = createOfflineItem(OfflineItemState.FAILED);
        item.failState = FailState.FILE_BLOCKED;
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED, DESCRIPTION_BLOCKED);
        mTestController.verifyIgnoreAction(true);

        OfflineItem item2 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);
        mTestController.verifyIgnoreAction(false);

        OfflineItem item3 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_COMPLETE, null);
        mTestController.verifyIgnoreAction(false);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleOfflineItemPending() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemPending() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        OfflineItem item2 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_PENDING, null);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleOfflineItemDangerous() {
        enableDangerousDownloadMessage();
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, TEST_FILE_NAME);
        mTestController.verifyIgnoreAction(false);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemDangerous() {
        enableDangerousDownloadMessage();
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOADING_TWO_FILES, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, TEST_FILE_NAME);
        mTestController.verifyIgnoreAction(false);
        markItemDangerous(item2);
        mTestController.onItemUpdated(item2);
        // The OfflineItem title is not shown in the description for multiple dangerous downloads.
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_DANGEROUS_BLOCKED, null);
        mTestController.verifyIgnoreAction(false);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testTwoOfflineItemDangerousThenValidateOne() {
        enableDangerousDownloadMessage();
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOADING_TWO_FILES, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item);
        item.title = "dangerous1.apk";
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, "dangerous1.apk");
        mTestController.verifyIgnoreAction(false);
        markItemDangerous(item2);
        item2.title = "dangerous2.apk";
        mTestController.onItemUpdated(item2);
        // The OfflineItem title is not shown in the description for multiple dangerous downloads.
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_DANGEROUS_BLOCKED, null);
        mTestController.verifyIgnoreAction(false);
        // Validate only one of the items.
        markItemValidated(item);
        mTestController.onItemUpdated(item);
        // The dangerous message for the second item remains.
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, "dangerous2.apk");
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testMultipleOfflineItemCompleteAndDangerous() {
        enableDangerousDownloadMessage();
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, TEST_FILE_NAME);
        mTestController.verifyIgnoreAction(false);

        OfflineItem item2 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);
        mTestController.verifyIgnoreAction(false);
        OfflineItem item3 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_COMPLETE, null);
        mTestController.verifyIgnoreAction(false);

        OfflineItem item4 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item4);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item4);
        mTestController.onItemUpdated(item4);
        // The previous "complete" result is still shown for a few moments before the "dangerous"
        // message appears.
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_COMPLETE, null);
        // The dangerous download from before is combined with this new one, so there are 2
        // dangerous downloads.
        waitForMessage(MESSAGE_TWO_DOWNLOAD_DANGEROUS_BLOCKED);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_DANGEROUS_BLOCKED, null);
        mTestController.verifyIgnoreAction(false);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testNewDownloadShowsUpImmediately() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);

        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        item2.isAccelerated = true;
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testPausedDownloadsAreIgnored() {
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);

        item.state = OfflineItemState.PAUSED;
        mTestController.onItemUpdated(item);
        mTestController.verifyMessageGone();

        item.state = OfflineItemState.IN_PROGRESS;
        mTestController.onItemUpdated(item);
        mTestController.verifyMessageGone();

        markItemComplete(item);
        mTestController.onItemUpdated(item);
        mTestController.verifyMessageGone();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testOnItemRemoved() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item1);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOADING_TWO_FILES, DESCRIPTION_DOWNLOADING);

        mTestController.onItemRemoved(item1.id);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);

        mTestController.onItemRemoved(item2.id);
        mTestController.verifyMessageGone();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testCancelledItemWillCloseMessageUi() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        item.state = OfflineItemState.CANCELLED;
        mTestController.onItemUpdated(item);
        mTestController.verifyMessageGone();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testCancelledItemWillClearDangerousItems() {
        enableDangerousDownloadMessage();
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, TEST_FILE_NAME);
        mTestController.verifyIgnoreAction(false);

        // Cancel the item.
        item.state = OfflineItemState.CANCELLED;
        mTestController.onItemUpdated(item);
        mTestController.verifyMessageGone();

        // Now a second dangerous item appears.
        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item2);
        item2.title = "a_different_file.apk";
        mTestController.onItemUpdated(item2);
        // The first dangerous download was cleared when canceled, so the new message only reflects
        // 1 dangerous item.
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, "a_different_file.apk");
        mTestController.verifyIgnoreAction(false);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testCompleteFailedComplete() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);

        OfflineItem item2 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);

        OfflineItem item3 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_COMPLETE, null);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testCompleteDangerousComplete() {
        enableDangerousDownloadMessage();
        OfflineItem item1 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);

        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item2);
        mTestController.onItemUpdated(item2);
        // The previous "complete" message is still shown for a few moments before the "dangerous"
        // message appears.
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);
        // Waiting for the message to cycle means the previous "complete" is cleared.
        waitForMessage(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, TEST_FILE_NAME);

        // Another complete item is now the only "complete".
        OfflineItem item3 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testPendingFailedPending() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        OfflineItem item2 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        OfflineItem item3 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_PENDING, null);

        waitForMessage(MESSAGE_DOWNLOAD_FAILED);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testDangerousFailedDangerous() {
        enableDangerousDownloadMessage();
        OfflineItem item1 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item1);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, TEST_FILE_NAME);

        OfflineItem item2 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, TEST_FILE_NAME);

        OfflineItem item3 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item3);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_DANGEROUS_BLOCKED, null);

        // Waiting for the message to cycle means the previous "dangerous" (for both downloads) is
        // cleared.
        waitForMessage(MESSAGE_DOWNLOAD_FAILED);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED, null);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testPendingAndDangerousAndPendingAndDangerous() {
        enableDangerousDownloadMessage();
        OfflineItem item1 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item2);
        item2.title = "dangerous2.apk";
        mTestController.onItemUpdated(item2);
        // The previous "pending" result is still shown for a few moments before the "dangerous"
        // message appears.
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);
        waitForMessage(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, "dangerous2.apk");

        OfflineItem item3 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item3);
        // The previous "dangerous" result is still shown for a few moments before the "pending"
        // message appears.
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, "dangerous2.apk");
        waitForMessage(MESSAGE_DOWNLOAD_PENDING);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        // Add a new item and mark it dangerous.
        OfflineItem item4 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item4);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item4);
        item4.title = "dangerous4.apk";
        mTestController.onItemUpdated(item4);
        // The previous "pending" result is still shown for a few moments before the "dangerous"
        // message appears.
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);
        waitForMessage(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, "dangerous4.apk");
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testPendingAndDangerousAndPendingThenImmediatelyDangerous() {
        enableDangerousDownloadMessage();
        OfflineItem item1 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        OfflineItem item2 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item2);
        item2.title = "dangerous2.apk";
        mTestController.onItemUpdated(item2);
        // The previous "pending" result is still shown for a few moments before the "dangerous"
        // message appears.
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);
        waitForMessage(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, "dangerous2.apk");

        OfflineItem item3 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item3);
        // The previous "dangerous" result is still shown for a few moments.
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, "dangerous2.apk");
        // This time (unlike the test above), don't wait for the message to cycle.

        // Immediately add a new item.
        OfflineItem item4 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item4);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item4);
        item4.title = "dangerous4.apk";
        mTestController.onItemUpdated(item4);
        // The new "dangerous" stacks onto the previous "dangerous" result.
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_DANGEROUS_BLOCKED, null);

        // Wait to cycle through the previous "pending" result.
        waitForMessage(MESSAGE_DOWNLOAD_PENDING);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testProgressCompleteFailedProgress() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);

        OfflineItem item2 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);

        OfflineItem item3 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);

        waitForMessage(MESSAGE_DOWNLOAD_FAILED);
        waitForMessage(MESSAGE_DOWNLOADING_FILE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testCompleteShowsUpImmediately() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.FAILED);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED, null);

        OfflineItem item2 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOAD_FAILED, null);

        markItemComplete(item2);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testResumeFromPendingShowsUpImmediately() {
        OfflineItem item1 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        OfflineItem item2 = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_TWO_DOWNLOAD_PENDING, null);

        item2.state = OfflineItemState.IN_PROGRESS;
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        item1.state = OfflineItemState.IN_PROGRESS;
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOADING_TWO_FILES, DESCRIPTION_DOWNLOADING);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testValidatedAfterDangerousShowsUpImmediately() {
        enableDangerousDownloadMessage();
        OfflineItem item = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, TEST_FILE_NAME);
        mTestController.verifyIgnoreAction(false);
        markItemValidated(item);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testProgressAndCompleteAndDangerousValidatedComplete() {
        enableDangerousDownloadMessage();
        OfflineItem item1 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item1);
        mTestController.verify(MESSAGE_DOWNLOADING_FILE, DESCRIPTION_DOWNLOADING);

        OfflineItem item2 = createOfflineItem(OfflineItemState.COMPLETE);
        mTestController.onItemUpdated(item2);
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);

        OfflineItem item3 = createOfflineItem(OfflineItemState.IN_PROGRESS);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_DOWNLOADING_TWO_FILES, DESCRIPTION_DOWNLOADING);
        markItemDangerous(item3);
        item3.title = "dangerous3.apk";
        mTestController.onItemUpdated(item3);
        // The previous "complete" message is still shown for a few moments before the "dangerous"
        // message appears.
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);
        waitForMessage(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED);
        mTestController.verify(MESSAGE_DOWNLOAD_DANGEROUS_BLOCKED, "dangerous3.apk");

        // Validate the item to generate a progress message, which combines with the "progress" for
        // item1.
        markItemValidated(item3);
        mTestController.onItemUpdated(item3);
        mTestController.verify(MESSAGE_DOWNLOADING_TWO_FILES, DESCRIPTION_DOWNLOADING);
        markItemComplete(item3);
        mTestController.onItemUpdated(item3);
        // Since we cycled through the previous "complete" message, item3 is now the only complete.
        mTestController.verify(MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testPausedAfterPendingWillCloseMessageUi() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        mTestController.onItemUpdated(item);
        mTestController.verify(MESSAGE_DOWNLOAD_PENDING, null);

        item.state = OfflineItemState.PAUSED;
        mTestController.onItemUpdated(item);
        mTestController.verifyMessageGone();

        item.state = OfflineItemState.IN_PROGRESS;
        mTestController.onItemUpdated(item);
        mTestController.verifyMessageGone();

        markItemDangerous(item);
        mTestController.onItemUpdated(item);
        mTestController.verifyMessageGone();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testLongUrlsAreElided() {
        OfflineItem item = createOfflineItem(OfflineItemState.PENDING);
        markItemComplete(item);
        item.url = LONG_URL_NEEDS_ELIDING;
        mTestController.onItemUpdated(item);

        mTestController.verify(
                MESSAGE_SINGLE_DOWNLOAD_COMPLETE, DESCRIPTION_DOWNLOAD_COMPLETE_ELIDED);
    }
}
