// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Notification;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Looper;
import android.util.Pair;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.download.DownloadState;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests Chrome download feature by attempting to download some files.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadTest implements CustomMainActivityStart {
    @Rule
    public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private static final String SUPERBO_CONTENTS =
            "plain text response from a POST";

    private EmbeddedTestServer mTestServer;

    private static final String TEST_DOWNLOAD_DIRECTORY = "/chrome/test/data/android/download/";

    private static final String FILENAME_WALLPAPER = "[large]wallpaper.dm";
    private static final String FILENAME_TEXT = "superbo.txt";
    private static final String FILENAME_TEXT_1 = "superbo (1).txt";
    private static final String FILENAME_TEXT_2 = "superbo (2).txt";
    private static final String FILENAME_SWF = "test.swf";
    private static final String FILENAME_GZIP = "test.gzip";

    private static final String[] TEST_FILES = new String[] {
        FILENAME_WALLPAPER, FILENAME_TEXT, FILENAME_TEXT_1, FILENAME_TEXT_2, FILENAME_SWF,
        FILENAME_GZIP
    };

    static class DownloadManagerRequestInterceptorForTest
            implements DownloadManagerService.DownloadManagerRequestInterceptor {
        public DownloadItem mDownloadItem;

        @Override
        public void interceptDownloadRequest(DownloadItem item, boolean notifyComplete) {
            mDownloadItem = item;
            Assert.assertTrue(notifyComplete);
        }
    }

    static class TestDownloadMessageUiController implements DownloadMessageUiController {
        public TestDownloadMessageUiController() {}

        @Override
        public void onDownloadStarted() {}

        @Override
        public void showIncognitoDownloadMessage(Callback<Boolean> callback) {}

        @Override
        public void onNotificationShown(ContentId id, int notificationId) {}

        @Override
        public void addDownloadInterstitialSource(GURL originalUrl) {}

        @Override
        public boolean isDownloadInterstitialItem(GURL originalUrl, String guid) {
            return false;
        }

        @Override
        public void onItemsAdded(List<OfflineItem> items) {}

        @Override
        public void onItemRemoved(ContentId id) {}

        @Override
        public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {}

        @Override
        public boolean isShowing() {
            return false;
        }
    }

    private static class MockNotificationService extends DownloadNotificationService {
        @Override
        void updateNotification(int id, Notification notification) {}

        @Override
        public void cancelNotification(int notificationId, ContentId id) {}

        @Override
        public int notifyDownloadSuccessful(final ContentId id, final String filePath,
                final String fileName, final long systemDownloadId, final OTRProfileID otrProfileID,
                final boolean isSupportedMimeType, final boolean isOpenable, final Bitmap icon,
                final GURL originalUrl, final boolean shouldPromoteOrigin, final GURL referrer,
                final long totalBytes) {
            return 0;
        }

        @Override
        public void notifyDownloadProgress(final ContentId id, final String fileName,
                final Progress progress, final long bytesReceived, final long timeRemainingInMillis,
                final long startTime, final OTRProfileID otrProfileID,
                final boolean canDownloadWhileMetered, final boolean isTransient, final Bitmap icon,
                final GURL originalUrl, final boolean shouldPromoteOrigin) {}

        @Override
        void notifyDownloadPaused(ContentId id, String fileName, boolean isResumable,
                boolean isAutoResumable, OTRProfileID otrProfileID, boolean isTransient,
                Bitmap icon, final GURL originalUrl, final boolean shouldPromoteOrigin,
                boolean hasUserGesture, boolean forceRebuild, @PendingState int pendingState) {}

        @Override
        public void notifyDownloadFailed(final ContentId id, final String fileName,
                final Bitmap icon, final GURL originalUrl, final boolean shouldPromoteOrigin,
                OTRProfileID otrProfileID, @FailState int failState) {}

        @Override
        public void notifyDownloadCanceled(final ContentId id, boolean hasUserGesture) {}

        @Override
        void resumeDownload(Intent intent) {}
    }

    @Before
    public void setUp() {
        deleteTestFiles();
        Looper.prepare();
        mTestServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
        DownloadNotificationService.setInstanceForTests(new MockNotificationService());
    }

    @After
    public void tearDown() {
        deleteTestFiles();
    }

    @Override
    public void customMainActivityStart() throws InterruptedException {
        mDownloadTestRule.startMainActivityOnBlankPage();
    }

    void waitForLastDownloadToFinish() {
        CriteriaHelper.pollUiThread(() -> {
            List<DownloadItem> downloads = mDownloadTestRule.getAllDownloads();
            Criteria.checkThat(downloads.size(), Matchers.greaterThanOrEqualTo(1));
            Criteria.checkThat(downloads.get(downloads.size() - 1).getDownloadInfo().state(),
                    Matchers.is(DownloadState.COMPLETE));
        });
    }

    void waitForAnyDownloadToCancel() {
        CriteriaHelper.pollUiThread(() -> {
            List<DownloadItem> downloads = mDownloadTestRule.getAllDownloads();
            Criteria.checkThat(downloads.size(), Matchers.greaterThanOrEqualTo(1));
            boolean hasCanceled = false;
            for (DownloadItem download : downloads) {
                if (download.getDownloadInfo().state() == DownloadState.CANCELLED) {
                    hasCanceled = true;
                    break;
                }
            }
            Criteria.checkThat(hasCanceled, Matchers.is(true));
        });
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    public void testHttpGetDownload() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "get.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();

        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(mDownloadTestRule.hasDownloaded(FILENAME_GZIP, null));
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    public void testHttpPostDownload() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();

        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(mDownloadTestRule.hasDownloaded(FILENAME_TEXT, SUPERBO_CONTENTS));
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @Policies.Add({ @Policies.Item(key = "PromptForDownloadLocation", string = "false") })
    public void testCloseEmptyDownloadTab() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "get.html"));
        waitForFocus();
        final int initialTabCount = mDownloadTestRule.getActivity().getCurrentTabModel().getCount();
        int currentCallCount = mDownloadTestRule.getChromeDownloadCallCount();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(currentCallCount));

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mDownloadTestRule.getActivity().getCurrentTabModel().getCount(),
                    Matchers.is(initialTabCount));
        });
    }

    private void openNewTab(String url) {
        Tab oldTab = mDownloadTestRule.getActivity().getActivityTabProvider().get();
        TabCreator tabCreator = mDownloadTestRule.getActivity().getTabCreator(false);
        final TabModel model = mDownloadTestRule.getActivity().getCurrentTabModel();
        final int count = model.getCount();
        final Tab newTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return tabCreator.createNewTab(
                    new LoadUrlParams(url, PageTransition.LINK), TabLaunchType.FROM_LINK, oldTab);
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(count + 1, Matchers.is(model.getCount()));
            Criteria.checkThat(newTab, Matchers.is(model.getTabAt(count)));
            Criteria.checkThat(ChromeTabUtils.isRendererReady(newTab), Matchers.is(true));
        });
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    public void testUrlEscaping() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "urlescaping.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();

        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(mDownloadTestRule.hasDownloaded(FILENAME_WALLPAPER, null));
    }

    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testOMADownloadInterception() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        try {
            final DownloadManagerRequestInterceptorForTest interceptor =
                    new DownloadManagerRequestInterceptorForTest();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> DownloadManagerService.getDownloadManagerService()
                            .setDownloadManagerRequestInterceptor(interceptor));
            List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
            headers.add(Pair.create("Content-Type", "application/vnd.oma.drm.message"));
            final String url = webServer.setResponse("/test.dm", "testdata", headers);
            mDownloadTestRule.loadUrl(UrlUtils.encodeHtmlDataUri("<script>"
                    + "  function download() {"
                    + "    window.open( '" + url + "')"
                    + "  }"
                    + "</script>"
                    + "<body id='body' onclick='download()'></body>"));
            DOMUtils.clickNode(mDownloadTestRule.getActivity().getCurrentWebContents(), "body");
            CriteriaHelper.pollUiThread(() -> {
                Criteria.checkThat(interceptor.mDownloadItem, Matchers.notNullValue());
                Criteria.checkThat(
                        interceptor.mDownloadItem.getDownloadInfo().getUrl().getSpec(), Matchers.is(url));
            });
        } finally {
            webServer.shutdown();
        }
    }

    private void waitForFocus() {
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        if (!currentView.hasFocus()) {
            TouchCommon.singleClickView(currentView);
        }
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Makes sure there are no files with names identical to the ones this test uses in the
     * downloads directory
     */
    private void deleteTestFiles() {
        mDownloadTestRule.deleteFilesInDownloadDirectory(TEST_FILES);
    }
}
