// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.content.pm.SigningInfo;
import android.net.Uri;
import android.text.TextUtils;

import androidx.browser.trusted.Token;
import androidx.browser.trusted.TrustedWebActivityService;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.TwaValidator;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionStore;
import org.chromium.chrome.browser.browsing_data.UrlFilters;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Tests the WebappRegistry class by ensuring that it persists data to SharedPreferences as
 * expected.
 */
@RunWith(BaseRobolectricTestRunner.class)
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
        final BrowserServicesIntentDataProvider mIntentDataProvider;
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

    @Before
    public void setUp() {
        WebappRegistry.refreshSharedPrefsForTesting();
        mSharedPreferences =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
        mSharedPreferences.edit().putLong(KEY_LAST_CLEANUP, INITIAL_TIME).commit();

        mCallbackCalled = false;
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
        RobolectricUtil.runAllBackgroundAndUi();
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
     * Test behaviour when there is a webapp with a null id registered. See crbug.com/40676347 for
     * details of the bug which caused this to occur.
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

        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
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

        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
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

        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(testPackageName);
        assertTrue(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin(testOrigin));

        final String exampleUrl = "https://example.com/app/";
        final String examplePackageName = "org.chromium.webapk.example";
        BrowserServicesIntentDataProvider exampleWebApk =
                new WebApkIntentDataProviderBuilder(examplePackageName, exampleUrl).build();
        registerWebapp(exampleWebApk);
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(examplePackageName);

        assertTrue(
                WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("https://example.com"));
        assertTrue(
                WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("https://example.com/"));
        assertTrue(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOrigin("https://example.com/other_path"));
        assertTrue(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOrigin("https://example.com:443"));
        assertFalse(
                WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("https://example.co"));
        assertFalse(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOrigin("https://example.com.other.domain"));
        assertFalse(
                WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("http://example.com"));
        assertFalse(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOrigin("https://example.com:8080"));

        final String companyUrl = "https://a.company.com/app/";
        final String companyPackageName = "org.chromium.webapk.company";
        BrowserServicesIntentDataProvider companyWebApk =
                new WebApkIntentDataProviderBuilder(companyPackageName, companyUrl).build();
        registerWebapp(companyWebApk);
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(companyPackageName);

        assertTrue(
                WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("https://a.company.com"));
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("https://a.com"));
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("https://a.comp"));

        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin((String) null));
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin((Origin) null));
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin(""));
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("invalid_origin"));
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("about:blank"));
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("file:///sdcard"));

        assertTrue(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOrigin(Origin.create("https://example.com")));
        assertFalse(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOrigin(Origin.create("https://example.co")));

        // Marking WebAPK as uninstalled should cause hasAtLeastOneWebApkForOrigin to return false.
        String companyWebappId = companyWebApk.getWebappExtras().id;
        WebappDataStorage companyStorage =
                WebappRegistry.getInstance().getWebappDataStorage(companyWebappId);
        assertNotNull(companyStorage);
        companyStorage.setWebApkUninstallTimestamp();
        assertFalse(
                WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin("https://a.company.com"));
    }

    @Test
    @Feature({"WebApk"})
    public void testHasWebApkForOriginWithoutPackageCheck() throws Exception {
        final String startUrl = START_URL + "/test_page.html";
        final String testOrigin = START_URL;
        final String testPackageName = "org.chromium.webapk";

        assertFalse(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOriginWithoutPackageCheck(testOrigin));

        String webappId = "webapp";
        registerWebappWithId(webappId, createShortcutIntentDataProvider(startUrl));
        assertFalse(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOriginWithoutPackageCheck(testOrigin));

        String incompleteWebApkId = "webapk-incomplete";
        registerWebappWithId(incompleteWebApkId, null);
        SharedPreferences incompleteWebApkPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + incompleteWebApkId,
                                Context.MODE_PRIVATE);
        incompleteWebApkPrefs.edit().putString(WebappDataStorage.KEY_SCOPE, startUrl).apply();
        assertFalse(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOriginWithoutPackageCheck(testOrigin));

        BrowserServicesIntentDataProvider webApkIntentDataProvider =
                new WebApkIntentDataProviderBuilder(testPackageName, startUrl).build();
        registerWebapp(webApkIntentDataProvider);
        assertTrue(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOriginWithoutPackageCheck(testOrigin));
        // The package-install check is intentionally skipped here, whereas
        // hasAtLeastOneWebApkForOrigin() returns false for the same origin because the WebAPK
        // package is not installed in this test.
        assertFalse(WebappRegistry.getInstance().hasAtLeastOneWebApkForOrigin(testOrigin));

        // A different origin that is only a string prefix of the scope must NOT match. This
        // guards against a raw scope.startsWith(origin) comparison spuriously matching e.g.
        // "https://foo.co" against a scope of "https://foo.com/test_page.html".
        assertFalse(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOriginWithoutPackageCheck("https://foo.co"));

        // An uninstalled WebAPK must be ignored even without the package-install check.
        WebappDataStorage webApkStorage =
                WebappRegistry.getInstance()
                        .getWebappDataStorage(webApkIntentDataProvider.getWebappExtras().id);
        assertNotNull(webApkStorage);
        webApkStorage.setWebApkUninstallTimestamp();
        assertFalse(
                WebappRegistry.getInstance()
                        .hasAtLeastOneWebApkForOriginWithoutPackageCheck(testOrigin));
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
        assertEquals(testManifestId, storage.getWebApkManifestId());
        assertEquals(testPackageName, storage.getWebApkPackageName());

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
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(testPackageName);

        assertEquals(
                testPackageName,
                WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));

        final String anotherManifestId = START_URL + "/test_page.html";
        assertNull(
                WebappRegistry.getInstance().getWebappDataStorageForManifestId(anotherManifestId));
    }

    @Test
    @Feature({"WebApk"})
    public void testFindWebApkPackageWithManifestId_ignoresUninstalledWebApk() throws Exception {
        final String testManifestId = START_URL + "/id";
        final String testPackageName = "org.chromium.webapk";

        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(testPackageName, START_URL)
                        .setWebApkManifestId(testManifestId)
                        .build();
        registerWebapp(intentDataProvider);
        WebappDataStorage storage =
                WebappRegistry.getInstance()
                        .getWebappDataStorage(intentDataProvider.getWebappExtras().id);
        assertNotNull(storage);

        // Package is not installed in PackageManager -> should return null.
        assertNull(WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));

        // Install package in PackageManager -> should return package name.
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(testPackageName);
        assertEquals(
                testPackageName,
                WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));

        // Mark storage as uninstalled -> should return null even if still in PackageManager.
        storage.setWebApkUninstallTimestamp();
        assertNull(WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));
    }

    @Test
    @Feature({"WebApk"})
    public void testPendingWebApkRegistration() {
        final String testManifestId = START_URL + "/id";
        final String testPackageName = "org.chromium.webapk";

        assertNull(WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));

        WebappRegistry.getInstance().registerPendingWebApk(testManifestId, testPackageName);
        assertEquals(
                testPackageName,
                WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));
    }

    @Test
    @Feature({"WebApk"})
    public void testRemovePendingWebApk() {
        final String testManifestId = START_URL + "/id";
        final String testPackageName = "org.chromium.webapk";

        WebappRegistry.getInstance().registerPendingWebApk(testManifestId, testPackageName);
        assertEquals(
                testPackageName,
                WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));

        WebappRegistry.getInstance().removePendingWebApk(testManifestId);
        assertNull(WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));
    }

    @Test
    @Feature({"WebApk"})
    public void testPendingWebApkClearedOnRegister() throws Exception {
        final String testManifestId = START_URL + "/id";
        final String testPackageName = "org.chromium.webapk";

        WebappRegistry.getInstance().registerPendingWebApk(testManifestId, testPackageName);
        assertEquals(
                testPackageName,
                WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));

        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(testPackageName, START_URL)
                        .setWebApkManifestId(testManifestId)
                        .build();
        registerWebapp(intentDataProvider);
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(testPackageName);

        // It should still be found because it is now registered.
        assertEquals(
                testPackageName,
                WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));

        WebappRegistry.getInstance()
                .removePendingWebApk(testManifestId); // Should be no-op if already removed
        assertEquals(
                testPackageName,
                WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));

        WebappRegistry.getInstance().clearForTesting();
        assertNull(WebappRegistry.getInstance().findWebApkWithManifestId(testManifestId));
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

    @Test
    @Feature({"Webapp"})
    public void testGetOriginsWithInstalledAppFiltersUninstalledWebApks() throws Exception {
        String webApkPackage = "test.webapk.package";
        String startUrl = "https://example.com/start";
        Origin startUrlOrigin = Origin.create(startUrl);

        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(webApkPackage, startUrl).build();
        registerWebapp(intentDataProvider);

        // 1. Initially it should be in the set of origins with installed app.
        assertTrue(
                WebappRegistry.getInstance()
                        .getOriginsWithInstalledApp()
                        .contains(startUrlOrigin.toString()));

        // 2. Mark as uninstalled.
        String webappId = intentDataProvider.getWebappExtras().id;
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(webappId);
        assertNotNull(storage);
        storage.setWebApkUninstallTimestamp();

        // 3. Verify it is now filtered out.
        assertFalse(
                WebappRegistry.getInstance()
                        .getOriginsWithInstalledApp()
                        .contains(startUrlOrigin.toString()));

        // 4. Re-register (simulate re-install)
        registerWebapp(intentDataProvider);

        // 5. Verify it is back in the set (uninstall timestamp was reset)
        assertTrue(
                WebappRegistry.getInstance()
                        .getOriginsWithInstalledApp()
                        .contains(startUrlOrigin.toString()));
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappRegistryObserverNotifiedOnRegister() throws Exception {
        String webApkPackage = "test.webapk.package";
        String startUrl = "https://example.com/start";
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(webApkPackage, startUrl).build();

        // 1. Set up observer
        final int[] notificationCount = {0};
        WebappRegistry.Observer observer = () -> notificationCount[0]++;
        WebappRegistry.getInstance().registerObserver(observer);

        // 2. Register webapp
        registerWebapp(intentDataProvider);

        // 3. Verify observer was notified
        assertEquals(1, notificationCount[0]);

        // Clean up
        WebappRegistry.getInstance().unregisterObserver(observer);
    }

    @Test
    public void testIsAppInstalledForUrl_handlesGurlAndTwaValidator() {
        GURL url = new GURL("https://example.com/app/path");
        assertFalse(WebappRegistry.getInstance().isAppInstalledForUrl(url));

        TwaValidator.setTwaInstalledOverrideForTesting(true);
        assertTrue(WebappRegistry.getInstance().isAppInstalledForUrl(url));

        // Non-HTTP/invalid URLs should safely return false.
        assertFalse(
                WebappRegistry.getInstance().isAppInstalledForUrl(new GURL("chrome://settings")));
        assertFalse(WebappRegistry.getInstance().isAppInstalledForUrl(new GURL("about:blank")));

        TwaValidator.setTwaInstalledOverrideForTesting(false);
        assertFalse(WebappRegistry.getInstance().isAppInstalledForUrl(url));
    }

    @Test
    @Feature({"WebApk"})
    public void testIsAppInstalledForUrl_isScopeAwareOnSameOrigin() throws Exception {
        // Regression test for crbug.com/568841400: two PWAs on the same origin with different
        // scopes must not be treated as the same app.
        final String packageA = "org.chromium.webapk.a";
        final String scopeA = "https://example.com/apps/pwa-a/";
        BrowserServicesIntentDataProvider webApkA =
                new WebApkIntentDataProviderBuilder(packageA, scopeA).setScope(scopeA).build();
        registerWebapp(webApkA);
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(packageA);
        TwaValidator.setTwaInstalledOverrideForTesting(false);

        WebappRegistry registry = WebappRegistry.getInstance();

        // In-scope URLs of A are "installed".
        assertTrue(registry.isAppInstalledForUrl(new GURL(scopeA)));
        assertTrue(registry.isAppInstalledForUrl(new GURL(scopeA + "page.html?x=1#frag")));

        // Same origin, different scope (PWA B) is NOT "installed".
        assertFalse(registry.isAppInstalledForUrl(new GURL("https://example.com/apps/pwa-b/")));
        // Parent directories of the scope are NOT "installed".
        assertFalse(registry.isAppInstalledForUrl(new GURL("https://example.com/apps/")));
        assertFalse(registry.isAppInstalledForUrl(new GURL("https://example.com/")));
        // Different origin is NOT "installed".
        assertFalse(
                registry.isAppInstalledForUrl(new GURL("https://other.example.com/apps/pwa-a/")));

        // Subdirectories within scopeA are "installed".
        assertTrue(registry.isAppInstalledForUrl(new GURL("https://example.com/apps/pwa-a/sub/")));
        // Because scopeA ends with '/', the bare path without a trailing slash and sibling
        // paths that share the prefix before '/' are NOT in scope.
        assertFalse(registry.isAppInstalledForUrl(new GURL("https://example.com/apps/pwa-a")));
        assertFalse(
                registry.isAppInstalledForUrl(new GURL("https://example.com/apps/pwa-a-other/")));

        // Uninstalled WebAPKs are ignored even for in-scope URLs.
        WebappDataStorage storageA = registry.getWebappDataStorage(webApkA.getWebappExtras().id);
        assertNotNull(storageA);
        storageA.setWebApkUninstallTimestamp();
        assertFalse(registry.isAppInstalledForUrl(new GURL(scopeA)));
    }

    @Test
    @Feature({"WebApk"})
    public void testIsAppInstalledForUrl_twoWebApksSameOrigin() throws Exception {
        final String packageA = "org.chromium.webapk.a";
        final String packageB = "org.chromium.webapk.b";
        final String scopeA = "https://example.com/apps/pwa-a/";
        final String scopeB = "https://example.com/apps/pwa-b/";
        registerWebapp(
                new WebApkIntentDataProviderBuilder(packageA, scopeA).setScope(scopeA).build());
        registerWebapp(
                new WebApkIntentDataProviderBuilder(packageB, scopeB).setScope(scopeB).build());
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(packageA);
        // B's package is NOT installed on the device yet.
        TwaValidator.setTwaInstalledOverrideForTesting(false);

        WebappRegistry registry = WebappRegistry.getInstance();
        assertTrue(registry.isAppInstalledForUrl(new GURL(scopeA)));
        assertFalse(registry.isAppInstalledForUrl(new GURL(scopeB)));

        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(packageB);
        assertTrue(registry.isAppInstalledForUrl(new GURL(scopeB)));
    }

    @Test
    @Feature({"WebApk"})
    public void testIsAppInstalledForUrl_nestedScopes() throws Exception {
        final String packageParent = "org.chromium.webapk.parent";
        final String scopeParent = "https://example.com/apps/";
        final String packageChild = "org.chromium.webapk.child";
        final String scopeChild = "https://example.com/apps/pwa-b/";
        registerWebapp(
                new WebApkIntentDataProviderBuilder(packageParent, scopeParent)
                        .setScope(scopeParent)
                        .build());
        BrowserServicesIntentDataProvider webApkChild =
                new WebApkIntentDataProviderBuilder(packageChild, scopeChild)
                        .setScope(scopeChild)
                        .build();
        registerWebapp(webApkChild);
        TwaValidator.setTwaInstalledOverrideForTesting(false);

        WebappRegistry registry = WebappRegistry.getInstance();

        // If the parent WebAPK is not installed in PackageManager (even with uninstallTimestamp
        // == 0) while the child WebAPK is installed, the loop continues past the parent and
        // matches the child for child URLs only.
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(packageChild);
        assertFalse(registry.isAppInstalledForUrl(new GURL(scopeParent)));
        assertTrue(registry.isAppInstalledForUrl(new GURL(scopeChild)));

        // Once the parent WebAPK is installed, it covers both its own scope and any nested path,
        // including the child scope even after the child WebAPK is uninstalled.
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(packageParent);
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .removePackage(packageChild);
        assertTrue(registry.isAppInstalledForUrl(new GURL(scopeParent)));
        assertTrue(registry.isAppInstalledForUrl(new GURL(scopeChild)));
        assertTrue(registry.isAppInstalledForUrl(new GURL("https://example.com/apps/other/")));

        WebappDataStorage storageChild =
                registry.getWebappDataStorage(webApkChild.getWebappExtras().id);
        assertNotNull(storageChild);
        storageChild.setWebApkUninstallTimestamp();
        assertTrue(registry.isAppInstalledForUrl(new GURL(scopeChild)));
    }

    @Test
    @Feature({"WebApk"})
    public void testIsAppInstalledForUrl_scopeClearedByClearHistory() throws Exception {
        // After "clear history" the stored scope is removed until the next launch; in that
        // window we fail open (report "not installed" so the user can still see Install).
        final String packageA = "org.chromium.webapk.a";
        final String scopeA = "https://example.com/apps/pwa-a/";
        BrowserServicesIntentDataProvider webApkA =
                new WebApkIntentDataProviderBuilder(packageA, scopeA).setScope(scopeA).build();
        registerWebapp(webApkA);
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(packageA);
        TwaValidator.setTwaInstalledOverrideForTesting(false);

        WebappRegistry registry = WebappRegistry.getInstance();
        assertTrue(registry.isAppInstalledForUrl(new GURL(scopeA)));

        WebappDataStorage storageA = registry.getWebappDataStorage(webApkA.getWebappExtras().id);
        assertNotNull(storageA);
        storageA.clearHistory();
        assertFalse(registry.isAppInstalledForUrl(new GURL(scopeA)));
    }

    @Test
    @Feature({"WebApk"})
    public void testIsAppInstalledForUrl_scopeUpdatedOnRelaunch() throws Exception {
        final String packageA = "org.chromium.webapk.a";
        final String startUrl = "https://example.com/apps/pwa-a/index.html";
        final String initialScope = "https://example.com/apps/";
        final String narrowedScope = "https://example.com/apps/pwa-a/";
        BrowserServicesIntentDataProvider webApkA =
                new WebApkIntentDataProviderBuilder(packageA, startUrl)
                        .setScope(initialScope)
                        .build();
        registerWebapp(webApkA);
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(packageA);
        TwaValidator.setTwaInstalledOverrideForTesting(false);

        WebappRegistry registry = WebappRegistry.getInstance();
        assertTrue(registry.isAppInstalledForUrl(new GURL("https://example.com/apps/pwa-b/")));

        // Simulate relaunching the WebAPK after a manifest update narrowed its scope.
        WebappDataStorage storageA = registry.getWebappDataStorage(webApkA.getWebappExtras().id);
        assertNotNull(storageA);
        storageA.updateFromWebappIntentDataProvider(
                new WebApkIntentDataProviderBuilder(packageA, startUrl)
                        .setScope(narrowedScope)
                        .build());

        assertTrue(registry.isAppInstalledForUrl(new GURL(narrowedScope)));
        assertFalse(registry.isAppInstalledForUrl(new GURL("https://example.com/apps/pwa-b/")));
    }

    @Test
    @Feature({"WebApk"})
    public void testQueryIsAppInstalledForUrl() throws Exception {
        // Register an installed WebAPK for scopeA. Turn off TWA detection so that only the
        // WebAPK decides the result.
        final String packageA = "org.chromium.webapk.a";
        final String scopeA = "https://example.com/apps/pwa-a/";
        final GURL outOfScope = new GURL("https://example.com/apps/pwa-b/");
        registerWebapp(
                new WebApkIntentDataProviderBuilder(packageA, scopeA).setScope(scopeA).build());
        Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager())
                .addPackage(packageA);
        TwaValidator.setTwaInstalledOverrideForTesting(false);
        WebappRegistry registry = WebappRegistry.getInstance();
        List<Boolean> results = new ArrayList<>();

        // Query one in-scope URL and one out-of-scope URL. The callbacks must not run until the
        // worker and UI tasks run.
        registry.queryIsAppInstalledForUrl(new GURL(scopeA + "page.html"), results::add);
        registry.queryIsAppInstalledForUrl(outOfScope, results::add);
        assertTrue("Results must be asynchronous", results.isEmpty());
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(Arrays.asList(true, false), results);

        // TWAs are detected on the worker; only http(s) URLs can have an installed app.
        TwaValidator.setTwaInstalledOverrideForTesting(true);
        // Pump after each query: runAllBackgroundAndUi() idles the UI looper before the worker, so
        // the chrome:// reply (posted straight to the UI thread) would otherwise come first.
        registry.queryIsAppInstalledForUrl(outOfScope, results::add);
        RobolectricUtil.runAllBackgroundAndUi();
        registry.queryIsAppInstalledForUrl(new GURL("chrome://settings"), results::add);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(Arrays.asList(true, false, true, false), results);
    }

    @Test
    @Feature({"WebApk"})
    @SuppressWarnings("unchecked")
    public void testQueryIsAppInstalledForUrl_callbackRunsWhenWorkerThrows() {
        // No WebAPK is registered, so the worker goes on to the TWA check for this page.
        final String twaPackage = "com.example.twa";
        final String pageUrl = "https://example.com/apps/page.html";
        PackageManager pm = RuntimeEnvironment.getApplication().getPackageManager();

        // Make a fake TWA package handle VIEW intents for the page, so that TwaValidator picks it
        // as a candidate.
        ResolveInfo viewResolveInfo = new ResolveInfo();
        viewResolveInfo.activityInfo = new ActivityInfo();
        viewResolveInfo.activityInfo.packageName = twaPackage;
        Shadows.shadowOf(pm)
                .addResolveInfoForIntent(
                        new Intent(Intent.ACTION_VIEW, Uri.parse(pageUrl)), viewResolveInfo);

        // Give the package a TrustedWebActivityService, so that TwaValidator goes on to read the
        // verified delegate apps.
        ResolveInfo serviceResolveInfo = new ResolveInfo();
        serviceResolveInfo.serviceInfo = new ServiceInfo();
        serviceResolveInfo.serviceInfo.packageName = twaPackage;
        Intent serviceIntent = new Intent();
        serviceIntent.setPackage(twaPackage);
        serviceIntent.setAction(TrustedWebActivityService.ACTION_TRUSTED_WEB_ACTIVITY_SERVICE);
        Shadows.shadowOf(pm).addResolveInfoForIntent(serviceIntent, serviceResolveInfo);

        // Make the delegate app set throw when the worker iterates it. This simulates an
        // unexpected RuntimeException on the worker thread.
        InstalledWebappPermissionStore store = mock(InstalledWebappPermissionStore.class);
        Set<Token> throwingTokens = mock(Set.class);
        when(throwingTokens.iterator()).thenThrow(new RuntimeException("Simulated worker failure"));
        when(store.getAllDelegateApps(any())).thenReturn(throwingTokens);
        WebappRegistry registry = WebappRegistry.getInstance();
        registry.setPermissionStoreForTesting(store);

        // Query the page, then run the worker task and the UI reply.
        List<Boolean> results = new ArrayList<>();
        registry.queryIsAppInstalledForUrl(new GURL(pageUrl), results::add);
        RobolectricUtil.runAllBackgroundAndUi();

        // The worker hit the exception, but the callback still ran on the UI thread with false.
        verify(throwingTokens).iterator();
        assertEquals(Collections.singletonList(false), results);
    }

    @Test
    @Feature({"WebApk"})
    public void testGetAppInstallStateKeyForUrl() throws Exception {
        // With no apps, an http(s) URL gets the origin-only key, for example
        // "https://example.com|0|". Empty, invalid and non-http(s) URLs get null, because no app
        // can be installed for them.
        final String packageA = "org.chromium.webapk.a";
        final String packageB = "org.chromium.webapk.b";
        final String scopeA = "https://example.com/apps/pwa-a/";
        final String scopeB = "https://example.com/apps/pwa-b/";
        final GURL pageA = new GURL(scopeA + "1.html");
        final GURL outOfScope = new GURL(scopeB + "1.html");
        WebappRegistry registry = WebappRegistry.getInstance();
        String originKey = registry.getAppInstallStateKeyForUrl(pageA);
        assertEquals("https://example.com|0|", originKey);
        assertEquals(originKey, registry.getAppInstallStateKeyForUrl(outOfScope));
        assertNull(registry.getAppInstallStateKeyForUrl(GURL.emptyGURL()));
        assertNull(registry.getAppInstallStateKeyForUrl(new GURL("not a url")));
        assertNull(registry.getAppInstallStateKeyForUrl(new GURL("about:blank")));
        assertNull(registry.getAppInstallStateKeyForUrl(new GURL("chrome://settings")));

        // Another site's WebAPK doesn't change the key.
        registerWebapp(
                new WebApkIntentDataProviderBuilder("org.chromium.webapk.o", "https://o.com/")
                        .setScope("https://o.com/")
                        .build());
        assertEquals(originKey, registry.getAppInstallStateKeyForUrl(pageA));

        // A WebAPK whose scope contains the URL changes it, for in-scope URLs only.
        BrowserServicesIntentDataProvider webApkA =
                new WebApkIntentDataProviderBuilder(packageA, scopeA).setScope(scopeA).build();
        registerWebapp(webApkA);
        String keyA = registry.getAppInstallStateKeyForUrl(pageA);
        assertNotEquals(originKey, keyA);
        assertEquals("https://example.com|0|" + packageA, keyA);
        assertEquals(keyA, registry.getAppInstallStateKeyForUrl(new GURL(scopeA + "2?q#r")));
        assertEquals(originKey, registry.getAppInstallStateKeyForUrl(outOfScope));

        // Two WebAPKs with different scopes on one origin give distinct keys.
        BrowserServicesIntentDataProvider webApkB =
                new WebApkIntentDataProviderBuilder(packageB, scopeB).setScope(scopeB).build();
        registerWebapp(webApkB);
        String keyB = registry.getAppInstallStateKeyForUrl(outOfScope);
        assertEquals("https://example.com|0|" + packageB, keyB);
        assertNotEquals(keyA, keyB);
        assertEquals(keyA, registry.getAppInstallStateKeyForUrl(pageA));

        // Uninstalling a WebAPK changes the key back.
        WebappDataStorage storageA = registry.getWebappDataStorage(webApkA.getWebappExtras().id);
        assertNotNull(storageA);
        storageA.setWebApkUninstallTimestamp();
        assertEquals(originKey, registry.getAppInstallStateKeyForUrl(pageA));

        // A TWA verified for the origin changes it via getDelegateAppCount.
        InstalledWebappPermissionStore store = mock(InstalledWebappPermissionStore.class);
        registry.setPermissionStoreForTesting(store);
        doReturn(1).when(store).getDelegateAppCount(any());
        assertEquals("https://example.com|1|", registry.getAppInstallStateKeyForUrl(pageA));
    }

    @Test
    @Feature({"WebApk"})
    public void testGetAppInstallStateKeyForUrl_nestedScopesAndLegacyShortcuts() throws Exception {
        // The parent WebAPK scope contains the child WebAPK scope.
        final String packageParent = "org.chromium.webapk.parent";
        final String scopeParent = "https://example.com/apps/";
        final String packageChild = "org.chromium.webapk.child";
        final String scopeChild = "https://example.com/apps/pwa-b/";
        final GURL childPage = new GURL(scopeChild + "index.html");
        WebappRegistry registry = WebappRegistry.getInstance();

        // With no apps, the child page gets the origin-only key.
        String initialKey = registry.getAppInstallStateKeyForUrl(childPage);
        assertEquals("https://example.com|0|", initialKey);

        // A legacy (non-WebAPK) shortcut on the same URL does not change the key.
        registerWebappWithId(
                "legacy_shortcut", createShortcutIntentDataProvider(childPage.getSpec()));
        assertEquals(initialKey, registry.getAppInstallStateKeyForUrl(childPage));

        // Register parent first, then child: nested scopes produce a sorted package list.
        registerWebapp(
                new WebApkIntentDataProviderBuilder(packageParent, scopeParent)
                        .setScope(scopeParent)
                        .build());
        registerWebapp(
                new WebApkIntentDataProviderBuilder(packageChild, scopeChild)
                        .setScope(scopeChild)
                        .build());

        // The child page is in both scopes, so its key lists both packages. A page in the parent
        // scope only lists the parent package.
        assertEquals(
                "https://example.com|0|" + packageChild + "," + packageParent,
                registry.getAppInstallStateKeyForUrl(childPage));
        assertEquals(
                "https://example.com|0|" + packageParent,
                registry.getAppInstallStateKeyForUrl(new GURL(scopeParent + "index.html")));
    }

    @Test
    @Feature({"WebApk"})
    public void testGetAppInstallStateKeyForUrl_twaCountAndRemoveOrigin() {
        // Start with an empty permission store, so that no TWA is verified for the origin.
        final GURL page = new GURL("https://example.com/apps/page.html");
        final Origin origin = Origin.create(page.getSpec());
        assertNotNull(origin);
        WebappRegistry registry = WebappRegistry.getInstance();
        InstalledWebappPermissionStore store = registry.getPermissionStore();
        store.clearForTesting();

        // With no TWA, the TWA count in the key is 0.
        assertEquals("https://example.com|0|", registry.getAppInstallStateKeyForUrl(page));

        // Create signed tokens for two fake TWA packages.
        Token token1 = createTokenForTestPackage("com.example.twa.one", "0102030405060708");
        Token token2 = createTokenForTestPackage("com.example.twa.two", "1112131415161718");

        // One TWA on the origin gives a count of 1.
        store.addDelegateApp(origin, token1);
        assertEquals("https://example.com|1|", registry.getAppInstallStateKeyForUrl(page));

        // Two TWAs on the same origin give a count of 2.
        store.addDelegateApp(origin, token2);
        assertEquals("https://example.com|2|", registry.getAppInstallStateKeyForUrl(page));

        // Removing the origin resets the TWA count in the key back to 0.
        store.removeOrigin(origin);
        assertEquals("https://example.com|0|", registry.getAppInstallStateKeyForUrl(page));
    }

    @Test
    @Feature({"WebApk"})
    public void testGetAppInstallStateKeyForUrl_changesAfterInitStoragesOnUiThread()
            throws Exception {
        // Build a WebAPK for scopeA, but do not register it through the registry.
        final String packageA = "org.chromium.webapk.a";
        final String scopeA = "https://example.com/apps/pwa-a/";
        final GURL pageA = new GURL(scopeA + "1.html");
        BrowserServicesIntentDataProvider webApkA =
                new WebApkIntentDataProviderBuilder(packageA, scopeA).setScope(scopeA).build();
        String webappId = webApkA.getWebappExtras().id;

        // Persist the WebAPK to SharedPreferences after setUp()'s initial load, so mStorages does
        // not contain it yet.
        addWebappsToRegistry(webappId);
        WebappDataStorage.open(webappId).updateFromWebappIntentDataProvider(webApkA);

        // The registry has not loaded the new storage, so the key does not include the WebAPK.
        WebappRegistry registry = WebappRegistry.getInstance();
        assertEquals("https://example.com|0|", registry.getAppInstallStateKeyForUrl(pageA));

        // Warm up from a background thread, then pump the UI thread to run initStoragesOnUiThread.
        Thread warmUpThread = new Thread(WebappRegistry::warmUpSharedPrefs);
        warmUpThread.start();
        warmUpThread.join();
        RobolectricUtil.runAllBackgroundAndUi();

        // initStoragesOnUiThread() added the WebAPK to mStorages, so the key now includes it.
        assertEquals(
                "https://example.com|0|" + packageA, registry.getAppInstallStateKeyForUrl(pageA));
    }

    private static Token createTokenForTestPackage(String packageName, String certHex) {
        PackageManager pm = RuntimeEnvironment.getApplication().getPackageManager();
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.signingInfo = new SigningInfo();
        Shadows.shadowOf(packageInfo.signingInfo)
                .setSignatures(new Signature[] {new Signature(certHex)});
        Shadows.shadowOf(pm).installPackage(packageInfo);
        Token token = Token.create(packageName, pm);
        assertNotNull(token);
        return token;
    }
}
