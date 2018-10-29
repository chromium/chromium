// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.text.TextUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browsing_data.UrlFilters;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Tests the WebappRegistry class by ensuring that it persists data to
 * SharedPreferences as expected.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {BackgroundShadowAsyncTask.class})
public class WebappRegistryTest {
    // These were copied from WebappRegistry for backward compatibility checking.
    private static final String REGISTRY_FILE_NAME = "webapp_registry";
    private static final String KEY_WEBAPP_SET = "webapp_set";
    private static final String KEY_LAST_CLEANUP = "last_cleanup";

    private static final String START_URL = "https://foo.com";

    private static final int INITIAL_TIME = 0;

    private SharedPreferences mSharedPreferences;
    private boolean mCallbackCalled;

    private static class FetchStorageCallback
            implements WebappRegistry.FetchWebappDataStorageCallback {
        Intent mShortcutIntent;
        boolean mCallbackCalled;

        FetchStorageCallback(Intent shortcutIntent) {
            mCallbackCalled = false;
            mShortcutIntent = shortcutIntent;
        }

        @Override
        public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
            mCallbackCalled = true;
            storage.updateFromShortcutIntent(mShortcutIntent);
        }

        boolean getCallbackCalled() {
            return mCallbackCalled;
        }
    }

    @Before
    public void setUp() throws Exception {
        WebappRegistry.refreshSharedPrefsForTesting();
        mSharedPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
        mSharedPreferences.edit().putLong(KEY_LAST_CLEANUP, INITIAL_TIME).commit();

        mCallbackCalled = false;
    }

    @After
    public void tearDown() {
        mSharedPreferences.edit().clear().apply();
    }

    private void registerWebapp(String webappId,
            WebappRegistry.FetchWebappDataStorageCallback callback) throws Exception {
        WebappRegistry.getInstance().register(webappId, callback);

        // Run background tasks to make sure the data is committed. Run UI thread tasks to make sure
        // the last used time is updated.
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
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
        registerWebapp("test", null);
        Set<String> actual = getRegisteredWebapps();
        assertEquals(1, actual.size());
        assertTrue(actual.contains("test"));
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappRegistrationUpdatesLastUsed() throws Exception {
        registerWebapp("test", null);

        long after = System.currentTimeMillis();
        SharedPreferences webAppPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "test", Context.MODE_PRIVATE);
        long actual = webAppPrefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertTrue("Timestamp is out of range", actual <= after);
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappIdsRetrieval() throws Exception {
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
        registerWebapp("second", null);

        // A copy of the expected set needs to be made as the SharedPreferences is using the copy
        // that was passed to it.
        final Set<String> secondExpected = new HashSet<>(expected);
        secondExpected.add("second");
        assertEquals(secondExpected, WebappRegistry.getRegisteredWebappIdsForTesting());
    }

    @Test
    @Feature({"Webapp"})
    public void testUnregisterClearsRegistry() throws Exception {
        Map<String, String> apps = new HashMap<>();
        apps.put("webapp1", "http://example.com/index.html");
        apps.put("webapp2", "https://www.google.com/foo/bar");
        apps.put("webapp3", "https://www.chrome.com");

        for (Map.Entry<String, String> app : apps.entrySet()) {
            registerWebapp(
                    app.getKey(), new FetchStorageCallback(createShortcutIntent(app.getValue())));
        }

        // Partial deletion.
        WebappRegistry.getInstance().unregisterWebappsForUrlsImpl(
                new UrlFilters.OneUrl("http://example.com/index.html"));

        Set<String> registeredWebapps = getRegisteredWebapps();
        assertEquals(2, registeredWebapps.size());
        for (String appName : apps.keySet()) {
            assertEquals(!TextUtils.equals(appName, "webapp1"),
                         registeredWebapps.contains(appName));
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
            registerWebapp(
                    app.getKey(), new FetchStorageCallback(createShortcutIntent(app.getValue())));
        }

        for (String appName : apps.keySet()) {
            SharedPreferences webAppPrefs =
                    ContextUtils.getApplicationContext().getSharedPreferences(
                            WebappDataStorage.SHARED_PREFS_FILE_PREFIX + appName,
                            Context.MODE_PRIVATE);
            webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, 100L).apply();
        }

        // Partial deletion.
        WebappRegistry.getInstance().unregisterWebappsForUrlsImpl(
                new UrlFilters.OneUrl("http://example.com/index.html"));

        for (String appName : apps.keySet()) {
            SharedPreferences webAppPrefs =
                    ContextUtils.getApplicationContext().getSharedPreferences(
                            WebappDataStorage.SHARED_PREFS_FILE_PREFIX + appName,
                            Context.MODE_PRIVATE);
            assertEquals(TextUtils.equals(appName, "webapp1"), webAppPrefs.getAll().isEmpty());
        }

        // Full deletion.
        WebappRegistry.getInstance().unregisterWebappsForUrlsImpl(new UrlFilters.AllUrls());
        for (String appName : apps.keySet()) {
            SharedPreferences webAppPrefs =
                    ContextUtils.getApplicationContext().getSharedPreferences(
                            WebappDataStorage.SHARED_PREFS_FILE_PREFIX + appName,
                            Context.MODE_PRIVATE);
            assertTrue(webAppPrefs.getAll().isEmpty());
        }
    }

    @Test
    @Feature({"Webapp"})
      public void testCleanupDoesNotRunTooOften() throws Exception {
        // Put the current time to just before the task should run.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION - 1;

        addWebappsToRegistry("oldWebapp");
        SharedPreferences webAppPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "oldWebapp", Context.MODE_PRIVATE);
        webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, Long.MIN_VALUE).apply();

        // Force a re-read of the preferences from disk.
        WebappRegistry.refreshSharedPrefsForTesting();
        WebappRegistry.getInstance().unregisterOldWebapps(currentTime);

        Set<String> actual = getRegisteredWebapps();
        assertEquals(new HashSet<>(Arrays.asList("oldWebapp")), actual);

        long actualLastUsed = webAppPrefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(Long.MIN_VALUE, actualLastUsed);

        // The last cleanup time was set to 0 in setUp() so check that this hasn't changed.
        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(INITIAL_TIME, lastCleanup);
    }

    @Test
    @Feature({"Webapp"})
    public void testCleanupDoesNotRemoveRecentApps() throws Exception {
        // Put the current time such that the task runs.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION;

        // Put the last used time just inside the no-cleanup window.
        addWebappsToRegistry("recentWebapp");
        SharedPreferences webAppPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "recentWebapp", Context.MODE_PRIVATE);
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

        long actualLastUsed = webAppPrefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(lastUsed, actualLastUsed);

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"Webapp"})
    public void testCleanupRemovesOldApps() throws Exception {
        // Put the current time such that the task runs.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION;

        // Put the last used time just outside the no-cleanup window.
        addWebappsToRegistry("oldWebapp");
        SharedPreferences webAppPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "oldWebapp", Context.MODE_PRIVATE);
        long lastUsed = currentTime - WebappRegistry.WEBAPP_UNOPENED_CLEANUP_DURATION;
        webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, lastUsed).apply();

        // Force a re-read of the preferences from disk.
        WebappRegistry.refreshSharedPrefsForTesting();

        // Because the time is just inside the window, there should be a cleanup of old web apps and
        // the last cleaned up time should be set to the current time.
        WebappRegistry.getInstance().unregisterOldWebapps(currentTime);

        Set<String> actual = getRegisteredWebapps();
        assertTrue(actual.isEmpty());

        long actualLastUsed = webAppPrefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(WebappDataStorage.TIMESTAMP_INVALID, actualLastUsed);

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"WebApk"})
    public void testCleanupRemovesUninstalledWebApks() throws Exception {
        String webappId1 = WebApkConstants.WEBAPK_ID_PREFIX + "uninstalledWebApk1";
        String webApkPackage1 = "uninstalledWebApk1";
        String webappId2 = WebApkConstants.WEBAPK_ID_PREFIX + "uninstalledWebApk2";
        String webApkPackage2 = "uninstalledWebApk2";

        FetchStorageCallback storageCallback1 =
                new FetchStorageCallback(createWebApkIntent(webappId1, START_URL, webApkPackage1));
        registerWebapp(webappId1, storageCallback1);
        assertTrue(storageCallback1.getCallbackCalled());

        FetchStorageCallback storageCallback2 =
                new FetchStorageCallback(createWebApkIntent(webappId1, START_URL, webApkPackage2));
        registerWebapp(webappId2, storageCallback2);
        assertTrue(storageCallback2.getCallbackCalled());

        // Verify that both WebAPKs are registered.
        Set<String> actual = getRegisteredWebapps();
        assertEquals(2, actual.size());
        assertTrue(actual.contains(webappId1));
        assertTrue(actual.contains(webappId2));

        // Set the current time such that the task runs.
        long currentTime = System.currentTimeMillis() + WebappRegistry.FULL_CLEANUP_DURATION;
        // Because the time is just inside the window, there should be a cleanup of
        // uninstalled WebAPKs and the last cleaned up time should be set to the
        // current time.
        WebappRegistry.getInstance().unregisterOldWebapps(currentTime);

        assertTrue(getRegisteredWebapps().isEmpty());

        long lastCleanup = mSharedPreferences.getLong(
                WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"WebApk"})
    public void testCleanupDoesNotRemoveInstalledWebApks() throws Exception {
        String webappId = WebApkConstants.WEBAPK_ID_PREFIX + "installedWebApk";
        String webApkPackage = "installedWebApk";
        String uninstalledWebappId = WebApkConstants.WEBAPK_ID_PREFIX + "uninstalledWebApk";
        String uninstalledWebApkPackage = "uninstalledWebApk";

        FetchStorageCallback storageCallback =
                new FetchStorageCallback(createWebApkIntent(webappId, START_URL, webApkPackage));
        registerWebapp(webappId, storageCallback);
        assertTrue(storageCallback.getCallbackCalled());

        FetchStorageCallback storageCallback2 = new FetchStorageCallback(
                createWebApkIntent(uninstalledWebappId, START_URL, uninstalledWebApkPackage));
        registerWebapp(uninstalledWebappId, storageCallback2);
        assertTrue(storageCallback2.getCallbackCalled());

        // Verify that both WebAPKs are registered.
        Set<String> actual = getRegisteredWebapps();
        assertEquals(2, actual.size());
        assertTrue(actual.contains(webappId));
        assertTrue(actual.contains(uninstalledWebappId));

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
        assertTrue(actual.contains(webappId));

        long lastCleanup = mSharedPreferences.getLong(
                WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"WebApk"})
    public void testCleanupDoesRemoveOldInstalledWebApks() throws Exception {
        String deprecatedWebappId = "webapk:installedWebApk";
        String webApkPackage = "installedWebApk";
        String installedWebappId = WebApkConstants.WEBAPK_ID_PREFIX + "installedWebApk";

        FetchStorageCallback storageCallback = new FetchStorageCallback(
                createWebApkIntent(deprecatedWebappId, START_URL, webApkPackage));
        registerWebapp(deprecatedWebappId, storageCallback);
        assertTrue(storageCallback.getCallbackCalled());

        FetchStorageCallback storageCallback2 = new FetchStorageCallback(
                createWebApkIntent(installedWebappId, START_URL, webApkPackage));
        registerWebapp(installedWebappId, storageCallback2);
        assertTrue(storageCallback2.getCallbackCalled());

        // Verify that both WebAPKs are registered.
        Set<String> actual = getRegisteredWebapps();
        assertEquals(2, actual.size());
        assertTrue(actual.contains(deprecatedWebappId));
        assertTrue(actual.contains(installedWebappId));

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
        assertTrue(actual.contains(installedWebappId));

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"Webapp"})
    public void testClearWebappHistory() throws Exception {
        final String webapp1Url = "https://www.google.com";
        final String webapp2Url = "https://drive.google.com";
        Intent shortcutIntent1 = createShortcutIntent(webapp1Url);
        Intent shortcutIntent2 = createShortcutIntent(webapp2Url);

        FetchStorageCallback storageCallback1 = new FetchStorageCallback(shortcutIntent1);
        registerWebapp("webapp1", storageCallback1);
        assertTrue(storageCallback1.getCallbackCalled());

        FetchStorageCallback storageCallback2 = new FetchStorageCallback(shortcutIntent2);
        registerWebapp("webapp2", storageCallback2);
        assertTrue(storageCallback2.getCallbackCalled());

        SharedPreferences webapp1Prefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp1", Context.MODE_PRIVATE);
        SharedPreferences webapp2Prefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp2", Context.MODE_PRIVATE);

        long webapp1OriginalLastUsed = webapp2Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        long webapp2OriginalLastUsed = webapp2Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertTrue(webapp1OriginalLastUsed != WebappDataStorage.TIMESTAMP_INVALID);
        assertTrue(webapp2OriginalLastUsed != WebappDataStorage.TIMESTAMP_INVALID);

        // Clear data for |webapp1Url|.
        WebappRegistry.getInstance().clearWebappHistoryForUrlsImpl(
                new UrlFilters.OneUrl(webapp1Url));

        Set<String> actual = getRegisteredWebapps();
        assertEquals(2, actual.size());
        assertTrue(actual.contains("webapp1"));
        assertTrue(actual.contains("webapp2"));

        // Verify that the last used time for the first web app is
        // WebappDataStorage.TIMESTAMP_INVALID, while for the second one it's unchanged.
        long actualLastUsed = webapp1Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(WebappDataStorage.TIMESTAMP_INVALID, actualLastUsed);
        actualLastUsed = webapp2Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(webapp2OriginalLastUsed, actualLastUsed);

        // Verify that the URL and scope for the first web app is WebappDataStorage.URL_INVALID,
        // while for the second one it's unchanged.
        String actualScope = webapp1Prefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualScope);
        String actualUrl = webapp1Prefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualUrl);
        actualScope = webapp2Prefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(webapp2Url + "/", actualScope);
        actualUrl = webapp2Prefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(webapp2Url, actualUrl);

        // Clear data for all urls.
        WebappRegistry.getInstance().clearWebappHistoryForUrlsImpl(new UrlFilters.AllUrls());

        // Verify that the last used time for both web apps is WebappDataStorage.TIMESTAMP_INVALID.
        actualLastUsed = webapp1Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(WebappDataStorage.TIMESTAMP_INVALID, actualLastUsed);
        actualLastUsed = webapp2Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertEquals(WebappDataStorage.TIMESTAMP_INVALID, actualLastUsed);

        // Verify that the URL and scope for both web apps is WebappDataStorage.URL_INVALID.
        actualScope = webapp1Prefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualScope);
        actualUrl = webapp1Prefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualUrl);
        actualScope = webapp2Prefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualScope);
        actualUrl = webapp2Prefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualUrl);
    }

    @Test
    @Feature({"Webapp"})
    public void testGetAfterClearWebappHistory() throws Exception {
        registerWebapp("webapp", null);

        SharedPreferences webappPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp", Context.MODE_PRIVATE);
        WebappRegistry.getInstance().clearWebappHistoryForUrlsImpl(new UrlFilters.AllUrls());

        // Open the webapp up and set the last used time.
        WebappRegistry.getInstance().getWebappDataStorage("webapp").updateLastUsedTime();

        // Verify that the last used time is valid.
        long actualLastUsed = webappPrefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);
        assertTrue(WebappDataStorage.TIMESTAMP_INVALID != actualLastUsed);
    }

    @Test
    @Feature({"Webapp"})
    public void testUpdateAfterClearWebappHistory() throws Exception {
        final String webappUrl = "http://www.google.com";
        final String webappScope = "http://www.google.com/";
        final Intent shortcutIntent = createShortcutIntent(webappUrl);
        registerWebapp("webapp", new FetchStorageCallback(shortcutIntent));

        SharedPreferences webappPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp", Context.MODE_PRIVATE);

        // Verify that the URL and scope match the original in the intent.
        String actualUrl = webappPrefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(webappUrl, actualUrl);
        String actualScope = webappPrefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(webappScope, actualScope);

        WebappRegistry.getInstance().clearWebappHistoryForUrlsImpl(new UrlFilters.AllUrls());

        // Update the webapp from the intent again.
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage("webapp");
        storage.updateFromShortcutIntent(shortcutIntent);

        // Verify that the URL and scope match the original in the intent.
        actualUrl = webappPrefs.getString(WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(webappUrl, actualUrl);
        actualScope = webappPrefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
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

        Intent shortcutIntent1 = createShortcutIntent(webapp1Url);
        Intent shortcutIntent2 = createShortcutIntent(webapp2Url);
        Intent shortcutIntent3 = createShortcutIntent(webapp3Url);
        Intent shortcutIntent4 = createShortcutIntent(webapp4Url);

        // Register the four web apps.
        registerWebapp("webapp1", new FetchStorageCallback(shortcutIntent1));
        registerWebapp("webapp2", new FetchStorageCallback(shortcutIntent2));
        registerWebapp("webapp3", new FetchStorageCallback(shortcutIntent3));
        registerWebapp("webapp4", new FetchStorageCallback(shortcutIntent4));

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

        String webApkId = WebApkConstants.WEBAPK_ID_PREFIX + "WebApk";
        registerWebapp(webApkId,
                new FetchStorageCallback(
                        createWebApkIntent(webApkId, startUrl, "org.chromium.webapk")));

        // testUrl should return null.
        WebappDataStorage storage1 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(testUrl);
        assertNull(storage1);

        String webappId = "webapp";
        registerWebapp(webappId, new FetchStorageCallback(createShortcutIntent(startUrl)));

        // testUrl should return the webapp.
        WebappDataStorage storage2 =
                WebappRegistry.getInstance().getWebappDataStorageForUrl(testUrl);
        assertEquals(webappId, storage2.getId());
    }

    private Set<String> addWebappsToRegistry(String... webapps) {
        final Set<String> expected = new HashSet<>(Arrays.asList(webapps));
        mSharedPreferences.edit().putStringSet(WebappRegistry.KEY_WEBAPP_SET, expected).apply();
        return expected;
    }

    private Set<String> getRegisteredWebapps() {
        return mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
    }

    private Intent createShortcutIntent(final String url) {
        return ShortcutHelper.createWebappShortcutIntentForTesting("id", url);
    }

    private Intent createWebApkIntent(String webappId, String url, String webApkPackage) {
        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, webappId)
                .putExtra(ShortcutHelper.EXTRA_URL, url)
                .putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackage);
        return intent;
    }
}
