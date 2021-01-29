// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.os.Handler;
import android.os.Looper;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItemSchedule;
import org.chromium.components.offline_items_collection.PendingState;

import java.util.UUID;

/**
 * Tests of {@link SystemDownloadNotifier}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SystemDownloadNotifierTest {
    @ClassRule
    public static final ChromeBrowserTestRule sBrowserTestRule = new ChromeBrowserTestRule();

    private final SystemDownloadNotifier mSystemDownloadNotifier = new SystemDownloadNotifier();
    private MockDownloadNotificationService mMockDownloadNotificationService;

    @BeforeClass
    public static void beforeClass() {
        Looper.prepare();
    }

    @Before
    public void setUp() {
        mMockDownloadNotificationService = new MockDownloadNotificationService();
        mSystemDownloadNotifier.setDownloadNotificationService(mMockDownloadNotificationService);
        mSystemDownloadNotifier.setHandler(new Handler(Looper.getMainLooper()));
    }

    private DownloadInfo getDownloadInfo(ContentId id) {
        return getDownloadInfoBuilder(id).build();
    }

    private DownloadInfo.Builder getDownloadInfoBuilder(ContentId id) {
        return new DownloadInfo.Builder()
                .setFileName("foo")
                .setBytesReceived(100)
                .setDownloadGuid(UUID.randomUUID().toString())
                .setContentId(id);
    }

    private void waitForNotifications(int numberOfNotifications) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mMockDownloadNotificationService.getNumberOfNotifications(),
                    Matchers.is(numberOfNotifications));
        });
    }

    /**
     * Tests that a single notification update will be immediately processed
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSingleNotification() {
        mSystemDownloadNotifier.notifyDownloadProgress(
                getDownloadInfo(new ContentId("download", "1")), 100,
                true /* canDownloadWhileMetered */);
        Assert.assertEquals(1, mMockDownloadNotificationService.getNumberOfNotifications());
    }

    /**
     * Tests that consecutive progress notifications can be merged together, and will be handled in
     * order.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testConsecutiveProgressNotifications() {
        DownloadInfo info = getDownloadInfo(new ContentId("download", "1"));
        mSystemDownloadNotifier.notifyDownloadProgress(
                info, 100, true /* canDownloadWhileMetered */);
        // Create 2 more progress updates on the same download and one of them will be skipped.
        mSystemDownloadNotifier.notifyDownloadProgress(
                info, 100, true /* canDownloadWhileMetered */);
        mSystemDownloadNotifier.notifyDownloadProgress(
                info, 100, true /* canDownloadWhileMetered */);
        // Create a progress update from a new download, this should create a new notification.
        mSystemDownloadNotifier.notifyDownloadProgress(
                getDownloadInfo(new ContentId("download", "2")), 100,
                true /* canDownloadWhileMetered */);
        Assert.assertEquals(1, mMockDownloadNotificationService.getNumberOfNotifications());
        int notificationId = mMockDownloadNotificationService.getLastNotificationId();
        waitForNotifications(2);
        Assert.assertEquals(
                notificationId, mMockDownloadNotificationService.getLastNotificationId());
        waitForNotifications(3);
        Assert.assertNotEquals(
                notificationId, mMockDownloadNotificationService.getLastNotificationId());
    }

    /**
     * Tests that higher priority notification will be handled before progress notification.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testNotificationWithDifferentPriorities() {
        DownloadInfo info = getDownloadInfo(new ContentId("download", "1"));
        mSystemDownloadNotifier.notifyDownloadProgress(
                info, 100, true /* canDownloadWhileMetered */);
        mSystemDownloadNotifier.notifyDownloadProgress(
                info, 100, true /* canDownloadWhileMetered */);
        mSystemDownloadNotifier.notifyDownloadSuccessful(
                getDownloadInfo(new ContentId("download", "2")), 1, true /* canResolve */,
                true /* isSupportedMimeType */);
        Assert.assertEquals(1, mMockDownloadNotificationService.getNumberOfNotifications());
        int notificationId = mMockDownloadNotificationService.getLastNotificationId();

        // The second notification should come from the 2nd download, as it has higher priority.
        waitForNotifications(2);
        Assert.assertNotEquals(
                notificationId, mMockDownloadNotificationService.getLastNotificationId());

        waitForNotifications(3);
        Assert.assertEquals(
                notificationId, mMockDownloadNotificationService.getLastNotificationId());
    }

    /**
     * No notifications when {@link DownloadInfo#getOfflineItemSchedule()} exists.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testDownloadLaterNotification() {
        DownloadInfo info = getDownloadInfoBuilder(new ContentId("download", "1"))
                                    .setOfflineItemSchedule(new OfflineItemSchedule(true, -1))
                                    .build();

        mSystemDownloadNotifier.notifyDownloadProgress(
                info, 100, true /* canDownloadWhileMetered */);
        mSystemDownloadNotifier.notifyDownloadPaused(info);
        mSystemDownloadNotifier.notifyDownloadFailed(info);
        mSystemDownloadNotifier.notifyDownloadInterrupted(info, true, PendingState.PENDING_NETWORK);

        Assert.assertEquals(0, mMockDownloadNotificationService.getNumberOfNotifications());
    }
}
