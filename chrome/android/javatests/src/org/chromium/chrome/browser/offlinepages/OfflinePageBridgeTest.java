// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.net.Uri;
import android.os.Build.VERSION_CODES;
import android.util.Base64;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link OfflinePageBridge}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class OfflinePageBridgeTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";
    private static final int TIMEOUT_MS = 5000;
    private static final ClientId TEST_CLIENT_ID =
            new ClientId(OfflinePageBridge.DOWNLOAD_NAMESPACE, "1234");

    private OfflinePageBridge mOfflinePageBridge;
    private EmbeddedTestServer mTestServer;
    private String mTestPage;
    private Profile mProfile;

    private void initializeBridgeForProfile() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // Ensure we start in an offline state.
                    mOfflinePageBridge = OfflinePageBridge.getForProfile(mProfile);
                    if (mOfflinePageBridge == null
                            || mOfflinePageBridge.isOfflinePageModelLoaded()) {
                        semaphore.release();
                        return;
                    }
                    mOfflinePageBridge.addObserver(
                            new OfflinePageModelObserver() {
                                @Override
                                public void offlinePageModelLoaded() {
                                    semaphore.release();
                                    mOfflinePageBridge.removeObserver(this);
                                }
                            });
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private OfflinePageBridge getBridgeForProfileKey() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        AtomicReference<OfflinePageBridge> offlinePageBridgeRef = new AtomicReference<>();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    ProfileKey profileKey = mProfile.getProfileKey();
                    // Ensure we start in an offline state.
                    OfflinePageBridge offlinePageBridge =
                            OfflinePageBridge.getForProfileKey(profileKey);
                    offlinePageBridgeRef.set(offlinePageBridge);
                    if (offlinePageBridge == null || offlinePageBridge.isOfflinePageModelLoaded()) {
                        semaphore.release();
                        return;
                    }
                    offlinePageBridge.addObserver(
                            new OfflinePageModelObserver() {
                                @Override
                                public void offlinePageModelLoaded() {
                                    semaphore.release();
                                    offlinePageBridge.removeObserver(this);
                                }
                            });
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return offlinePageBridgeRef.get();
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Ensure we start in an offline state.
                    NetworkChangeNotifier.forceConnectivityState(false);
                    if (!NetworkChangeNotifier.isInitialized()) {
                        NetworkChangeNotifier.init();
                    }
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                });

        initializeBridgeForProfile();
        List<Long> ids = new ArrayList<>();
        for (OfflinePageItem page : OfflineTestUtil.getAllPages()) {
            ids.add(page.getOfflineId());
        }
        deletePages(ids);

        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    public void testProfileAndKeyMapToSameOfflinePageBridge() throws Exception {
        OfflinePageBridge offlinePageBridgeRetrievedByKey = getBridgeForProfileKey();
        Assert.assertSame(mOfflinePageBridge, offlinePageBridgeRetrievedByKey);
    }

    @Test
    @MediumTest
    public void testLoadOfflinePagesWhenEmpty() throws Exception {
        List<OfflinePageItem> offlinePages = OfflineTestUtil.getAllPages();
        Assert.assertEquals("Offline pages count incorrect.", 0, offlinePages.size());
    }

    @Test
    @MediumTest
    public void testAddOfflinePageAndLoad() throws Exception {
        sActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        List<OfflinePageItem> allPages = OfflineTestUtil.getAllPages();
        OfflinePageItem offlinePage = allPages.get(0);
        Assert.assertEquals("Offline pages count incorrect.", 1, allPages.size());
        Assert.assertEquals("Offline page item url incorrect.", mTestPage, offlinePage.getUrl());
    }

    @Test
    @MediumTest
    public void testGetPageByBookmarkId() throws Exception {
        sActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        OfflinePageItem offlinePage = OfflineTestUtil.getPageByClientId(TEST_CLIENT_ID);
        Assert.assertEquals("Offline page item url incorrect.", mTestPage, offlinePage.getUrl());
        Assert.assertNull(
                "Offline page is not supposed to exist",
                OfflineTestUtil.getPageByClientId(
                        new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "-42")));
    }

    @Test
    @MediumTest
    public void testDeleteOfflinePage() throws Exception {
        deletePage(TEST_CLIENT_ID, DeletePageResult.SUCCESS);
        sActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        Assert.assertNotNull(
                "Offline page should be available, but it is not.",
                OfflineTestUtil.getPageByClientId(TEST_CLIENT_ID));
        deletePage(TEST_CLIENT_ID, DeletePageResult.SUCCESS);
        Assert.assertNull(
                "Offline page should be gone, but it is available.",
                OfflineTestUtil.getPageByClientId(TEST_CLIENT_ID));
    }

    @Test
    @MediumTest
    public void testOfflinePageBridgeDisabled_InIncognitoTabbedActivity() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getPrimaryOTRProfile(/* createIfNeeded= */ true);
                });
        initializeBridgeForProfile();
        Assert.assertEquals(null, mOfflinePageBridge);
    }

    @Test
    @MediumTest
    public void testOfflinePageBridgeForProfileKeyDisabled_InIncognitoTabbedActivity()
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getPrimaryOTRProfile(/* createIfNeeded= */ true);
                });
        OfflinePageBridge offlinePageBridgeRetrievedByKey = getBridgeForProfileKey();
        Assert.assertNull(offlinePageBridgeRetrievedByKey);
    }

    @Test
    @MediumTest
    public void testOfflinePageBridgeDisabled_InIncognitoCCT() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OTRProfileID otrProfileID = OTRProfileID.createUnique("CCT:Incognito");
                    mProfile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileID, /* createIfNeeded= */ true);
                    Assert.assertTrue(mProfile.isOffTheRecord());
                    Assert.assertFalse(mProfile.isPrimaryOTRProfile());
                });
        initializeBridgeForProfile();
        Assert.assertEquals(null, mOfflinePageBridge);
    }

    @Test
    @MediumTest
    public void testOfflinePageBridgeForProfileKeyDisabled_InIncognitoCCT() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OTRProfileID otrProfileID = OTRProfileID.createUnique("CCT:Incognito");
                    mProfile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileID, /* createIfNeeded= */ true);
                    Assert.assertTrue(mProfile.isOffTheRecord());
                    Assert.assertFalse(mProfile.isPrimaryOTRProfile());
                });
        OfflinePageBridge offlinePageBridgeRetrievedByKey = getBridgeForProfileKey();
        Assert.assertNull(offlinePageBridgeRetrievedByKey);
    }

    @Test
    @MediumTest
    public void testDeletePagesByOfflineIds() throws Exception {
        // Save 3 pages and record their offline IDs to delete later.
        Set<String> pageUrls = new HashSet<>();
        pageUrls.add(mTestPage);
        pageUrls.add(mTestPage + "?foo=1");
        pageUrls.add(mTestPage + "?foo=2");
        int pagesToDeleteCount = pageUrls.size();
        List<Long> offlineIdsToDelete = new ArrayList<>();
        for (String url : pageUrls) {
            sActivityTestRule.loadUrl(url);
            offlineIdsToDelete.add(savePage(SavePageResult.SUCCESS, url));
        }
        Assert.assertEquals(
                "The pages should exist now that we saved them.",
                pagesToDeleteCount,
                getUrlsExistOfflineFromSet(pageUrls).size());

        // Save one more page but don't save the offline ID, this page should not be deleted.
        Set<String> pageUrlsToSave = new HashSet<>();
        String pageToSave = mTestPage + "?bar=1";
        pageUrlsToSave.add(pageToSave);
        int pagesToSaveCount = pageUrlsToSave.size();
        for (String url : pageUrlsToSave) {
            sActivityTestRule.loadUrl(url);
            savePage(SavePageResult.SUCCESS, pageToSave);
        }
        Assert.assertEquals(
                "The pages should exist now that we saved them.",
                pagesToSaveCount,
                getUrlsExistOfflineFromSet(pageUrlsToSave).size());

        // Delete the first 3 pages.
        deletePages(offlineIdsToDelete);
        Assert.assertEquals(
                "The page should cease to exist.", 0, getUrlsExistOfflineFromSet(pageUrls).size());

        // We should not have deleted the one we didn't ask to delete.
        Assert.assertEquals(
                "The page should not be deleted.",
                pagesToSaveCount,
                getUrlsExistOfflineFromSet(pageUrlsToSave).size());
    }

    @Test
    @MediumTest
    public void testGetPagesByNamespace() throws Exception {
        // Save 3 pages and record their offline IDs to delete later.
        Set<Long> offlineIdsToFetch = new HashSet<>();
        for (int i = 0; i < 3; i++) {
            String url = mTestPage + "?foo=" + i;
            sActivityTestRule.loadUrl(url);
            offlineIdsToFetch.add(savePage(SavePageResult.SUCCESS, url));
        }

        // Save a page in a different namespace.
        String urlToIgnore = mTestPage + "?bar=1";
        sActivityTestRule.loadUrl(urlToIgnore);
        long offlineIdToIgnore =
                savePage(
                        SavePageResult.SUCCESS,
                        urlToIgnore,
                        new ClientId(OfflinePageBridge.ASYNC_NAMESPACE, "-42"));

        List<OfflinePageItem> pages = getPagesByNamespace(OfflinePageBridge.DOWNLOAD_NAMESPACE);
        Assert.assertEquals(
                "The number of pages returned does not match the number of pages saved.",
                offlineIdsToFetch.size(),
                pages.size());
        for (OfflinePageItem page : pages) {
            offlineIdsToFetch.remove(page.getOfflineId());
        }
        Assert.assertEquals(
                "There were different pages saved than those returned by getPagesByNamespace.",
                0,
                offlineIdsToFetch.size());

        // Check that the page in the other namespace still exists.
        List<OfflinePageItem> asyncPages = getPagesByNamespace(OfflinePageBridge.ASYNC_NAMESPACE);
        Assert.assertEquals(
                "The page saved in an alternate namespace is no longer there.",
                1,
                asyncPages.size());
        Assert.assertEquals(
                "The offline ID of the page saved in an alternate namespace does not match.",
                offlineIdToIgnore,
                asyncPages.get(0).getOfflineId());
    }

    @Test
    @MediumTest
    public void testDownloadPage() throws Exception {
        final OfflinePageOrigin origin =
                new OfflinePageOrigin("abc.xyz", new String[] {"deadbeef"});
        sActivityTestRule.loadUrl(mTestPage);
        final String originString = origin.encodeAsJsonString();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNotNull(
                            "Tab is null", sActivityTestRule.getActivity().getActivityTab());
                    Assert.assertEquals(
                            "URL does not match requested.",
                            mTestPage,
                            sActivityTestRule.getActivity().getActivityTab().getUrl().getSpec());
                    Assert.assertNotNull("WebContents is null", sActivityTestRule.getWebContents());

                    mOfflinePageBridge.addObserver(
                            new OfflinePageModelObserver() {
                                @Override
                                public void offlinePageAdded(OfflinePageItem newPage) {
                                    mOfflinePageBridge.removeObserver(this);
                                    semaphore.release();
                                }
                            });

                    OfflinePageDownloadBridge.startDownload(
                            sActivityTestRule.getActivity().getActivityTab(), origin);
                });
        Assert.assertTrue(
                "Semaphore acquire failed. Timed out.",
                semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        List<OfflinePageItem> pages = OfflineTestUtil.getAllPages();
        Assert.assertEquals(originString, pages.get(0).getRequestOrigin());
    }

    @Test
    @MediumTest
    public void testSavePageWithRequestOrigin() throws Exception {
        final OfflinePageOrigin origin =
                new OfflinePageOrigin("abc.xyz", new String[] {"deadbeef"});
        sActivityTestRule.loadUrl(mTestPage);
        final String originString = origin.encodeAsJsonString();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mOfflinePageBridge.addObserver(
                            new OfflinePageModelObserver() {
                                @Override
                                public void offlinePageAdded(OfflinePageItem newPage) {
                                    mOfflinePageBridge.removeObserver(this);
                                    semaphore.release();
                                }
                            });
                    mOfflinePageBridge.savePage(
                            sActivityTestRule.getWebContents(),
                            TEST_CLIENT_ID,
                            origin,
                            new SavePageCallback() {
                                @Override
                                public void onSavePageDone(
                                        int savePageResult, String url, long offlineId) {}
                            });
                });

        Assert.assertTrue(
                "Semaphore acquire failed. Timed out.",
                semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        List<OfflinePageItem> pages = OfflineTestUtil.getAllPages();
        Assert.assertEquals(originString, pages.get(0).getRequestOrigin());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/842801")
    public void testSavePageNoOrigin() throws Exception {
        sActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        List<OfflinePageItem> pages = OfflineTestUtil.getAllPages();
        Assert.assertEquals("", pages.get(0).getRequestOrigin());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(
            value = VERSION_CODES.R,
            reason = "OfflinePage File Path is content uri on R+")
    // TODO: expand this test to match testGetLoadUrlParamsForOpeningMhtmlFileUrl_File.
    public void testGetLoadUrlParamsForOpeningMhtmlFileUrl_ContentUri() throws Exception {
        sActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        List<OfflinePageItem> allPages = OfflineTestUtil.getAllPages();
        Assert.assertEquals(1, allPages.size());
        OfflinePageItem offlinePage = allPages.get(0);

        // The content URL pointing to the archive file should be replaced with http/https URL of
        // the offline page.
        String contentUrl = offlinePage.getFilePath();
        LoadUrlParams loadUrlParams = getLoadUrlParamsForOpeningMhtmlFileOrContent(contentUrl);
        Assert.assertEquals(offlinePage.getUrl(), loadUrlParams.getUrl());
        String extraHeaders = loadUrlParams.getVerbatimHeaders();
        Assert.assertNotNull(extraHeaders);
        Assert.assertNotEquals(-1, extraHeaders.indexOf("reason=content_url_intent"));
        Assert.assertNotEquals(
                "intent_url field not found in header: " + extraHeaders,
                -1,
                extraHeaders.indexOf(
                        "intent_url="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(contentUrl),
                                        Base64.NO_WRAP)));
        Assert.assertNotEquals(
                -1, extraHeaders.indexOf("id=" + Long.toString(offlinePage.getOfflineId())));
    }

    @Test
    @MediumTest
    @MaxAndroidSdkLevel(
            value = VERSION_CODES.Q,
            reason = "OfflinePage File Path is content uri on R+")
    public void testGetLoadUrlParamsForOpeningMhtmlFileUrl_File() throws Exception {
        sActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        List<OfflinePageItem> allPages = OfflineTestUtil.getAllPages();
        Assert.assertEquals(1, allPages.size());
        OfflinePageItem offlinePage = allPages.get(0);
        File archiveFile = new File(offlinePage.getFilePath());

        // The file URL pointing to the archive file should be replaced with http/https URL of the
        // offline page.
        String fileUrl = Uri.fromFile(archiveFile).toString();
        LoadUrlParams loadUrlParams = getLoadUrlParamsForOpeningMhtmlFileOrContent(fileUrl);
        Assert.assertEquals(offlinePage.getUrl(), loadUrlParams.getUrl());
        String extraHeaders = loadUrlParams.getVerbatimHeaders();
        Assert.assertNotNull(extraHeaders);
        Assert.assertNotEquals(-1, extraHeaders.indexOf("reason=file_url_intent"));
        Assert.assertNotEquals(
                "intent_url field not found in header: " + extraHeaders,
                -1,
                extraHeaders.indexOf(
                        "intent_url="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(fileUrl),
                                        Base64.NO_WRAP)));
        Assert.assertNotEquals(
                -1, extraHeaders.indexOf("id=" + Long.toString(offlinePage.getOfflineId())));

        // Make a copy of the original archive file.
        File tempFile = File.createTempFile("Test", "");
        copyFile(archiveFile, tempFile);

        // The file URL pointing to file copy should also be replaced with http/https URL of the
        // offline page.
        String tempFileUrl = Uri.fromFile(tempFile).toString();
        loadUrlParams = getLoadUrlParamsForOpeningMhtmlFileOrContent(tempFileUrl);
        Assert.assertEquals(offlinePage.getUrl(), loadUrlParams.getUrl());
        extraHeaders = loadUrlParams.getVerbatimHeaders();
        Assert.assertNotNull(extraHeaders);
        Assert.assertNotEquals(
                "reason field not found in header: " + extraHeaders,
                -1,
                extraHeaders.indexOf("reason=file_url_intent"));
        Assert.assertNotEquals(
                "intent_url field not found in header: " + extraHeaders,
                -1,
                extraHeaders.indexOf(
                        "intent_url="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(tempFileUrl),
                                        Base64.NO_WRAP)));
        Assert.assertNotEquals(
                "id field not found in header: " + extraHeaders,
                -1,
                extraHeaders.indexOf("id=" + Long.toString(offlinePage.getOfflineId())));

        // Modify the copied file.
        FileChannel tempFileChannel = new FileOutputStream(tempFile, true).getChannel();
        tempFileChannel.truncate(10);
        tempFileChannel.close();

        // The file URL pointing to modified file copy should still get the file URL.
        loadUrlParams = getLoadUrlParamsForOpeningMhtmlFileOrContent(tempFileUrl);
        Assert.assertEquals(tempFileUrl, loadUrlParams.getUrl());
        extraHeaders = loadUrlParams.getVerbatimHeaders();
        Assert.assertNull(extraHeaders);

        // Cleans up.
        Assert.assertTrue(tempFile.delete());
    }

    // Returns offline ID.
    private long savePage(final int expectedResult, final String expectedUrl)
            throws InterruptedException {
        return savePage(expectedResult, expectedUrl, TEST_CLIENT_ID);
    }

    // Returns offline ID.
    private long savePage(
            final int expectedResult, final String expectedUrl, final ClientId clientId)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicLong result = new AtomicLong(-1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNotNull(
                            "Tab is null", sActivityTestRule.getActivity().getActivityTab());
                    Assert.assertEquals(
                            "URL does not match requested.",
                            expectedUrl,
                            sActivityTestRule.getActivity().getActivityTab().getUrl().getSpec());
                    Assert.assertNotNull("WebContents is null", sActivityTestRule.getWebContents());

                    mOfflinePageBridge.savePage(
                            sActivityTestRule.getWebContents(),
                            clientId,
                            new SavePageCallback() {
                                @Override
                                public void onSavePageDone(
                                        int savePageResult, String url, long offlineId) {
                                    Assert.assertEquals(
                                            "Requested and returned URLs differ.",
                                            expectedUrl,
                                            url);
                                    Assert.assertEquals(
                                            "Save result incorrect.",
                                            expectedResult,
                                            savePageResult);
                                    result.set(offlineId);
                                    semaphore.release();
                                }
                            });
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result.get();
    }

    private void deletePages(final List<Long> offlineIds) throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mOfflinePageBridge.deletePagesByOfflineId(
                            offlineIds,
                            new Callback<Integer>() {
                                @Override
                                public void onResult(Integer deletePageResult) {
                                    semaphore.release();
                                }
                            });
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private void deletePage(final ClientId bookmarkId, final int expectedResult)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicInteger deletePageResultRef = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mOfflinePageBridge.deletePage(
                            bookmarkId,
                            new Callback<Integer>() {
                                @Override
                                public void onResult(Integer deletePageResult) {
                                    deletePageResultRef.set(deletePageResult.intValue());
                                    semaphore.release();
                                }
                            });
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals("Delete result incorrect.", expectedResult, deletePageResultRef.get());
    }

    private List<OfflinePageItem> getPagesByNamespace(final String namespace)
            throws InterruptedException {
        final List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        final Semaphore semaphore = new Semaphore(0);
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mOfflinePageBridge.getPagesByNamespace(
                            namespace,
                            new Callback<List<OfflinePageItem>>() {
                                @Override
                                public void onResult(List<OfflinePageItem> pages) {
                                    result.addAll(pages);
                                    semaphore.release();
                                }
                            });
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result;
    }

    private void forceConnectivityStateOnUiThread(final boolean state) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkChangeNotifier.forceConnectivityState(state);
                });
    }

    private Set<String> getUrlsExistOfflineFromSet(final Set<String> query)
            throws TimeoutException {
        final Set<String> result = new HashSet<>();
        final List<OfflinePageItem> pages = OfflineTestUtil.getAllPages();
        for (String url : query) {
            for (OfflinePageItem page : pages) {
                if (url.equals(page.getUrl())) {
                    result.add(page.getUrl());
                }
            }
        }
        return result;
    }

    private LoadUrlParams getLoadUrlParamsForOpeningMhtmlFileOrContent(String url)
            throws InterruptedException {
        final AtomicReference<LoadUrlParams> ref = new AtomicReference<>();
        final Semaphore semaphore = new Semaphore(0);
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mOfflinePageBridge.getLoadUrlParamsForOpeningMhtmlFileOrContent(
                            url,
                            (loadUrlParams) -> {
                                ref.set(loadUrlParams);
                                semaphore.release();
                            });
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return ref.get();
    }

    private static void copyFile(File source, File dest) throws IOException {
        FileChannel inputChannel = null;
        FileChannel outputChannel = null;
        try {
            inputChannel = new FileInputStream(source).getChannel();
            outputChannel = new FileOutputStream(dest).getChannel();
            outputChannel.transferFrom(inputChannel, 0, inputChannel.size());
        } finally {
            inputChannel.close();
            outputChannel.close();
        }
    }
}
