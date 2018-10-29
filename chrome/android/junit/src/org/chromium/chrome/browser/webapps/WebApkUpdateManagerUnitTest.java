// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.webapk.lib.client.WebApkVersion.REQUEST_UPDATE_FOR_SHELL_APK_VERSION;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowBitmap;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.test.ShadowUrlUtilities;
import org.chromium.chrome.test.support.DisableHistogramsRule;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.io.File;
import java.io.FileOutputStream;
import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for WebApkUpdateManager.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class, ShadowUrlUtilities.class})
public class WebApkUpdateManagerUnitTest {
    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    @Rule
    public MockWebappDataStorageClockRule mClockRule = new MockWebappDataStorageClockRule();

    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String UNBOUND_WEBAPK_PACKAGE_NAME = "com.webapk.test_package";

    /** Web Manifest URL */
    private static final String WEB_MANIFEST_URL = "manifest.json";

    private static final String START_URL = "/start_url.html";
    private static final String SCOPE_URL = "/";
    private static final String NAME = "Long Name";
    private static final String SHORT_NAME = "Short Name";
    private static final String PRIMARY_ICON_URL = "/icon.png";
    private static final String PRIMARY_ICON_MURMUR2_HASH = "3";
    private static final String BADGE_ICON_URL = "/badge.png";
    private static final String BADGE_ICON_MURMUR2_HASH = "4";
    private static final @WebDisplayMode int DISPLAY_MODE = WebDisplayMode.UNDEFINED;
    private static final int ORIENTATION = ScreenOrientationValues.DEFAULT;
    private static final long THEME_COLOR = 1L;
    private static final long BACKGROUND_COLOR = 2L;

    /** Different name than the one used in {@link defaultManifestData()}. */
    private static final String DIFFERENT_NAME = "Different Name";

    /** Mock {@link WebApkUpdateDataFetcher}. */
    private static class TestWebApkUpdateDataFetcher extends WebApkUpdateDataFetcher {
        private boolean mStarted;

        public boolean wasStarted() {
            return mStarted;
        }

        @Override
        public boolean start(Tab tab, WebApkInfo oldInfo, Observer observer) {
            mStarted = true;
            return true;
        }
    }

    private static class TestWebApkUpdateManager extends WebApkUpdateManager {
        private Callback<Boolean> mStoreUpdateRequestCallback;
        private WebApkUpdateManager.WebApkUpdateCallback mUpdateCallback;
        private TestWebApkUpdateDataFetcher mFetcher;
        private String mUpdateName;
        private boolean mDestroyedFetcher;

        public TestWebApkUpdateManager(WebappDataStorage storage) {
            super(storage);
        }

        /**
         * Returns whether the is-update-needed check has been triggered.
         */
        public boolean updateCheckStarted() {
            return mFetcher != null && mFetcher.wasStarted();
        }

        /**
         * Returns whether an update has been requested.
         */
        public boolean updateRequested() {
            return mStoreUpdateRequestCallback != null;
        }

        /**
         * Returns the "name" from the requested update. Null if an update has not been requested.
         */
        public String requestedUpdateName() {
            return mUpdateName;
        }

        public boolean destroyedFetcher() {
            return mDestroyedFetcher;
        }

        public Callback<Boolean> getStoreUpdateRequestCallback() {
            return mStoreUpdateRequestCallback;
        }

        public WebApkUpdateManager.WebApkUpdateCallback getUpdateCallback() {
            return mUpdateCallback;
        }

        @Override
        protected WebApkUpdateDataFetcher buildFetcher() {
            mFetcher = new TestWebApkUpdateDataFetcher();
            return mFetcher;
        }

        @Override
        protected void storeWebApkUpdateRequestToFile(String updateRequestPath, WebApkInfo info,
                String primaryIconUrl, String badgeIconUrl, boolean isManifestStale,
                @WebApkUpdateReason int updateReason, Callback<Boolean> callback) {
            mStoreUpdateRequestCallback = callback;
            mUpdateName = info.name();
            writeRandomTextToFile(updateRequestPath);
        }

        @Override
        protected void updateWebApkFromFile(
                String updateRequestPath, WebApkUpdateCallback callback) {
            mUpdateCallback = callback;
        }

        @Override
        protected void destroyFetcher() {
            mFetcher = null;
            mDestroyedFetcher = true;
        }
    }

    private static class ManifestData {
        public String startUrl;
        public String scopeUrl;
        public String name;
        public String shortName;
        public Map<String, String> iconUrlToMurmur2HashMap;
        public String primaryIconUrl;
        public Bitmap primaryIcon;
        public String badgeIconUrl;
        public Bitmap badgeIcon;
        public @WebDisplayMode int displayMode;
        public int orientation;
        public long themeColor;
        public long backgroundColor;
    }

    private static String getWebApkId(String packageName) {
        return WebApkConstants.WEBAPK_ID_PREFIX + packageName;
    }

    private WebappDataStorage getStorage(String packageName) {
        return WebappRegistry.getInstance().getWebappDataStorage(getWebApkId(packageName));
    }

    /**
     * Registers WebAPK with default package name. Overwrites previous registrations.
     * @param packageName         Package name for which to register the WebApk.
     * @param manifestData        <meta-data> values for WebAPK's Android Manifest.
     * @param shellApkVersionCode WebAPK's version of the //chrome/android/webapk/shell_apk code.
     */
    private void registerWebApk(
            String packageName, ManifestData manifestData, int shellApkVersionCode) {
        Bundle metaData = new Bundle();
        metaData.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, shellApkVersionCode);
        metaData.putString(WebApkMetaDataKeys.START_URL, manifestData.startUrl);
        metaData.putString(WebApkMetaDataKeys.SCOPE, manifestData.scopeUrl);
        metaData.putString(WebApkMetaDataKeys.NAME, manifestData.name);
        metaData.putString(WebApkMetaDataKeys.SHORT_NAME, manifestData.shortName);
        metaData.putString(WebApkMetaDataKeys.THEME_COLOR, manifestData.themeColor + "L");
        metaData.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, manifestData.backgroundColor + "L");
        metaData.putString(WebApkMetaDataKeys.WEB_MANIFEST_URL, WEB_MANIFEST_URL);

        String iconUrlsAndIconMurmur2Hashes = "";
        for (Map.Entry<String, String> mapEntry : manifestData.iconUrlToMurmur2HashMap.entrySet()) {
            String murmur2Hash = mapEntry.getValue();
            if (murmur2Hash == null) {
                murmur2Hash = "0";
            }
            iconUrlsAndIconMurmur2Hashes += " " + mapEntry.getKey() + " " + murmur2Hash;
        }
        iconUrlsAndIconMurmur2Hashes = iconUrlsAndIconMurmur2Hashes.trim();
        metaData.putString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES, iconUrlsAndIconMurmur2Hashes);

        WebApkTestHelper.registerWebApkWithMetaData(packageName, metaData);
    }

    private static ManifestData defaultManifestData() {
        ManifestData manifestData = new ManifestData();
        manifestData.startUrl = START_URL;
        manifestData.scopeUrl = SCOPE_URL;
        manifestData.name = NAME;
        manifestData.shortName = SHORT_NAME;

        manifestData.iconUrlToMurmur2HashMap = new HashMap<>();
        manifestData.iconUrlToMurmur2HashMap.put(PRIMARY_ICON_URL, PRIMARY_ICON_MURMUR2_HASH);
        manifestData.iconUrlToMurmur2HashMap.put(BADGE_ICON_URL, BADGE_ICON_MURMUR2_HASH);

        manifestData.primaryIconUrl = PRIMARY_ICON_URL;
        manifestData.primaryIcon = createBitmap(Color.GREEN);
        manifestData.badgeIconUrl = BADGE_ICON_URL;
        manifestData.badgeIcon = createBitmap(Color.BLUE);
        manifestData.displayMode = DISPLAY_MODE;
        manifestData.orientation = ORIENTATION;
        manifestData.themeColor = THEME_COLOR;
        manifestData.backgroundColor = BACKGROUND_COLOR;
        return manifestData;
    }

    private static WebApkInfo infoFromManifestData(ManifestData manifestData) {
        if (manifestData == null) return null;

        final String kPackageName = "org.random.webapk";
        return WebApkInfo.create(getWebApkId(kPackageName), "", manifestData.scopeUrl,
                new WebApkInfo.Icon(manifestData.primaryIcon),
                new WebApkInfo.Icon(manifestData.badgeIcon), null, manifestData.name,
                manifestData.shortName, manifestData.displayMode, manifestData.orientation, -1,
                manifestData.themeColor, manifestData.backgroundColor, kPackageName, -1,
                WEB_MANIFEST_URL, manifestData.startUrl, WebApkInfo.WebApkDistributor.BROWSER,
                manifestData.iconUrlToMurmur2HashMap, null, false /* forceNavigation */,
                false /* useTransparentSplash */);
    }

    /**
     * Creates 1x1 bitmap.
     * @param color The bitmap color.
     */
    private static Bitmap createBitmap(int color) {
        int colors[] = {color};
        return ShadowBitmap.createBitmap(colors, 1, 1, Bitmap.Config.ALPHA_8);
    }

    private static void updateIfNeeded(WebApkUpdateManager updateManager) {
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
    }

    private static void updateIfNeeded(String packageName, WebApkUpdateManager updateManager) {
        // Use the intent version of {@link WebApkInfo#create()} in order to test default values
        // set by the intent version of {@link WebApkInfo#create()}.
        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_URL, "");
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, packageName);
        WebApkInfo info = WebApkInfo.create(intent);

        updateManager.updateIfNeeded(null, info);
    }

    private static void onGotUnchangedWebManifestData(WebApkUpdateManager updateManager) {
        onGotManifestData(updateManager, defaultManifestData());
    }

    private static void onGotDifferentData(WebApkUpdateManager updateManager) {
        ManifestData manifestData = defaultManifestData();
        manifestData.name = DIFFERENT_NAME;
        onGotManifestData(updateManager, manifestData);
    }

    private static void onGotManifestData(
            WebApkUpdateManager updateManager, ManifestData fetchedManifestData) {
        String primaryIconUrl = randomIconUrl(fetchedManifestData);
        String badgeIconUrl = randomIconUrl(fetchedManifestData);
        updateManager.onGotManifestData(
                infoFromManifestData(fetchedManifestData), primaryIconUrl, badgeIconUrl);
    }

    /**
     * Tries to complete update request.
     * @param updateManager
     * @param result The result of the update task. Emulates the proto creation as always
     *               succeeding.
     */
    private static void tryCompletingUpdate(
            TestWebApkUpdateManager updateManager, @WebApkInstallResult int result) {
        // Emulate proto creation as always succeeding.
        Callback<Boolean> storeUpdateRequestCallback =
                updateManager.getStoreUpdateRequestCallback();
        if (storeUpdateRequestCallback == null) return;

        storeUpdateRequestCallback.onResult(true);

        updateManager.updateWhileNotRunning(Mockito.mock(Runnable.class));
        WebApkUpdateManager.WebApkUpdateCallback updateCallback = updateManager.getUpdateCallback();
        if (updateCallback == null) return;

        updateCallback.onResultFromNative(result, false /* relaxUpdates */);
        ShadowApplication.getInstance().runBackgroundTasks();
    }

    private static void writeRandomTextToFile(String path) {
        File file = new File(path);
        new File(file.getParent()).mkdirs();
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(ApiCompatibilityUtils.getBytesUtf8("something"));
        } catch (Exception e) {
        }
    }

    private static String randomIconUrl(ManifestData fetchedManifestData) {
        if (fetchedManifestData == null || fetchedManifestData.iconUrlToMurmur2HashMap.isEmpty()) {
            return null;
        }
        return fetchedManifestData.iconUrlToMurmur2HashMap.keySet().iterator().next();
    }

    /**
     * Checks whether the WebAPK is updated given data from the WebAPK's Android Manifest and data
     * from the fetched Web Manifest.
     */
    private boolean checkUpdateNeededForFetchedManifest(
            ManifestData androidManifestData, ManifestData fetchedManifestData) {
        registerWebApk(
                WEBAPK_PACKAGE_NAME, androidManifestData, REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        updateManager.onGotManifestData(infoFromManifestData(fetchedManifestData),
                fetchedManifestData.primaryIconUrl, fetchedManifestData.badgeIconUrl);
        return updateManager.updateRequested();
    }

    @Before
    public void setUp() {
        PathUtils.setPrivateDataDirectorySuffix("chrome");
        CommandLine.init(null);

        registerWebApk(
                WEBAPK_PACKAGE_NAME, defaultManifestData(), REQUEST_UPDATE_FOR_SHELL_APK_VERSION);

        WebappRegistry.getInstance().register(getWebApkId(WEBAPK_PACKAGE_NAME),
                new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {}
                });
        ShadowApplication.getInstance().runBackgroundTasks();

        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        storage.updateTimeOfLastWebApkUpdateRequestCompletion();
        storage.updateDidLastWebApkUpdateRequestSucceed(true);
    }

    /**
     * Test that the is-update-needed check is tried the next time that the WebAPK is launched if
     * Chrome is killed prior to the initial URL finishing loading.
     */
    @Test
    public void testCheckOnNextLaunchIfClosePriorToFirstPageLoad() {
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);
        {
            TestWebApkUpdateManager updateManager =
                    new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
            updateIfNeeded(updateManager);
            assertTrue(updateManager.updateCheckStarted());
        }

        // Chrome is killed. {@link WebApkUpdateManager#OnGotManifestData()} is not called.

        {
            // Relaunching the WebAPK should do an is-update-needed check.
            TestWebApkUpdateManager updateManager =
                    new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
            updateIfNeeded(updateManager);
            assertTrue(updateManager.updateCheckStarted());
            onGotUnchangedWebManifestData(updateManager);
        }

        {
            // Relaunching the WebAPK should not do an is-update-needed-check.
            TestWebApkUpdateManager updateManager =
                    new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
            updateIfNeeded(updateManager);
            assertFalse(updateManager.updateCheckStarted());
        }
    }

    /**
     * Test that the completion time of the previous WebAPK update is not modified if:
     * - The previous WebAPK update succeeded.
     * AND
     * - A WebAPK update is not required.
     */
    @Test
    public void testUpdateNotNeeded() {
        long initialTime = mClockRule.currentTimeMillis();
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotUnchangedWebManifestData(updateManager);
        assertFalse(updateManager.updateRequested());

        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        assertTrue(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(initialTime, storage.getLastWebApkUpdateRequestCompletionTimeMs());
    }

    /**
     * Test that the last WebAPK update is marked as having succeeded if:
     * - The previous WebAPK update failed.
     * AND
     * - A WebAPK update is no longer required.
     */
    @Test
    public void testMarkUpdateAsSucceededIfUpdateNoLongerNeeded() {
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        storage.updateDidLastWebApkUpdateRequestSucceed(false);
        mClockRule.advance(WebappDataStorage.RETRY_UPDATE_DURATION);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotUnchangedWebManifestData(updateManager);
        assertFalse(updateManager.updateRequested());

        assertTrue(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(mClockRule.currentTimeMillis(),
                storage.getLastWebApkUpdateRequestCompletionTimeMs());
    }

    /**
     * Test that the WebAPK update is marked as having failed if Chrome is killed prior to the
     * WebAPK update completing.
     */
    @Test
    public void testMarkUpdateAsFailedIfClosePriorToUpdateCompleting() {
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());

        // Chrome is killed. {@link WebApkUpdateCallback#onResultFromNative} is never called.

        // Check {@link WebappDataStorage} state.
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        assertFalse(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(mClockRule.currentTimeMillis(),
                storage.getLastWebApkUpdateRequestCompletionTimeMs());
    }

    /**
     * Test that the pending update file is deleted after update completes regardless of whether
     * update succeeded.
     */
    @Test
    public void testPendingUpdateFileDeletedAfterUpdateCompletion() {
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(storage);
        updateIfNeeded(updateManager);

        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());
        String updateRequestPath = storage.getPendingUpdateRequestPath();
        assertNotNull(updateRequestPath);
        assertTrue(new File(updateRequestPath).exists());

        tryCompletingUpdate(updateManager, WebApkInstallResult.FAILURE);

        assertNull(storage.getPendingUpdateRequestPath());
        assertFalse(new File(updateRequestPath).exists());
    }

    /**
     * Test that the pending update file is deleted if
     * {@link WebApkUpdateManager#nativeStoreWebApkUpdateRequestToFile} creates the pending update
     * file but fails.
     */
    @Test
    public void testFileDeletedIfStoreWebApkUpdateRequestToFileFails() {
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(storage);
        updateIfNeeded(updateManager);

        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());
        String updateRequestPath = storage.getPendingUpdateRequestPath();
        assertNotNull(updateRequestPath);
        assertTrue(new File(updateRequestPath).exists());

        updateManager.getStoreUpdateRequestCallback().onResult(false);
        ShadowApplication.getInstance().runBackgroundTasks();

        assertNull(storage.getPendingUpdateRequestPath());
        assertFalse(new File(updateRequestPath).exists());
    }

    /**
     * Test that an update with data from the WebAPK's Android manifest is done if:
     * - WebAPK's code is out of date
     * AND
     * - WebAPK's start_url does not refer to a Web Manifest.
     *
     * It is good to minimize the number of users with out of date WebAPKs. We try to keep WebAPKs
     * up to date even if the web developer has removed the Web Manifest from their site.
     */
    @Test
    public void testShellApkOutOfDateNoWebManifest() {
        registerWebApk(WEBAPK_PACKAGE_NAME, defaultManifestData(),
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION - 1);
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        updateManager.onGotManifestData(null, null, null);
        assertTrue(updateManager.updateRequested());
        assertEquals(NAME, updateManager.requestedUpdateName());

        // Check that the {@link WebApkUpdateDataFetcher} has been destroyed. This prevents
        // {@link #onGotManifestData()} from getting called.
        assertTrue(updateManager.destroyedFetcher());
    }

    /**
     * Test that an update with data from the fetched Web Manifest is done if the WebAPK's code is
     * out of date and the WebAPK's start_url refers to a Web Manifest.
     */
    @Test
    public void testShellApkOutOfDateStillHasWebManifest() {
        registerWebApk(WEBAPK_PACKAGE_NAME, defaultManifestData(),
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION - 1);
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        onGotManifestData(updateManager, defaultManifestData());
        assertTrue(updateManager.updateRequested());
        assertEquals(NAME, updateManager.requestedUpdateName());

        assertTrue(updateManager.destroyedFetcher());
    }

    /**
     * Test that an update is requested if:
     * - start_url does not refer to a Web Manifest.
     * AND
     * - The user eventually navigates to a page pointing to a Web Manifest with the correct URL.
     * AND
     * - The Web Manifest has changed.
     */
    @Test
    public void testStartUrlRedirectsToPageWithUpdatedWebManifest() {
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());

        // start_url does not have a Web Manifest. {@link #onGotManifestData} is called as a result
        // of update manager timing out. No update should be requested.
        updateManager.onGotManifestData(null, null, null);
        assertFalse(updateManager.updateRequested());
        // {@link WebApkUpdateDataFetcher} should still be alive so that it can get
        // {@link #onGotManifestData} when page with the Web Manifest finishes loading.
        assertFalse(updateManager.destroyedFetcher());

        // User eventually navigates to page with Web Manifest.

        ManifestData manifestData = defaultManifestData();
        manifestData.name = DIFFERENT_NAME;
        onGotManifestData(updateManager, manifestData);
        assertTrue(updateManager.updateRequested());
        assertEquals(DIFFERENT_NAME, updateManager.requestedUpdateName());

        assertTrue(updateManager.destroyedFetcher());
    }

    /**
     * Test that an update is not requested if:
     * - start_url does not refer to a Web Manifest.
     * AND
     * - The user eventually navigates to a page pointing to a Web Manifest with the correct URL.
     * AND
     * - The Web Manifest has not changed.
     */
    @Test
    public void testStartUrlRedirectsToPageWithUnchangedWebManifest() {
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));
        updateIfNeeded(updateManager);

        // Update manager times out.
        updateManager.onGotManifestData(null, null, null);
        onGotManifestData(updateManager, defaultManifestData());
        assertFalse(updateManager.updateRequested());

        // We got the Web Manifest. The {@link WebApkUpdateDataFetcher} should be destroyed to stop
        // it from fetching the Web Manifest for subsequent page loads.
        assertTrue(updateManager.destroyedFetcher());
    }

    @Test
    public void testManifestDoesNotUpgrade() {
        assertFalse(
                checkUpdateNeededForFetchedManifest(defaultManifestData(), defaultManifestData()));
    }

    /**
     * Test that a webapk with an unexpected package name does not request updates.
     */
    @Test
    public void testUnboundWebApkDoesNotUpgrade() {
        ManifestData androidManifestData = defaultManifestData();

        registerWebApk(UNBOUND_WEBAPK_PACKAGE_NAME, androidManifestData,
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(UNBOUND_WEBAPK_PACKAGE_NAME));
        updateIfNeeded(UNBOUND_WEBAPK_PACKAGE_NAME, updateManager);
        assertFalse(updateManager.updateCheckStarted());
        assertFalse(updateManager.updateRequested());
    }

    /**
     * Test that an upgrade is not requested when the Web Manifest did not change and the Web
     * Manifest scope is empty.
     */
    @Test
    public void testManifestEmptyScopeShouldNotUpgrade() {
        ManifestData oldData = defaultManifestData();
        // webapk_installer.cc sets the scope to the default scope if the scope is empty.
        oldData.scopeUrl = ShortcutHelper.getScopeFromUrl(oldData.startUrl);
        ManifestData fetchedData = defaultManifestData();
        fetchedData.scopeUrl = "";
        assertTrue(!oldData.scopeUrl.equals(fetchedData.scopeUrl));
        assertFalse(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /**
     * Test that an upgrade is requested when the Web Manifest is updated from using a non-empty
     * scope to an empty scope.
     */
    @Test
    public void testManifestNonEmptyScopeToEmptyScopeShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        oldData.startUrl = "/fancy/scope/special/snowflake.html";
        oldData.scopeUrl = "/fancy/scope/";
        assertTrue(!oldData.scopeUrl.equals(ShortcutHelper.getScopeFromUrl(oldData.startUrl)));
        ManifestData fetchedData = defaultManifestData();
        fetchedData.startUrl = "/fancy/scope/special/snowflake.html";
        fetchedData.scopeUrl = "";

        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK was generated using icon at {@link PRIMARY_ICON_URL} from Web Manifest.
     * - Bitmap at {@link PRIMARY_ICON_URL} has changed.
     */
    @Test
    public void testPrimaryIconChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put(
                fetchedData.primaryIconUrl, PRIMARY_ICON_MURMUR2_HASH + "1");
        fetchedData.primaryIcon = createBitmap(Color.BLUE);
        assertTrue(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK was generated using icon at {@link BADGE_ICON_URL} from Web Manifest.
     * - Bitmap at {@link BADGE_ICON_URL} has changed.
     */
    @Test
    public void testBadgeIconChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put(
                fetchedData.badgeIconUrl, BADGE_ICON_MURMUR2_HASH + "1");
        fetchedData.badgeIcon = createBitmap(Color.GREEN);
        assertTrue(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK is generated using icon at {@link PRIMARY_ICON_URL} from Web Manifest.
     * - A new icon URL is added to the Web Manifest. And InstallableManager selects the new icon as
     *   the primary icon.
     */
    @Test
    public void testPrimaryIconUrlChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put("/icon2.png", "22");
        fetchedData.primaryIconUrl = "/icon2.png";
        assertTrue(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK is generated using icon at {@link BADGE_ICON_URL} from Web Manifest.
     * - A new icon URL is added to the Web Manifest. And InstallableManager selects the new icon as
     *   the badge icon.
     */
    @Test
    public void testBadgeIconUrlChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put("/badge2.png", "44");
        fetchedData.badgeIconUrl = "/badge2.png";
        assertTrue(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
    }

    /**
     * Test that an upgrade is not requested if:
     * - icon URL is added to the Web Manifest
     * AND
     * - "best" icon URL for the primary icon did not change.
     * AND
     * - "best" icon URL for the badge icon did not change.
     */
    @Test
    public void testIconUrlsChangeShouldNotUpgradeIfPrimaryIconUrlAndBadgeIconUrlDoNotChange() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put("/icon2.png", null);
        assertFalse(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
    }

    /**
     * Test that an upgrade is not requested if:
     * - the WebAPK's meta data has murmur2 hashes for all of the icons.
     * AND
     * - the Web Manifest has not changed
     * AND
     * - the computed best icon URLs are different from the one stored in the WebAPK's meta data.
     */
    @Test
    public void testWebManifestSameButBestIconUrlChangedShouldNotUpgrade() {
        String iconUrl1 = "/icon1.png";
        String iconUrl2 = "/icon2.png";
        String badgeUrl1 = "/badge1.png";
        String badgeUrl2 = "/badge2.pgn";
        String hash1 = "11";
        String hash2 = "22";
        String hash3 = "33";
        String hash4 = "44";

        ManifestData androidManifestData = defaultManifestData();
        androidManifestData.primaryIconUrl = iconUrl1;
        androidManifestData.badgeIconUrl = badgeUrl1;
        androidManifestData.iconUrlToMurmur2HashMap.clear();
        androidManifestData.iconUrlToMurmur2HashMap.put(iconUrl1, hash1);
        androidManifestData.iconUrlToMurmur2HashMap.put(iconUrl2, hash2);
        androidManifestData.iconUrlToMurmur2HashMap.put(badgeUrl1, hash3);
        androidManifestData.iconUrlToMurmur2HashMap.put(badgeUrl2, hash4);

        ManifestData fetchedManifestData = defaultManifestData();
        fetchedManifestData.primaryIconUrl = iconUrl2;
        fetchedManifestData.badgeIconUrl = badgeUrl2;
        fetchedManifestData.iconUrlToMurmur2HashMap.clear();
        fetchedManifestData.iconUrlToMurmur2HashMap.put(iconUrl1, null);
        fetchedManifestData.iconUrlToMurmur2HashMap.put(iconUrl2, hash2);
        fetchedManifestData.iconUrlToMurmur2HashMap.put(badgeUrl1, null);
        fetchedManifestData.iconUrlToMurmur2HashMap.put(badgeUrl2, hash4);

        assertFalse(checkUpdateNeededForFetchedManifest(androidManifestData, fetchedManifestData));
    }

    /**
     * Tests that a WebAPK update is requested immediately if:
     * the Shell APK is out of date,
     * AND
     * there wasn't a previous request for this ShellAPK version.
     */
    @Test
    public void testShellApkOutOfDate() {
        registerWebApk(WEBAPK_PACKAGE_NAME, defaultManifestData(),
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION - 1);
        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(getStorage(WEBAPK_PACKAGE_NAME));

        // There have not been any update requests for the current ShellAPK version. A WebAPK update
        // should be requested immediately.
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotManifestData(updateManager, defaultManifestData());
        assertTrue(updateManager.updateRequested());
        tryCompletingUpdate(updateManager, WebApkInstallResult.FAILURE);

        mClockRule.advance(1);
        updateIfNeeded(updateManager);
        assertFalse(updateManager.updateCheckStarted());

        // A previous update request was made for the current ShellAPK version. A WebAPK update
        // should be requested after the regular delay.
        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL - 1);
        updateIfNeeded(updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotManifestData(updateManager, defaultManifestData());
        assertTrue(updateManager.updateRequested());
    }
}
