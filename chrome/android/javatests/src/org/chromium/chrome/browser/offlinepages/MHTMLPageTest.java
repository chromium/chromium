// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.download.DownloadTestRule;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.PageTransition;

import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for offline page request handling. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MHTMLPageTest implements CustomMainActivityStart {
    @Rule public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private static final int TIMEOUT_MS = 5000;
    private static final String[] TEST_FILES = new String[] {"hello.mhtml", "test.mht"};

    private EmbeddedTestServer mTestServer;

    /**
     * Observes the download updates from the new download backend. Depending on whether the new
     * download backend is enabled or not, either this class or TestDownloadNotificationService will
     * receive the update.
     */
    private static class TestNewDownloadBackendObserver implements OfflineContentProvider.Observer {
        private Semaphore mSemaphore;

        TestNewDownloadBackendObserver(Semaphore semaphore) {
            mSemaphore = semaphore;
        }

        @Override
        public void onItemsAdded(List<OfflineItem> items) {}

        @Override
        public void onItemRemoved(ContentId id) {}

        @Override
        public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
            mSemaphore.release();
        }
    }

    @Before
    public void setUp() {
        deleteTestFiles();
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        ApplicationProvider.getApplicationContext(), ServerCertificate.CERT_OK);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        deleteTestFiles();
    }

    @Override
    public void customMainActivityStart() throws InterruptedException {
        mDownloadTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "Flaky. crbug.com/1030558")
    public void testDownloadMultipartRelatedPageFromServer() throws Exception {
        // .mhtml file is mapped to "multipart/related" by the test server.
        final String url = mTestServer.getURL("/chrome/test/data/android/hello.mhtml");
        final Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        final Semaphore semaphore = new Semaphore(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OfflineContentAggregatorFactory.get()
                            .addObserver(new TestNewDownloadBackendObserver(semaphore));
                    tab.loadUrl(
                            new LoadUrlParams(
                                    url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR));
                });

        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    @SmallTest
    public void testDownloadMessageRfc822PageFromServer() throws Exception {
        // .mht file is mapped to "message/rfc822" by the test server.
        final String url = mTestServer.getURL("/chrome/test/data/android/test.mht");
        final Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        final Semaphore semaphore = new Semaphore(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OfflineContentAggregatorFactory.get()
                            .addObserver(new TestNewDownloadBackendObserver(semaphore));
                    tab.loadUrl(
                            new LoadUrlParams(
                                    url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR));
                });

        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    @SmallTest
    public void testLoadMultipartRelatedPageFromLocalFile() {
        // .mhtml file is mapped to "multipart/related" by the test server.
        String url = UrlUtils.getIsolatedTestFileUrl("chrome/test/data/android/hello.mhtml");
        mDownloadTestRule.loadUrl(url);
    }

    @Test
    @SmallTest
    public void testLoadMessageRfc822PageFromLocalFile() {
        // .mht file is mapped to "message/rfc822" by the test server.
        String url = UrlUtils.getIsolatedTestFileUrl("chrome/test/data/android/test.mht");
        mDownloadTestRule.loadUrl(url);
    }

    /**
     * Makes sure there are no files with names identical to the ones this test uses in the
     * downloads directory
     */
    private void deleteTestFiles() {
        mDownloadTestRule.deleteFilesInDownloadDirectory(TEST_FILES);
    }
}
