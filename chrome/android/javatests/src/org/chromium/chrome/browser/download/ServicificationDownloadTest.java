// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.graphics.Bitmap;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ReducedModeNativeTestRule;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.url.GURL;

import java.util.List;

/** Tests interrupted download can be resumed with minimal browser mode. */
@RunWith(ChromeJUnit4ClassRunner.class)
public final class ServicificationDownloadTest {
    @Rule public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();
    @Rule public ReducedModeNativeTestRule mNativeTestRule = new ReducedModeNativeTestRule();

    private static final String TEST_DOWNLOAD_FILE = "/chrome/test/data/android/download/test.gzip";
    private static final String DOWNLOAD_GUID = "F7FB1F59-7DE1-4845-AFDB-8A688F70F583";
    private MockDownloadNotificationService mNotificationService;
    private DownloadUpdateObserver mDownloadUpdateObserver;

    private static class MockDownloadNotificationService extends DownloadNotificationService {
        private boolean mDownloadCompleted;

        @Override
        public int notifyDownloadSuccessful(
                ContentId id,
                String filePath,
                String fileName,
                long systemDownloadId,
                OTRProfileID otrProfileID,
                boolean isSupportedMimeType,
                boolean isOpenable,
                Bitmap icon,
                GURL originalUrl,
                boolean shouldPromoteOrigin,
                GURL referrer,
                long totalBytes) {
            mDownloadCompleted = true;
            return 0;
        }

        public void waitForDownloadCompletion() {
            CriteriaHelper.pollUiThread(
                    () -> mDownloadCompleted, "Failed waiting for the download to complete.");
        }
    }

    private static class DownloadUpdateObserver implements OfflineContentProvider.Observer {
        private boolean mDownloadCompleted;

        @Override
        public void onItemsAdded(List<OfflineItem> items) {}

        @Override
        public void onItemRemoved(ContentId id) {}

        @Override
        public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
            mDownloadCompleted = item.state == OfflineItemState.COMPLETE;
        }

        public void waitForDownloadCompletion() {
            CriteriaHelper.pollUiThread(
                    () -> mDownloadCompleted, "Failed waiting for the download to complete.");
        }
    }

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNotificationService = new MockDownloadNotificationService();
                    mDownloadUpdateObserver = new DownloadUpdateObserver();
                });
    }

    @Test
    @LargeTest
    @Feature({"Download"})
    public void testResumeInterruptedDownloadUsingDownloadOfflineContentProvider() {
        mNativeTestRule.assertMinimalBrowserStarted();

        String tempFile =
                InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getCacheDir()
                                .getPath()
                        + "/test.gzip";
        TestFileUtil.deleteFile(tempFile);
        final String url = mEmbeddedTestServerRule.getServer().getURL(TEST_DOWNLOAD_FILE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DownloadManagerService downloadManagerService =
                            DownloadManagerService.getDownloadManagerService();
                    downloadManagerService.disableAddCompletedDownloadToDownloadManager();
                    ((SystemDownloadNotifier) downloadManagerService.getDownloadNotifier())
                            .setDownloadNotificationService(mNotificationService);
                    downloadManagerService.createInterruptedDownloadForTest(
                            url, DOWNLOAD_GUID, tempFile);
                    OfflineContentAggregatorFactory.get().addObserver(mDownloadUpdateObserver);
                    OfflineContentAggregatorFactory.get()
                            .resumeDownload(new ContentId("LEGACY_DOWNLOAD", DOWNLOAD_GUID));
                });
        mDownloadUpdateObserver.waitForDownloadCompletion();
    }
}
