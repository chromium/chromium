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

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
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
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Tests Chrome download feature by attempting to download some files. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class DownloadTest {
    @ClassRule
    public static DownloadTestRule sDownloadTestRule =
            new DownloadTestRule(
                    new CustomMainActivityStart() {
                        @Override
                        public void customMainActivityStart() throws InterruptedException {
                            sDownloadTestRule.startMainActivityOnBlankPage();
                        }
                    });

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sDownloadTestRule, false);

    private static final String SUPERBO_CONTENTS = "plain text response from a POST";

    private static EmbeddedTestServer sTestServer;

    private static final String TEST_DOWNLOAD_DIRECTORY = "/chrome/test/data/android/download/";

    private static final String FILENAME_WALLPAPER = "[large]wallpaper.dm";
    private static final String FILENAME_TEXT = "superbo.txt";
    private static final String FILENAME_TEXT_1 = "superbo (1).txt";
    private static final String FILENAME_TEXT_2 = "superbo (2).txt";
    private static final String FILENAME_SWF = "test.swf";
    private static final String FILENAME_GZIP = "test.gzip";

    private static final String[] TEST_FILES =
            new String[] {
                FILENAME_WALLPAPER,
                FILENAME_TEXT,
                FILENAME_TEXT_1,
                FILENAME_TEXT_2,
                FILENAME_SWF,
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
        public int notifyDownloadSuccessful(
                final ContentId id,
                final String filePath,
                final String fileName,
                final long systemDownloadId,
                final OTRProfileID otrProfileID,
                final boolean isSupportedMimeType,
                final boolean isOpenable,
                final Bitmap icon,
                final GURL originalUrl,
                final boolean shouldPromoteOrigin,
                final GURL referrer,
                final long totalBytes) {
            return 0;
        }

        @Override
        public void notifyDownloadProgress(
                final ContentId id,
                final String fileName,
                final Progress progress,
                final long bytesReceived,
                final long timeRemainingInMillis,
                final long startTime,
                final OTRProfileID otrProfileID,
                final boolean canDownloadWhileMetered,
                final boolean isTransient,
                final Bitmap icon,
                final GURL originalUrl,
                final boolean shouldPromoteOrigin) {}

        @Override
        void notifyDownloadPaused(
                ContentId id,
                String fileName,
                boolean isResumable,
                boolean isAutoResumable,
                OTRProfileID otrProfileID,
                boolean isTransient,
                Bitmap icon,
                final GURL originalUrl,
                final boolean shouldPromoteOrigin,
                boolean hasUserGesture,
                boolean forceRebuild,
                @PendingState int pendingState) {}

        @Override
        public void notifyDownloadFailed(
                final ContentId id,
                final String fileName,
                final Bitmap icon,
                final GURL originalUrl,
                final boolean shouldPromoteOrigin,
                OTRProfileID otrProfileID,
                @FailState int failState) {}

        @Override
        public void notifyDownloadCanceled(final ContentId id, boolean hasUserGesture) {}

        @Override
        void resumeDownload(Intent intent) {}
    }

    @BeforeClass
    public static void beforeClass() {
        Looper.prepare();
        sTestServer = sDownloadTestRule.getTestServer();
        DownloadNotificationService.setInstanceForTests(new MockNotificationService());
    }

    @Before
    public void setUp() {
        sDownloadTestRule.resetCallbackHelper();
    }

    @After
    public void tearDown() {
        deleteTestFiles();
    }

    void waitForLastDownloadToFinish() {
        CriteriaHelper.pollUiThread(
                () -> {
                    List<DownloadItem> downloads = sDownloadTestRule.getAllDownloads();
                    Criteria.checkThat(downloads.size(), Matchers.greaterThanOrEqualTo(1));
                    Criteria.checkThat(
                            downloads.get(downloads.size() - 1).getDownloadInfo().state(),
                            Matchers.is(DownloadState.COMPLETE));
                });
    }

    void waitForAnyDownloadToCancel() {
        CriteriaHelper.pollUiThread(
                () -> {
                    List<DownloadItem> downloads = sDownloadTestRule.getAllDownloads();
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
    @LargeTest
    @Feature({"Downloads"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testHttpGetDownload() throws Exception {
        loadUrl(sTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "get.html"));
        waitForFocus();
        View currentView = sDownloadTestRule.getActivity().getActivityTab().getView();

        int callCount = sDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(sDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(sDownloadTestRule.hasDownloaded(FILENAME_GZIP, null));
    }

    @Test
    @LargeTest
    @Feature({"Downloads"})
    public void testHttpPostDownload() throws Exception {
        loadUrl(sTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        View currentView = sDownloadTestRule.getActivity().getActivityTab().getView();

        int callCount = sDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(sDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(sDownloadTestRule.hasDownloaded(FILENAME_TEXT, SUPERBO_CONTENTS));
    }

    @Test
    @LargeTest
    @Feature({"Downloads"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @Policies.Add({@Policies.Item(key = "PromptForDownloadLocation", string = "false")})
    public void testCloseEmptyDownloadTab() throws Exception {
        loadUrl(sTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "get.html"));
        waitForFocus();
        final int initialTabCount = sDownloadTestRule.getActivity().getCurrentTabModel().getCount();
        int currentCallCount = sDownloadTestRule.getChromeDownloadCallCount();
        View currentView = sDownloadTestRule.getActivity().getActivityTab().getView();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(sDownloadTestRule.waitForChromeDownloadToFinish(currentCallCount));

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            sDownloadTestRule.getActivity().getCurrentTabModel().getCount(),
                            Matchers.is(initialTabCount));
                });
    }

    private void openNewTab(String url) {
        Tab oldTab = sDownloadTestRule.getActivity().getActivityTabProvider().get();
        TabCreator tabCreator = sDownloadTestRule.getActivity().getTabCreator(false);
        final TabModel model = sDownloadTestRule.getActivity().getCurrentTabModel();
        final int count = model.getCount();
        final Tab newTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return tabCreator.createNewTab(
                                    new LoadUrlParams(url, PageTransition.LINK),
                                    TabLaunchType.FROM_LINK,
                                    oldTab);
                        });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(count + 1, Matchers.is(model.getCount()));
                    Criteria.checkThat(newTab, Matchers.is(model.getTabAt(count)));
                    Criteria.checkThat(ChromeTabUtils.isRendererReady(newTab), Matchers.is(true));
                });
    }

    @Test
    @LargeTest
    @Feature({"Downloads"})
    public void testUrlEscaping() throws Exception {
        loadUrl(sTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "urlescaping.html"));
        waitForFocus();
        View currentView = sDownloadTestRule.getActivity().getActivityTab().getView();

        int callCount = sDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(sDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(sDownloadTestRule.hasDownloaded(FILENAME_WALLPAPER, null));
    }

    private void loadUrl(String url) {
        sDownloadTestRule.loadUrlInTab(
                url,
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                sDownloadTestRule.getActivity().getActivityTab(),
                20L // 20 seconds timeout
                );
    }

    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testOMADownloadInterception() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        try {
            final DownloadManagerRequestInterceptorForTest interceptor =
                    new DownloadManagerRequestInterceptorForTest();
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            DownloadManagerService.getDownloadManagerService()
                                    .setDownloadManagerRequestInterceptor(interceptor));
            List<Pair<String, String>> headers = new ArrayList<>();
            headers.add(Pair.create("Content-Type", "application/vnd.oma.drm.message"));
            final String url = webServer.setResponse("/test.dm", "testdata", headers);
            sDownloadTestRule.loadUrl(
                    UrlUtils.encodeHtmlDataUri(
                            "<script>"
                                    + "  function download() {"
                                    + "    window.open( '"
                                    + url
                                    + "')"
                                    + "  }"
                                    + "</script>"
                                    + "<body id='body' onclick='download()'></body>"));
            DOMUtils.clickNode(sDownloadTestRule.getActivity().getCurrentWebContents(), "body");
            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(interceptor.mDownloadItem, Matchers.notNullValue());
                        Criteria.checkThat(
                                interceptor.mDownloadItem.getDownloadInfo().getUrl().getSpec(),
                                Matchers.is(url));
                    });
        } finally {
            webServer.shutdown();
        }
    }

    private void waitForFocus() {
        View currentView = sDownloadTestRule.getActivity().getActivityTab().getView();
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
        sDownloadTestRule.deleteFilesInDownloadDirectory(TEST_FILES);
    }
}
