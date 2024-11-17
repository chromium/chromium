// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.download.DownloadManagerBridge.DownloadQueryResult;
import org.chromium.chrome.browser.download.OMADownloadHandler.OMAInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.io.ByteArrayInputStream;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Tests for OMADownloadHandler class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class OMADownloadHandlerTest {
    @ClassRule
    public static final ChromeBrowserTestRule sBrowserTestRule = new ChromeBrowserTestRule();

    private static final String INSTALL_NOTIFY_URI = "http://test/test";

    private TestInfoBarController mTestInfoBarController;

    @Before
    public void before() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestInfoBarController = new TestInfoBarController();
                    DownloadManagerService.getDownloadManagerService()
                            .setInfoBarControllerForTesting(mTestInfoBarController);
                });
    }

    private Context getTestContext() {
        return new AdvancedMockContext(ApplicationProvider.getApplicationContext());
    }

    /** Mock implementation of the DownloadMessageUiController. */
    static class TestInfoBarController implements DownloadMessageUiController {
        public boolean mDownloadStarted;
        public OfflineItem mLastUpdatedItem;

        public TestInfoBarController() {}

        @Override
        public void onDownloadStarted() {
            mDownloadStarted = true;
        }

        @Override
        public void showIncognitoDownloadMessage(Callback<Boolean> callback) {}

        @Override
        public void addDownloadInterstitialSource(GURL originalUrl) {}

        @Override
        public boolean isDownloadInterstitialItem(GURL originalUrl, String guid) {
            return false;
        }

        @Override
        public void onItemsAdded(List<OfflineItem> items) {}

        @Override
        public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
            mLastUpdatedItem = item;
        }

        @Override
        public void onItemRemoved(ContentId id) {}

        @Override
        public void onNotificationShown(ContentId id, int notificationId) {}

        @Override
        public boolean isShowing() {
            return false;
        }

        public OfflineItem getLastUpdatedItem() {
            return mLastUpdatedItem;
        }
    }

    private static class OMADownloadHandlerForTest extends OMADownloadHandler {
        public String mNofityURI;
        public long mDownloadId;

        public OMADownloadHandlerForTest(Context context) {
            super(context);
            addObserverForTest(
                    downloadId -> {
                        mDownloadId = downloadId;
                    });
        }

        @Override
        protected boolean sendNotification(
                OMAInfo omaInfo, DownloadInfo downloadInfo, long downloadId, String statusMessage) {
            mNofityURI = omaInfo.getValue(OMA_INSTALL_NOTIFY_URI);
            return true;
        }
    }

    /** Helper class to verify the result of {@DownloadManagerDelegate.queryDownloadResult}. */
    private static class DownloadQueryResultVerifier implements Callback<DownloadQueryResult> {
        private int mExpectedDownloadStatus;

        public boolean mQueryCompleted;

        public DownloadQueryResultVerifier(int expectedDownloadStatus) {
            mExpectedDownloadStatus = expectedDownloadStatus;
        }

        @Override
        public void onResult(DownloadQueryResult result) {
            mQueryCompleted = true;
            Assert.assertEquals(mExpectedDownloadStatus, result.downloadStatus);
        }
    }

    private void waitForQueryCompletion(final DownloadQueryResultVerifier verifier) {
        CriteriaHelper.pollUiThread(() -> verifier.mQueryCompleted);
    }

    /** Test to make sure {@link OMADownloadHandler#getSize} returns the right size for OMAInfo. */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetSize() {
        OMADownloadHandler.OMAInfo info = new OMADownloadHandler.OMAInfo();
        Assert.assertEquals(OMADownloadHandler.getSize(info), 0);

        info.addAttributeValue("size", "100");
        Assert.assertEquals(OMADownloadHandler.getSize(info), 100);

        info.addAttributeValue("size", "100,000");
        Assert.assertEquals(OMADownloadHandler.getSize(info), 100000);

        info.addAttributeValue("size", "100000");
        Assert.assertEquals(OMADownloadHandler.getSize(info), 100000);
    }

    /**
     * Test to make sure {@link OMADownloadHandler.OMAInfo#getDrmType} returns the right DRM type.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetDrmType() {
        OMADownloadHandler.OMAInfo info = new OMADownloadHandler.OMAInfo();
        Assert.assertEquals(info.getDrmType(), null);

        info.addAttributeValue("type", "text/html");
        Assert.assertEquals(info.getDrmType(), null);

        info.addAttributeValue("type", MimeUtils.OMA_DRM_MESSAGE_MIME);
        Assert.assertEquals(info.getDrmType(), MimeUtils.OMA_DRM_MESSAGE_MIME);

        // Test that only the first DRM MIME type is returned.
        info.addAttributeValue("type", MimeUtils.OMA_DRM_CONTENT_MIME);
        Assert.assertEquals(info.getDrmType(), MimeUtils.OMA_DRM_MESSAGE_MIME);
    }

    /**
     * Test to make sure {@link OMADownloadHandler#getOpennableType} returns the right MIME type.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetOpennableType() {
        OMADownloadHandler.OMAInfo info = new OMADownloadHandler.OMAInfo();
        Assert.assertEquals(OMADownloadHandler.getOpennableType(info), null);

        info.addAttributeValue(OMADownloadHandler.OMA_TYPE, "application/octet-stream");
        info.addAttributeValue(OMADownloadHandler.OMA_TYPE, MimeUtils.OMA_DRM_MESSAGE_MIME);
        info.addAttributeValue(OMADownloadHandler.OMA_TYPE, "text/html");
        Assert.assertEquals(OMADownloadHandler.getOpennableType(info), null);

        info.addAttributeValue(OMADownloadHandler.OMA_OBJECT_URI, "http://www.test.com/test.html");
        Assert.assertEquals(OMADownloadHandler.getOpennableType(info), "text/html");

        // Test that only the first opennable type is returned.
        info.addAttributeValue(OMADownloadHandler.OMA_TYPE, "image/png");
        Assert.assertEquals(OMADownloadHandler.getOpennableType(info), "text/html");
    }

    /**
     * Test to make sure {@link OMADownloadHandler#parseDownloadDescriptor} returns the correct
     * OMAInfo if the input is valid.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testParseValidDownloadDescriptor() {
        String downloadDescriptor =
                "<media xmlns=\"http://www.openmobilealliance.org/xmlns/dd\">\r\n"
                        + "<DDVersion>1.0</DDVersion>\r\n"
                        + "<name>test.dm</name>\r\n"
                        + "<size>1,000</size>\r\n"
                        + "<type>image/jpeg</type>\r\n"
                        + "<garbage>this is just garbage</garbage>\r\n"
                        + "<type>application/vnd.oma.drm.message</type>\r\n"
                        + "<vendor>testvendor</vendor>\r\n"
                        + "<description>testjpg</description>\r\n"
                        + "<objectURI>http://test/test.dm</objectURI>\r\n"
                        + "<nextURL>http://nexturl.html</nextURL>\r\n"
                        + "</media>";
        OMADownloadHandler.OMAInfo info =
                OMADownloadHandler.parseDownloadDescriptor(
                        new ByteArrayInputStream(
                                ApiCompatibilityUtils.getBytesUtf8(downloadDescriptor)));
        Assert.assertFalse(info.isEmpty());
        Assert.assertEquals(
                info.getValue(OMADownloadHandler.OMA_OBJECT_URI), "http://test/test.dm");
        Assert.assertEquals(info.getValue(OMADownloadHandler.OMA_DD_VERSION), "1.0");
        Assert.assertEquals(info.getValue(OMADownloadHandler.OMA_NAME), "test.dm");
        Assert.assertEquals(info.getValue(OMADownloadHandler.OMA_SIZE), "1,000");
        Assert.assertEquals(info.getValue(OMADownloadHandler.OMA_VENDOR), "testvendor");
        Assert.assertEquals(info.getValue(OMADownloadHandler.OMA_DESCRIPTION), "testjpg");
        Assert.assertEquals(info.getValue(OMADownloadHandler.OMA_NEXT_URL), "http://nexturl.html");
        List<String> types = info.getTypes();
        assertThat(
                types, Matchers.containsInAnyOrder("image/jpeg", MimeUtils.OMA_DRM_MESSAGE_MIME));
    }

    /**
     * Test that {@link OMADownloadHandler#parseDownloadDescriptor} returns empty result on invalid
     * input.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testParseInvalidDownloadDescriptor() {
        String downloadDescriptor =
                "<media xmlns=\"http://www.openmobilealliance.org/xmlns/dd\">\r\n" + "</media>";
        OMADownloadHandler.OMAInfo info =
                OMADownloadHandler.parseDownloadDescriptor(
                        new ByteArrayInputStream(
                                ApiCompatibilityUtils.getBytesUtf8(downloadDescriptor)));
        Assert.assertTrue(info.isEmpty());

        downloadDescriptor =
                "<media xmlns=\"http://www.openmobilealliance.org/xmlns/dd\">\r\n"
                        + "<DDVersion>1.0</DDVersion>\r\n"
                        + "<name>"
                        + "<size>1,000</size>\r\n"
                        + "test.dm"
                        + "</name>\r\n"
                        + "</media>";
        info =
                OMADownloadHandler.parseDownloadDescriptor(
                        new ByteArrayInputStream(
                                ApiCompatibilityUtils.getBytesUtf8(downloadDescriptor)));
        Assert.assertNull(info);

        downloadDescriptor =
                "garbage"
                        + "<media xmlns=\"http://www.openmobilealliance.org/xmlns/dd\">\r\n"
                        + "<DDVersion>1.0</DDVersion>\r\n"
                        + "</media>";
        info =
                OMADownloadHandler.parseDownloadDescriptor(
                        new ByteArrayInputStream(
                                ApiCompatibilityUtils.getBytesUtf8(downloadDescriptor)));
        Assert.assertNull(info);
    }

    /**
     * Test to make sure {@link DownloadManagerBridge#queryDownloadResult} will report correctly
     * about the status of completed downloads and removed downloads.
     */
    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.Q) // https://crbug.com/338971643
    @Feature({"Download"})
    public void testQueryDownloadResult() {
        Context context = getTestContext();
        DownloadManager manager =
                (DownloadManager) getTestContext().getSystemService(Context.DOWNLOAD_SERVICE);
        long downloadId1 =
                manager.addCompletedDownload(
                        "test",
                        "test",
                        false,
                        "text/html",
                        UrlUtils.getIsolatedTestFilePath(
                                "chrome/test/data/android/download/download.txt"),
                        4,
                        true);

        DownloadQueryResultVerifier verifier =
                new DownloadQueryResultVerifier(DownloadStatus.COMPLETE);
        DownloadManagerBridge.queryDownloadResult(downloadId1, verifier);
        waitForQueryCompletion(verifier);

        manager.remove(downloadId1);
        verifier = new DownloadQueryResultVerifier(DownloadStatus.CANCELLED);
        DownloadManagerBridge.queryDownloadResult(downloadId1, verifier);
        waitForQueryCompletion(verifier);
    }

    /**
     * Test to make sure {@link OMADownloadHandler#clearPendingOMADownloads} will clear the OMA
     * notifications and pass the notification URI to {@link OMADownloadHandler}.
     */
    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.Q) // https://crbug.com/338971643
    @Feature({"Download"})
    public void testClearPendingOMADownloads() {
        Context context = getTestContext();
        DownloadManager manager =
                (DownloadManager) getTestContext().getSystemService(Context.DOWNLOAD_SERVICE);
        long downloadId1 =
                manager.addCompletedDownload(
                        "test",
                        "test",
                        false,
                        "text/html",
                        UrlUtils.getIsolatedTestFilePath(
                                "chrome/test/data/android/download/download.txt"),
                        4,
                        true);

        final OMADownloadHandlerForTest omaHandler = new OMADownloadHandlerForTest(context);

        // Write a few pending downloads into shared preferences.
        Set<String> pendingOmaDownloads = new HashSet<>();
        pendingOmaDownloads.add(String.valueOf(downloadId1) + "," + INSTALL_NOTIFY_URI);
        DownloadManagerService.storeDownloadInfo(
                ChromeSharedPreferences.getInstance(),
                ChromePreferenceKeys.DOWNLOAD_PENDING_OMA_DOWNLOADS,
                pendingOmaDownloads,
                /* forceCommit= */ false);

        pendingOmaDownloads =
                DownloadManagerService.getStoredDownloadInfo(
                        ChromeSharedPreferences.getInstance(),
                        ChromePreferenceKeys.DOWNLOAD_PENDING_OMA_DOWNLOADS);
        Assert.assertEquals(1, pendingOmaDownloads.size());

        omaHandler.clearPendingOMADownloads();

        // Wait for OMADownloadHandler to clear the pending downloads.
        CriteriaHelper.pollUiThread(
                () -> {
                    OfflineItem item = mTestInfoBarController.getLastUpdatedItem();
                    Criteria.checkThat(item, Matchers.notNullValue());
                    Criteria.checkThat(item.state, Matchers.is(OfflineItemState.COMPLETE));
                });

        // The pending downloads set in the shared prefs should be empty now.
        pendingOmaDownloads =
                DownloadManagerService.getStoredDownloadInfo(
                        ChromeSharedPreferences.getInstance(),
                        ChromePreferenceKeys.DOWNLOAD_PENDING_OMA_DOWNLOADS);
        Assert.assertEquals(0, pendingOmaDownloads.size());
        Assert.assertEquals(omaHandler.mNofityURI, INSTALL_NOTIFY_URI);

        manager.remove(downloadId1);
    }

    /**
     * Test that calling {@link OMADownloadHandler#enqueueDownloadManagerRequest} for an OMA
     * download will enqueue a new DownloadManager request and insert an entry into the SharedPrefs.
     */
    @Test
    @MediumTest
    @Feature({"Download"})
    public void testEnqueueOMADownloads() {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        Context context = getTestContext();

        OMADownloadHandler.OMAInfo omaInfo = new OMAInfo();
        omaInfo.addAttributeValue(OMADownloadHandler.OMA_NAME, "test.gzip");
        omaInfo.addAttributeValue(
                OMADownloadHandler.OMA_OBJECT_URI,
                testServer.getURL("/chrome/test/data/android/download/test.gzip"));
        omaInfo.addAttributeValue(OMADownloadHandler.OMA_INSTALL_NOTIFY_URI, INSTALL_NOTIFY_URI);

        DownloadInfo info = new DownloadInfo.Builder().build();
        final OMADownloadHandlerForTest omaHandler =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new OMADownloadHandlerForTest(context) {
                                @Override
                                public void onReceive(Context context, Intent intent) {
                                    // Ignore all the broadcasts.
                                }
                            };
                        });
        omaHandler.clearPendingOMADownloads();
        omaHandler.downloadOMAContent(0, info, omaInfo);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(omaHandler.mDownloadId, Matchers.not(0));
                    Criteria.checkThat(mTestInfoBarController.mDownloadStarted, Matchers.is(true));
                });
        Set<String> downloads =
                DownloadManagerService.getStoredDownloadInfo(
                        ChromeSharedPreferences.getInstance(),
                        ChromePreferenceKeys.DOWNLOAD_PENDING_OMA_DOWNLOADS);
        Assert.assertEquals(1, downloads.size());
        OMADownloadHandler.OMAEntry entry =
                OMADownloadHandler.OMAEntry.parseOMAEntry((String) downloads.toArray()[0]);
        Assert.assertEquals(entry.mDownloadId, omaHandler.mDownloadId);
        Assert.assertEquals(entry.mInstallNotifyURI, INSTALL_NOTIFY_URI);
        DownloadManager manager =
                (DownloadManager) getTestContext().getSystemService(Context.DOWNLOAD_SERVICE);
        manager.remove(omaHandler.mDownloadId);
    }
}
