// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link OfflinePageArchivePublisherBridge}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OfflinePageArchivePublisherBridgeTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";
    private static final int TIMEOUT_MS = 5000;
    private static final ClientId TEST_CLIENT_ID =
            new ClientId(OfflinePageBridge.DOWNLOAD_NAMESPACE, "1234");

    private OfflinePageBridge mOfflinePageBridge;
    private EmbeddedTestServer mTestServer;
    private String mTestPage;

    private void initializeBridgeForProfile(final boolean incognitoProfile)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            Profile profile = Profile.getLastUsedProfile();
            if (incognitoProfile) {
                profile = profile.getOffTheRecordProfile();
            }
            // Ensure we start in an offline state.
            mOfflinePageBridge = OfflinePageBridge.getForProfile(profile);
            if (mOfflinePageBridge == null || mOfflinePageBridge.isOfflinePageModelLoaded()) {
                semaphore.release();
                return;
            }
            mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                @Override
                public void offlinePageModelLoaded() {
                    semaphore.release();
                    mOfflinePageBridge.removeObserver(this);
                }
            });
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        if (!incognitoProfile) Assert.assertNotNull(mOfflinePageBridge);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Ensure we start in an offline state.
            NetworkChangeNotifier.forceConnectivityState(false);
            if (!NetworkChangeNotifier.isInitialized()) {
                NetworkChangeNotifier.init();
            }
        });

        initializeBridgeForProfile(false);

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    // TODO(iwells): Change "29" to "Build.VERSION_CODES.Q" when it's available.
    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_greater_than = 28)
    public void testAddCompletedDownload() throws InterruptedException, TimeoutException {
        Assert.assertTrue(OfflinePageArchivePublisherBridge.isAndroidDownloadManagerInstalled());

        mActivityTestRule.loadUrl(mTestPage);
        savePage(TEST_CLIENT_ID);
        OfflinePageItem page = OfflineTestUtil.getAllPages().get(0);

        long downloadId = OfflinePageArchivePublisherBridge.addCompletedDownload(page.getTitle(),
                "description", page.getFilePath(), page.getFileSize(), page.getUrl(), "");

        Assert.assertNotEquals(0L, downloadId);
    }

    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_greater_than = 28)
    public void testRemove() throws InterruptedException, TimeoutException {
        Assert.assertTrue(OfflinePageArchivePublisherBridge.isAndroidDownloadManagerInstalled());

        mActivityTestRule.loadUrl(mTestPage);
        savePage(TEST_CLIENT_ID);
        OfflinePageItem page = OfflineTestUtil.getAllPages().get(0);

        long downloadId = OfflinePageArchivePublisherBridge.addCompletedDownload(page.getTitle(),
                "description", page.getFilePath(), page.getFileSize(), page.getUrl(), "");

        Assert.assertNotEquals(0L, downloadId);

        long[] ids = new long[] {downloadId};
        Assert.assertEquals(1, OfflinePageArchivePublisherBridge.remove(ids));
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(29)
    public void testPublishArchiveToDownloadsCollection()
            throws InterruptedException, TimeoutException {
        // Save a page and publish.
        mActivityTestRule.loadUrl(mTestPage);
        savePage(TEST_CLIENT_ID);
        OfflinePageItem page = OfflineTestUtil.getAllPages().get(0);

        String publishedUri =
                OfflinePageArchivePublisherBridge.publishArchiveToDownloadsCollection(page);
        Assert.assertFalse(publishedUri.isEmpty());
        Assert.assertTrue(ContentUriUtils.isContentUri(publishedUri));
        Assert.assertTrue(ContentUriUtils.contentUriExists(publishedUri));
    }

    // Returns offline ID.
    private void savePage(final ClientId clientId) throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mOfflinePageBridge.savePage(
                    mActivityTestRule.getWebContents(), clientId, new SavePageCallback() {
                        @Override
                        public void onSavePageDone(int savePageResult, String url, long offlineId) {
                            semaphore.release();
                        }
                    });
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }
}
