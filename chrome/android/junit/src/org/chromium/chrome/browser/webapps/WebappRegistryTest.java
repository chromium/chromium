// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browsing_data.UrlFilters;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.webapps.WebappRegistry.GetWebApkSpecificsImplSetWebappInfoForTesting;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.sync.protocol.WebApkSpecifics;
import org.chromium.ui.util.ColorUtils;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Tests the WebappRegistry class by ensuring that it persists data to
 * SharedPreferences as expected.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {BackgroundShadowAsyncTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class WebappRegistryTest {
    // These were copied from WebappRegistry for backward compatibility checking.
    private static final String REGISTRY_FILE_NAME = "webapp_registry";
    private static final String KEY_WEBAPP_SET = "webapp_set";
    private static final String KEY_LAST_CLEANUP = "last_cleanup";

    private static final String START_URL = "https://foo.com";

    private static final int INITIAL_TIME = 0;

    private SharedPreferences mSharedPreferences;
    private boolean mCallbackCalled;

    @Rule public JniMocker mJniMocker = new JniMocker();

    private static class FetchStorageCallback
            implements WebappRegistry.FetchWebappDataStorageCallback {
        BrowserServicesIntentDataProvider mIntentDataProvider;
        boolean mCallbackCalled;

        FetchStorageCallback(BrowserServicesIntentDataProvider intentDataProvider) {
            mIntentDataProvider = intentDataProvider;
        }

        @Override
        public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
            mCallbackCalled = true;
            if (mIntentDataProvider != null) {
                storage.updateFromWebappIntentDataProvider(mIntentDataProvider);
            }
        }

        boolean getCallbackCalled() {
            return mCallbackCalled;
        }
    }

    private static class TestWebApkSyncServiceJni implements WebApkSyncService.Natives {
        @Override
        public void onWebApkUsed(byte[] webApkSpecifics, boolean isInstall) {}

        @Override
        public void onWebApkUninstalled(String manifestId) {}

        @Override
        public void removeOldWebAPKsFromSync(long currentTimeMsSinceUnixEpoch) {}

        @Override
        public void fetchRestorableApps(
                Profile profile, WebApkSyncService.PwaRestorableListCallback callback) {}
    }

    @Before
    public void setUp() {
        WebappRegistry.refreshSharedPrefsForTesting();
        mSharedPreferences =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
        mSharedPreferences.edit().putLong(KEY_LAST_CLEANUP, INITIAL_TIME).commit();

        mCallbackCalled = false;

        mJniMocker.mock(WebApkSyncServiceJni.TEST_HOOKS, new TestWebApkSyncServiceJni());
    }

    private void registerWebapp(BrowserServicesIntentDataProvider intentDataProvider)
            throws Exception {
        registerWebappWithId(intentDataProvider.getWebappExtras().id, intentDataProvider);
    }

    private void registerWebappWithId(
            String webappId, BrowserServicesIntentDataProvider intentDataProvider)
            throws Exception {
        FetchStorageCallback callback = new FetchStorageCallback(intentDataProvider);
        WebappRegistry.getInstance().register(webappId, callback);

        // Run background tasks to make sure the data is committed. Run UI thread tasks to make sure
        // the last used time is updated.
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(callback.getCallbackCalled());
    }

    @Test
    @Feature({"Webapp"})
    public void testBackwardCompatibility() {
        assertEquals(REGISTRY_FILE_NAME, WebappRegistry.REGISTRY_FILE_NAME);
        assertEquals(KEY_WEBAPP_SET, WebappRegistry.KEY_WEBAPP_SET);
        assertEquals(KEY_LAST_CLEANUP, WebappRegistry.KEY_LAST_CLEANUP);
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappRegistrationAddsToSharedPrefs() throws Exception {
        registerWebappWithId("test", null);
        Set<String> actual = getRegisteredWebapps();
        assertEquals(1, actual.size());
        assertTrue(actual.contains("test"));
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappRegistrationUpdatesLastUsed() throws Exception {
        registerWebappWithId("test", null);

        long after = System.currentTimeMillis();
        SharedPreferences webAppPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "test",
                                Context.MODE_PRIVATE);
        long actual =
                webAppPrefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertTrue("Timestamp is out of range", actual <= after);
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappIdsRetrieval() {
        final Set<String> expected = addWebappsToRegistry("first", "second");
        assertEquals(expected, WebappRegistry.getRegisteredWebappIdsForTesting());
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappIdsRetrievalRegisterRetrival() throws Exception {
        final Set<String> expected = addWebappsToRegistry("first");
        assertEquals(expected, WebappRegistry.getRegisteredWebappIdsForTesting());

        // Force a re-read of the preferences from disk. Add a new web app via the registry.
        WebappRegistry.refreshSharedPrefsForTesting();
        registerWebappWithId("second", null);

        // A copy of the expected set needs to be made as the SharedPreferences is using the copy
        // that was passed to it.
        final Set<String> secondExpected = new HashSet<>(expected);
        secondExpected.add("second");
        assertEquals(secondExpected, WebappRegistry.getRegisteredWebappIdsForTesting());
    }

    /**
     * Test behaviour when there is a webapp with a null id registered. See crbug.com/1055566
     * for details of the bug which caused this to occur.
     */
    @Test
    @Feature({"Webapp"})
    public void testWebappNullId() throws Exception {
        addWebappsToRegistry(new String[] {null});
        registerWebappWithId(null, createShortcutIntentDataProvider("https://www.google.ca"));
        assertEquals(1, WebappRegistry.getRegisteredWebappIdsForTesting().size());

        WebappRegistry.refreshSharedPrefsForTesting();

        // Does not crash.
        assertEquals(
                null,
                WebappRegistry.getInstance().getWebappDataStorageForUrl("https://www.google.ca/"));

        long currentTime = System.currentTimeMillis();
        WebappRegistry.getInstance()
                .unregisterOldWebapps(currentTime + WebappRegistry.FULL_CLEANUP_DURATION);
        assertTrue(WebappRegistry.getRegisteredWebappIdsForTesting().isEmpty());
    }

    @Test
    @Feature({"Webapp"})
    public void testUnregisterClearsRegistry() throws Exception {
        Map<String, String> apps = new HashMap<>();
        apps.put("webapp1", "http://example.com/index.html");
        apps.put("webapp2", "https://www.google.com/foo/bar");
        apps.put("webapp3", "https://www.chrome.com");

        for (Map.Entry<String, String> app : apps.entrySet()) {
            registerWebappWithId(app.getKey(), createShortcutIntentDataProvider(app.getValue()));
        }

        // Partial deletion.
        WebappRegistry.getInstance()
                .unregisterWebappsForUrlsImpl(
                        new UrlFilters.OneUrl("http://example.com/index.html"));

        Set<String> registeredWebapps = getRegisteredWebapps();
        assertEquals(2, registeredWebapps.size());
        for (String appName : apps.keySet()) {
            assertEquals(
                    !TextUtils.equals(appName, "webapp1"), registeredWebapps.contains(appName));
        }

        // Full deletion.
        WebappRegistry.getInstance().unregisterWebappsForUrlsImpl(new UrlFilters.AllUrls());
        assertTrue(getRegisteredWebapps().isEmpty());
    }

    @Test
    @Feature({"Webapp"})
    public void testUnregisterClearsWebappDataStorage() throws Exception {
        Map<String, String> apps = new HashMap<>();
        apps.put("webapp1", "http://example.com/index.html");
        apps.put("webapp2", "https://www.google.com/foo/bar");
        apps.put("webapp3", "https://www.chrome.com");

        for (Map.Entry<String, String> app : apps.entrySet()) {
            registerWebappWithId(app.getKey(), createShortcutIntentDataProvider(app.getValue()));
        }

        for (String appName : apps.keySet()) {
            SharedPreferences webAppPrefs =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(
                                    WebappDataStorage.SHARED_PREFS_FILE_PREFIX + appName,
                                    Context.MODE_PRIVATE);
            webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, 100L).apply();
        }

        // Partial deletion.
        WebappRegistry.getInstance()
                .unregisterWebappsForUrlsImpl(
                        new UrlFilters.OneUrl("http://example.com/index.html"));

        for (String appName : apps.keySet()) {
            SharedPreferences webAppPrefs =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(
                                    WebappDataStorage.SHARED_PREFS_FILE_PREFIX + appName,
                                    Context.MODE_PRIVATE);
            assertEquals(TextUtils.equals(appName, "webapp1"), webAppPrefs.getAll().isEmpty());
        }

        // Full deletion.
        WebappRegistry.getInstance().unregisterWebappsForUrlsImpl(new UrlFilters.AllUrls());
        for (String appName : apps.keySet()) {
            SharedPreferences webAppPrefs =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(
                                    WebappDataStorage.SHARED_PREFS_FILE_PREFIX + appName,
                                    Context.MODE_PRIVATE);
            assertTrue(webAppPrefs.getAll().isEmpty());
        }
    }

    @Test
    @Feature({"Webapp"})
    public void testCleanupDoesNotRunTooOften() {
        // Put the current time to just before the task should run.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION - 1;

        addWebappsToRegistry("oldWebapp");
        SharedPreferences webAppPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "oldWebapp",
                                Context.MODE_PRIVATE);
        webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, Long.MIN_VALUE).apply();

        // Force a re-read of the preferences from disk.
        WebappRegistry.refreshSharedPrefsForTesting();
        WebappRegistry.getInstance().unregisterOldWebapps(currentTime);

        Set<String> actual = getRegisteredWebapps();
        assertEquals(new HashSet<>(Arrays.asList("oldWebapp")), actual);

        long actualLastUsed =
                webAppPrefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(Long.MIN_VALUE, actualLastUsed);

        // The last cleanup time was set to 0 in setUp() so check that this hasn't changed.
        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(INITIAL_TIME, lastCleanup);
    }

    @Test
    @Feature({"Webapp"})
    public void testCleanupDoesNotRemoveRecentApps() {
        // Put the current time such that the task runs.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION;

        // Put the last used time just inside the no-cleanup window.
        addWebappsToRegistry("recentWebapp");
        SharedPreferences webAppPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "recentWebapp",
                                Context.MODE_PRIVATE);
        long lastUsed = currentTime - WebappRegistry.WEBAPP_UNOPENED_CLEANUP_DURATION + 1;
        webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, lastUsed).apply();

        // Force a re-read of the preferences from disk.
        WebappRegistry.refreshSharedPrefsForTesting();

        // Because the time is just inside the window, there should be a cleanup but the web app
        // should not be deleted as it was used recently. The last cleanup time should also be
        // set to the current time.
        WebappRegistry.getInstance().unregisterOldWebapps(currentTime);

        Set<String> actual = getRegisteredWebapps();
        assertEquals(new HashSet<>(Arrays.asList("recentWebapp")), actual);

        long actualLastUsed =
                webAppPrefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(lastUsed, actualLastUsed);

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"Webapp"})
    public void testCleanupRemovesOldApps() {
        // Put the current time such that the task runs.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION;

        // Put the last used time just outside the no-cleanup window.
        addWebappsToRegistry("oldWebapp");
        SharedPreferences webAppPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "oldWebapp",
                                Context.MODE_PRIVATE);
        long lastUsed = currentTime - WebappRegistry.WEBAPP_UNOPENED_CLEANUP_DURATION;
        webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, lastUsed).apply();

        // Force a re-read of the preferences from disk.
        WebappRegistry.refreshSharedPrefsForTesting();

        // Because the time is just inside the window, there should be a cleanup of old web apps and
        // the last cleaned up time should be set to the current time.
        WebappRegistry.getInstance().unregisterOldWebapps(currentTime);

        Set<String> actual = getRegisteredWebapps();
        assertTrue(actual.isEmpty());

        long actualLastUsed =
                webAppPrefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(WebappDataStorage.TIMESTAMP_INVALID, actualLastUsed);

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"WebApk"})
    public void testCleanupRemovesUninstalledWebApks() throws Exception {
        String webApkPackage1 = "uninstalledWebApk1";
        String webApkPackage2 = "uninstalledWebApk2";

        BrowserServicesIntentDataProvider intentDataProvider1 =
                new WebApkIntentDataProviderBuilder(webApkPackage1, START_URL).build();
        registerWebapp(intentDataProvider1);

        BrowserServicesIntentDataProvider intentDataProvider2 =
                new WebApkIntentDataProviderBuilder(webApkPackage2, START_URL).build();
        registerWebapp(intentDataProvider2);

        // Verify that both WebAPKs are registered.
        assertEquals(2, getRegisteredWebapps().size());
        assertTrue(isRegisteredWebapp(intentDataProvider1));
        assertTrue(isRegisteredWebapp(intentDataProvider2));

        // Set the current time such that the task runs.
        long currentTime = System.currentTimeMillis() + WebappRegistry.FULL_CLEANUP_DURATION;
        // Because the time is just inside the window, there should be a cleanup of
        // uninstalled WebAPKs and the last cleaned up time should be set to the
        // current time.
        WebappRegistry.getInstance().unregisterOldWebapps(currentTime);

        assertTrue(getRegisteredWebapps().isEmpty());

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"WebApk"})
    public void testCleanupDoesNotRemoveInstalledWebApks() throws Exception {
        String webApkPackage = "installedWebApk";
        String uninstalledWebApkPackage = "uninstalledWebApk";

        BrowserServicesIntentDataProvider webApkIntentDataProvider =
                new WebApkIntentDataProviderBuilder(webApkPackage, START_URL).build();
        registerWebapp(webApkIntentDataProvider);

        BrowserServicesIntentDataProvider uninstalledWebApkIntentDataProvider =
                new WebApkIntentDataProviderBuilder(uninstalledWebApkPackage, START_URL).build();
        registerWebapp(uninstalledWebApkIntentDataProvider);

        // Verify that both WebAPKs are registered.
        assertEquals(2, getRegisteredWebapps().size());
        assertTrue(isRegisteredWebapp(webApkIntentDataProvider));
        assertTrue(isRegisteredWebapp(uninstalledWebApkIntentDataProvider));

        Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager())
                .addPackage(webApkPackage);

        // Set the current time such that the task runs.
        long currentTime = System.currentTimeMillis() + WebappRegistry.FULL_CLEANUP_DURATION;
        // Because the time is just inside the window, there should be a cleanup of
        // uninstalled WebAPKs and the last cleaned up time should be set to the
        // current time.
        WebappRegistry.getInstance().unregisterOldWebapps(currentTime);

        assertEquals(1, getRegisteredWebapps().size());
        assertTrue(isRegisteredWebapp(webApkIntentDataProvider));

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"WebApk"})
    public void testCleanupDoesRemoveOldInstalledWebApks() throws Exception {
        String deprecatedWebApkIdPrefix = "webapk:";
        String webApkPackage = "installedWebApk";
        BrowserServicesIntentDataProvider webApkIntentDataProvider =
                new WebApkIntentDataProviderBuilder(webApkPackage, START_URL).build();
        String deprecatedWebApkId =
                deprecatedWebApkIdPrefix
                        + webApkIntentDataProvider.getWebApkExtras().webApkPackageName;

        registerWebappWithId(deprecatedWebApkId, webApkIntentDataProvider);
        registerWebapp(webApkIntentDataProvider);

        // Verify that both WebAPKs are registered.
        Set<String> actual = getRegisteredWebapps();
        assertEquals(2, actual.size());
        assertTrue(actual.contains(deprecatedWebApkId));
        assertTrue(actual.contains(webApkIntentDataProvider.getWebappExtras().id));

        Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager())
                .addPackage(webApkPackage);

        // Set the current time such that the task runs.
        long currentTime = System.currentTimeMillis() + WebappRegistry.FULL_CLEANUP_DURATION;
        // Because the time is just inside the window, there should be a cleanup of
        // uninstalled WebAPKs and the last cleaned up time should be set to the
        // current time.
        WebappRegistry.getInstance().unregisterOldWebapps(currentTime);

        actual = getRegisteredWebapps();
        assertEquals(1, actual.size());
        assertTrue(actual.contains(webApkIntentDataProvider.getWebappExtras().id));

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"Webapp"})
    public void testClearWebappHistory() throws Exception {
        final String webapp1Url = "https://www.google.com";
        final String webapp2Url = "https://drive.google.com";
        BrowserServicesIntentDataProvider webappIntentDataProvider1 =
                createShortcutIntentDataProvider(webapp1Url);
        BrowserServicesIntentDataProvider webappIntentDataProvider2 =
                createShortcutIntentDataProvider(webapp2Url);

        registerWebappWithId("webapp1", webappIntentDataProvider1);
        registerWebappWithId("webapp2", webappIntentDataProvider2);

        SharedPreferences webapp1Prefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp1",
                                Context.MODE_PRIVATE);
        SharedPreferences webapp2Prefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp2",
                                Context.MODE_PRIVATE);

        long webapp1OriginalLastUsed =
                webapp2Prefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        long webapp2OriginalLastUsed =
                webapp2Prefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertTrue(webapp1OriginalLastUsed != WebappDataStorage.TIMESTAMP_INVALID);
        assertTrue(webapp2OriginalLastUsed != WebappDataStorage.TIMESTAMP_INVALID);

        // Clear data for |webapp1Url|.
        WebappRegistry.getInstance()
                .clearWebappHistoryForUrlsImpl(new UrlFilters.OneUrl(webapp1Url));

        Set<String> actual = getRegisteredWebapps();
        assertEquals(2, actual.size());
        assertTrue(actual.contains("webapp1"));
        assertTrue(actual.contains("webapp2"));

        // Verify that the last used time for the first web app is
        // WebappDataStorage.TIMESTAMP_INVALID, while for the second one it's unchanged.
        long actualLastUsed =
                webapp1Prefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(WebappDataStorage.TIMESTAMP_INVALID, actualLastUsed);
        actualLastUsed =
                webapp2Prefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(webapp2OriginalLastUsed, actualLastUsed);

        // Verify that the URL and scope for the first web app is WebappDataStorage.URL_INVALID,
        // while for the second one it's unchanged.
        String actualScope =
                webapp1Prefs.getString(WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualScope);
        String actualUrl =
                webapp1Prefs.getString(WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualUrl);
        actualScope =
                webapp2Prefs.getString(WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(webapp2Url + "/", actualScope);
        actualUrl =
                webapp2Prefs.getString(WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(webapp2Url, actualUrl);

        // Clear data for all urls.
        WebappRegistry.getInstance().clearWebappHistoryForUrlsImpl(new UrlFilters.AllUrls());

        // Verify that the last used time for both web apps is WebappDataStorage.TIMESTAMP_INVALID.
        actualLastUsed =
                webapp1Prefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(WebappDataStorage.TIMESTAMP_INVALID, actualLastUsed);
        actualLastUsed =
                webapp2Prefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(WebappDataStorage.TIMESTAMP_INVALID, actualLastUsed);

        // Verify that the URL and scope for both web apps is WebappDataStorage.URL_INVALID.
        actualScope =
                webapp1Prefs.getString(WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualScope);
        actualUrl =
                webapp1Prefs.getString(WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualUrl);
        actualScope =
                webapp2Prefs.getString(WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualScope);
        actualUrl =
                webapp2Prefs.getString(WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualUrl);
    }

    @Test
    @Feature({"Webapp"})
    public void testGetAfterClearWebappHistory() throws Exception {
        registerWebappWithId("webapp", null);

        SharedPreferences webappPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp",
                                Context.MODE_PRIVATE);
        WebappRegistry.getInstance().clearWebappHistoryForUrlsImpl(new UrlFilters.AllUrls());

        // Open the webapp up and set the last used time.
        WebappRegistry.getInstance().getWebappDataStorage("webapp").updateLastUsedTime();

        // Verify that the last used time is valid.
        long actualLastUsed =
                webappPrefs.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertTrue(WebappDataStorage.TIMESTAMP_INVALID != actualLastUsed);
    }

    @Test
    @Feature({"Webapp"})
    public void testUpdateAfterClearWebappHistory() throws Exception {
        final String webappUrl = "http://www.google.com";
        final String webappScope = "http://www.google.com/";
        final BrowserServicesIntentDataProvider webappIntentDataProvider =
                createShortcutIntentDataProvider(webappUrl);
        registerWebappWithId("webapp", webappIntentDataProvider);

        SharedPreferences webappPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp",
                                Context.MODE_PRIVATE);

        // Verify that the URL and scope match the original in the intent.
        String actualUrl =
                webappPrefs.getString(WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(webappUrl, actualUrl);
        String actualScope =
                webappPrefs.getString(WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(webappScope, actualScope);

        WebappRegistry.getInstance().clearWebappHistoryForUrlsImpl(new UrlFilters.AllUrls());

        // Update the webapp from the intent again.
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage("webapp");
        storage.updateFromWebappIntentDataProvider(webappIntentDataProvider);

        // Verify that the URL and scope match the original in the intent.
        actualUrl = webappPrefs.getString(WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(webappUrl, actualUrl);
        actualScope =
                webappPrefs.getString(WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(webappScope, actualScope);
    }

    @Test
    @Feature({"Webapp"})
    public void testGetWebappDataStorageForUrl() throws Exception {
        // Ensure that getWebappDataStorageForUrl returns the correct WebappDataStorage object.
        // URLs should return the WebappDataStorage with the longest scope that the URL starts with.
        final String webapp1Url = "https://www.google.com/";
        final String webapp2Url = "https://drive.google.com/";
        final String webapp3Url = "https://www.google.com/drive/index.html";
        final String webapp4Url = "https://www.google.com/drive/docs/index.html";

        final String webapp3Scope = "https://www.google.com/drive/";
        final String webapp4Scope = "https://www.google.com/drive/docs/";

        final String test1Url = "https://www.google.com/index.html";
        final String test2Url = "https://www.google.com/drive/recent.html";
        final String test3Url = "https://www.google.com/drive/docs/recent.html";
        final String test4Url = "https://www.google.com/drive/docs/recent/index.html";
        final String test5Url = "https://drive.google.com/docs/recent/trash";
        final String test6Url = "https://maps.google.com/";

        BrowserServicesIntentDataProvider intentDataProvider1 =
                createShortcutIntentDataProvider(webapp1Url);
        BrowserServicesIntentDataProvider intentDataProvider2 =
                createShortcutIntentDataProvider(webapp2Url);
        BrowserServicesIntentDataProvider intentDataProvider3 =
                createShortcutIntentDataProvider(webapp3Url);
        BrowserServicesIntentDataProvider intentDataProvider4 =
                createShortcutIntentDataProvider(webapp4Url);

        // Register the four web apps.
        registerWebappWithId("webapp1", intentDataProvider1);
        registerWebappWithId("webapp2", intentDataProvider2);
        registerWebappWithId("webapp3", intentDataProvider3);
        registerWebappWithId("webapp4", intentDataProvider4);

        // test1Url should return webapp1.
        WebappDataStorage storage1 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(test1Url);
        assertEquals(webapp1Url, storage1.getUrl());
        assertEquals(webapp1Url, storage1.getScope());

        // test2Url should return webapp3.
        WebappDataStorage storage2 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(test2Url);
        assertEquals(webapp3Url, storage2.getUrl());
        assertEquals(webapp3Scope, storage2.getScope());

        // test3Url should return webapp4.
        WebappDataStorage storage3 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(test3Url);
        assertEquals(webapp4Url, storage3.getUrl());
        assertEquals(webapp4Scope, storage3.getScope());

        // test4Url should return webapp4.
        WebappDataStorage storage4 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(test4Url);
        assertEquals(webapp4Url, storage4.getUrl());
        assertEquals(webapp4Scope, storage4.getScope());

        // test5Url should return webapp2.
        WebappDataStorage storage5 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(test5Url);
        assertEquals(webapp2Url, storage5.getUrl());
        assertEquals(webapp2Url, storage5.getScope());

        // test6Url doesn't correspond to a web app, so the storage returned is null.
        WebappDataStorage storage6 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(test6Url);
        assertEquals(null, storage6);
    }

    @Test
    @Feature({"WebApk"})
    public void testGetWebappDataStorageForUrlWithWebApk() throws Exception {
        final String startUrl = START_URL;
        final String testUrl = START_URL + "/index.html";

        BrowserServicesIntentDataProvider webApkIntentDataProvider =
                new WebApkIntentDataProviderBuilder("org.chromium.webapk", startUrl).build();
        registerWebapp(webApkIntentDataProvider);

        // testUrl should return null.
        WebappDataStorage storage1 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(testUrl);
        assertNull(storage1);

        String webappId = "webapp";
        registerWebappWithId(webappId, createShortcutIntentDataProvider(startUrl));

        // testUrl should return the webapp.
        WebappDataStorage storage2 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(testUrl);
        assertEquals(webappId, storage2.getId());
    }

    @Test
    @Feature({"WebApk"})
    public void testHasWebApkForOrigin() throws Exception {
        final String startUrl = START_URL + "/test_page.html";
        final String testOrigin = START_URL;
        final String testPackageName = "org.chromium.webapk";

        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin(testOrigin));

        String webappId = "webapp";
        registerWebappWithId(webappId, createShortcutIntentDataProvider(startUrl));
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin(testOrigin));

        BrowserServicesIntentDataProvider webApkIntentDataProvider =
                new WebApkIntentDataProviderBuilder(testPackageName, startUrl).build();
        registerWebapp(webApkIntentDataProvider);
        // Still fails because the WebAPK is "no longer installed" according to PackageManager.
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin(testOrigin));

        Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager())
                .addPackage(testPackageName);
        assertTrue(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin(testOrigin));
    }

    @Test
    @Feature({"WebApk"})
    public void testFindWebApkWithManifestId() throws Exception {
        final String testManifestId = START_URL + "/id";
        final String testPackageName = "org.chromium.webapk";

        assertNull(WebappRegistry.getInstance().getWebappDataStorageForManifestId(testManifestId));

        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(testPackageName, START_URL)
                        .setWebApkManifestId(testManifestId)
                        .build();
        registerWebapp(intentDataProvider);

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorageForManifestId(testManifestId);
        assertNotNull(storage);
        assertEquals(storage.getWebApkManifestId(), testManifestId);
        assertEquals(storage.getWebApkPackageName(), testPackageName);

        final String anotherManifestId = START_URL + "/test_page.html";
        assertNull(
                WebappRegistry.getInstance().getWebappDataStorageForManifestId(anotherManifestId));
    }

    @Test
    @Feature({"WebApk"})
    public void testFindWebApkPackageWithManifestId() throws Exception {
        final String testManifestId = START_URL + "/id";
        final String testPackageName = "org.chromium.webapk";

        assertNull(WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));

        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(testPackageName, START_URL)
                        .setWebApkManifestId(testManifestId)
                        .build();
        registerWebapp(intentDataProvider);

        assertEquals(
                WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId),
                testPackageName);

        final String anotherManifestId = START_URL + "/test_page.html";
        assertNull(
                WebappRegistry.getInstance().getWebappDataStorageForManifestId(anotherManifestId));
    }

    @Test
    @Feature({"WebApk"})
    public void testGetWebApkSyncDatas() throws Exception {
        final String testStartUrl1 = START_URL;
        final String testManifestId1 = testStartUrl1 + "/id";
        final String testPackageName1 = "org.chromium.webapk";
        final String testName1 = "My App";
        final String testShortName1 = "app";
        final long testToolbarColor1 = Color.WHITE;
        final String testScope1 = testStartUrl1;

        final String testStartUrl2 = START_URL + "/2";
        final String testManifestId2 = testStartUrl2 + "/id";
        final String testPackageName2 = "org.chromium.webapk2";
        final String testName2 = null;
        final String testShortName2 = "app2";
        final long testToolbarColor2 = Color.BLACK;
        final String testScope2 = testStartUrl2;

        final String testStartUrl3 = START_URL + "/3";
        final String testManifestId3 = null;
        final String testPackageName3 = "org.chromium.webapk3";
        final String testName3 = "My App3";
        final String testShortName3 = "";
        final long testToolbarColor3 = ColorUtils.INVALID_COLOR;
        final String testScope3 = testStartUrl3;

        final String testStartUrl4 = START_URL + "/4";
        final String testManifestId4 = testStartUrl4 + "/id";
        final String testPackageName4 = "org.chromium.webapk4";
        final String testName4 = "My App4";
        final String testShortName4 = "app4";
        final long testToolbarColor4 = ColorUtils.INVALID_COLOR;
        final String testScope4 = null;

        WebappRegistry webApkRegistry = WebappRegistry.getInstance();
        Map<String, BrowserServicesIntentDataProvider> expectedIntentDataProviders =
                new HashMap<String, BrowserServicesIntentDataProvider>();

        BrowserServicesIntentDataProvider intentDataProvider1 =
                new WebApkIntentDataProviderBuilder(testPackageName1, testStartUrl1)
                        .setWebApkManifestId(testManifestId1)
                        .setName(testName1)
                        .setShortName(testShortName1)
                        .setToolbarColor(testToolbarColor1)
                        .setScope(testScope1)
                        .build();
        expectedIntentDataProviders.put(testScope1, intentDataProvider1);

        BrowserServicesIntentDataProvider intentDataProvider2 =
                new WebApkIntentDataProviderBuilder(testPackageName2, testStartUrl2)
                        .setWebApkManifestId(testManifestId2)
                        .setName(testName2)
                        .setShortName(testShortName2)
                        .setToolbarColor(testToolbarColor2)
                        .setScope(testScope2)
                        .build();
        expectedIntentDataProviders.put(testScope2, intentDataProvider2);

        // This one will not be returned because it has no manifest id.
        BrowserServicesIntentDataProvider intentDataProvider3 =
                new WebApkIntentDataProviderBuilder(testPackageName3, testStartUrl3)
                        .setWebApkManifestId(testManifestId3)
                        .setName(testName3)
                        .setShortName(testShortName3)
                        .setToolbarColor(testToolbarColor3)
                        .setScope(testScope3)
                        .build();
        expectedIntentDataProviders.put(testScope3, intentDataProvider3);

        // This one will not be returned because it has no scope.
        BrowserServicesIntentDataProvider intentDataProvider4 =
                new WebApkIntentDataProviderBuilder(testPackageName4, testStartUrl4)
                        .setWebApkManifestId(testManifestId4)
                        .setName(testName4)
                        .setShortName(testShortName4)
                        .setToolbarColor(testToolbarColor4)
                        .setScope(testScope4)
                        .build();

        GetWebApkSpecificsImplSetWebappInfoForTesting setWebappInfoForTesting =
                (scope) -> {
                    WebApkDataProvider.setWebappInfoForTesting(
                            WebappInfo.create(expectedIntentDataProviders.get(scope)));
                };

        assertEquals(0, webApkRegistry.getWebApkSpecificsImpl(setWebappInfoForTesting).size());

        registerWebapp(intentDataProvider1);
        registerWebapp(intentDataProvider2);
        registerWebapp(intentDataProvider3);
        registerWebapp(intentDataProvider4);

        List<WebApkSpecifics> webApkSpecificsList =
                webApkRegistry.getWebApkSpecificsImpl(setWebappInfoForTesting);
        assertEquals(2, webApkSpecificsList.size());

        Set<String> visitedScopes = new HashSet<String>();
        for (WebApkSpecifics webApkSpecifics : webApkSpecificsList) {
            BrowserServicesIntentDataProvider intentDataProvider =
                    expectedIntentDataProviders.get(webApkSpecifics.getScope());
            WebApkExtras webApkExtras = intentDataProvider.getWebApkExtras();
            WebappExtras webappExtras = intentDataProvider.getWebappExtras();
            ColorProvider colorProvider = intentDataProvider.getColorProvider();

            assertEquals(webApkExtras.manifestId, webApkSpecifics.getManifestId());
            assertEquals(webApkExtras.manifestStartUrl, webApkSpecifics.getStartUrl());

            if (webappExtras.name != null && !webappExtras.name.equals("")) {
                assertTrue(webApkSpecifics.hasName());
                assertEquals(webappExtras.name, webApkSpecifics.getName());
            } else if (webappExtras.shortName != null) {
                assertTrue(webApkSpecifics.hasName());
                assertEquals(webappExtras.shortName, webApkSpecifics.getName());
            } else {
                assertFalse(webApkSpecifics.hasName());
            }

            if (colorProvider.hasCustomToolbarColor()) {
                assertTrue(webApkSpecifics.hasThemeColor());
                assertEquals(colorProvider.getToolbarColor(), webApkSpecifics.getThemeColor());
            } else {
                assertFalse(webApkSpecifics.hasThemeColor());
            }

            assertEquals(webappExtras.scopeUrl, webApkSpecifics.getScope());

            visitedScopes.add(webApkSpecifics.getScope());
        }

        assertEquals(2, visitedScopes.size());
    }

    private Set<String> addWebappsToRegistry(String... webapps) {
        final Set<String> expected = new HashSet<>(Arrays.asList(webapps));
        mSharedPreferences.edit().putStringSet(WebappRegistry.KEY_WEBAPP_SET, expected).apply();
        return expected;
    }

    private boolean isRegisteredWebapp(BrowserServicesIntentDataProvider webappIntentDataProvider) {
        String id = webappIntentDataProvider.getWebappExtras().id;
        return getRegisteredWebapps().contains(id);
    }

    private Set<String> getRegisteredWebapps() {
        return mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
    }

    private BrowserServicesIntentDataProvider createShortcutIntentDataProvider(final String url) {
        return WebappIntentDataProviderFactory.create(
                ShortcutHelper.createWebappShortcutIntentForTesting("id", url));
    }
}
