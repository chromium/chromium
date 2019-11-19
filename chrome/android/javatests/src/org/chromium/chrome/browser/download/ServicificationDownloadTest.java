// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ReducedModeNativeTestRule;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.ArrayList;

/**
 * Tests interrupted download can be resumed with Service Manager only mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
public final class ServicificationDownloadTest {
    @Rule
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();
    @Rule
    public ReducedModeNativeTestRule mNativeTestRule = new ReducedModeNativeTestRule();

    private static final String TEST_DOWNLOAD_FILE = "/chrome/test/data/android/download/test.gzip";
    private static final String DOWNLOAD_GUID = "F7FB1F59-7DE1-4845-AFDB-8A688F70F583";
    private MockDownloadNotificationService mNotificationService;
    private DownloadUpdateObserver mDownloadUpdateObserver;

    private static class MockDownloadNotificationService extends DownloadNotificationService {
        private boolean mDownloadCompleted;

        @Override
        public int notifyDownloadSuccessful(ContentId id, String filePath, String fileName,
                long systemDownloadId, boolean isOffTheRecord, boolean isSupportedMimeType,
                boolean isOpenable, Bitmap icon, String originalUrl, boolean shouldPromoteOrigin,
                String referrer, long totalBytes) {
            mDownloadCompleted = true;
            return 0;
        }

        public void waitForDownloadCompletion() {
            CriteriaHelper.pollUiThread(
                    new Criteria("Failed waiting for the download to complete.") {
                        @Override
                        public boolean isSatisfied() {
                            return mDownloadCompleted;
                        }
                    });
        }
    }

    private static class DownloadUpdateObserver implements OfflineContentProvider.Observer {
        private boolean mDownloadCompleted;

        @Override
        public void onItemsAdded(ArrayList<OfflineItem> items) {}

        @Override
        public void onItemRemoved(ContentId id) {}

        @Override
        public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
            mDownloadCompleted = item.state == OfflineItemState.COMPLETE;
        }

        public void waitForDownloadCompletion() {
            CriteriaHelper.pollUiThread(
                    new Criteria("Failed waiting for the download to complete.") {
                        @Override
                        public boolean isSatisfied() {
                            return mDownloadCompleted;
                        }
                    });
        }
    }

    @Before
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mNotificationService = new MockDownloadNotificationService();
            mDownloadUpdateObserver = new DownloadUpdateObserver();
        });
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
    }

    private static boolean useDownloadOfflineContentProvider() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER);
    }

    @Test
    @LargeTest
    @Feature({"Download"})
    public void testResumeInterruptedDownload() {
        if (useDownloadOfflineContentProvider()) return;
        mNativeTestRule.assertOnlyServiceManagerStarted();

        String tempFile = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getCacheDir()
                                  .getPath()
                + "/test.gzip";
        TestFileUtil.deleteFile(tempFile);
        DownloadItem item = new DownloadItem(false,
                new DownloadInfo.Builder()
                        .setDownloadGuid(DOWNLOAD_GUID)
                        .setIsOffTheRecord(false)
                        .build());
        final String url = mEmbeddedTestServerRule.getServer().getURL(TEST_DOWNLOAD_FILE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DownloadManagerService downloadManagerService =
                    DownloadManagerService.getDownloadManagerService();
            downloadManagerService.disableAddCompletedDownloadToDownloadManager();
            ((SystemDownloadNotifier) downloadManagerService.getDownloadNotifier())
                    .setDownloadNotificationService(mNotificationService);
            downloadManagerService.createInterruptedDownloadForTest(url, DOWNLOAD_GUID, tempFile);
            downloadManagerService.resumeDownload(
                    new ContentId("download", DOWNLOAD_GUID), item, true);
        });
        mNotificationService.waitForDownloadCompletion();
    }

    @Test
    @LargeTest
    @Feature({"Download"})
    @CommandLineFlags.Add({"enable-features=UseDownloadOfflineContentProvider"})
    public void testResumeInterruptedDownloadUsingDownloadOfflineContentProvider() {
        if (!useDownloadOfflineContentProvider()) return;
        mNativeTestRule.assertOnlyServiceManagerStarted();

        String tempFile = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getCacheDir()
                                  .getPath()
                + "/test.gzip";
        TestFileUtil.deleteFile(tempFile);
        final String url = mEmbeddedTestServerRule.getServer().getURL(TEST_DOWNLOAD_FILE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DownloadManagerService downloadManagerService =
                    DownloadManagerService.getDownloadManagerService();
            downloadManagerService.disableAddCompletedDownloadToDownloadManager();
            ((SystemDownloadNotifier) downloadManagerService.getDownloadNotifier())
                    .setDownloadNotificationService(mNotificationService);
            downloadManagerService.createInterruptedDownloadForTest(url, DOWNLOAD_GUID, tempFile);
            OfflineContentAggregatorFactory.get().addObserver(mDownloadUpdateObserver);
            OfflineContentAggregatorFactory.get().resumeDownload(
                    new ContentId("LEGACY_DOWNLOAD", DOWNLOAD_GUID), true);
        });
        mDownloadUpdateObserver.waitForDownloadCompletion();
    }
}
