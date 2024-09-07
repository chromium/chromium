// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.intents.BitmapHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;

import java.util.concurrent.TimeUnit;

/**
 * Tests the WebappDataStorage class by ensuring that it persists data to SharedPreferences as
 * expected.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {BackgroundShadowAsyncTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class WebappDataStorageTest {
    @Rule public FakeTimeTestRule mClockRule = new FakeTimeTestRule();

    private SharedPreferences mSharedPreferences;
    private boolean mCallbackCalled;

    @Before
    public void setUp() {
        mSharedPreferences =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "test",
                                Context.MODE_PRIVATE);

        // Set the last_used as if the web app had been registered by WebappRegistry.
        mSharedPreferences.edit().putLong(WebappDataStorage.KEY_LAST_USED, 0).apply();

        mCallbackCalled = false;
    }

    @Test
    @Feature({"Webapp"})
    public void testBackwardCompat() {
        assertEquals("webapp_", WebappDataStorage.SHARED_PREFS_FILE_PREFIX);
        assertEquals("splash_icon", WebappDataStorage.KEY_SPLASH_ICON);
        assertEquals("last_used", WebappDataStorage.KEY_LAST_USED);
        assertEquals("url", WebappDataStorage.KEY_URL);
        assertEquals("scope", WebappDataStorage.KEY_SCOPE);
        assertEquals("icon", WebappDataStorage.KEY_ICON);
        assertEquals("name", WebappDataStorage.KEY_NAME);
        assertEquals("short_name", WebappDataStorage.KEY_SHORT_NAME);
        assertEquals("orientation", WebappDataStorage.KEY_ORIENTATION);
        assertEquals("theme_color", WebappDataStorage.KEY_THEME_COLOR);
        assertEquals("background_color", WebappDataStorage.KEY_BACKGROUND_COLOR);
        assertEquals("source", WebappDataStorage.KEY_SOURCE);
        assertEquals("is_icon_generated", WebappDataStorage.KEY_IS_ICON_GENERATED);
        assertEquals("version", WebappDataStorage.KEY_VERSION);
    }

    @Test
    @Feature({"Webapp"})
    public void testLastUsedRetrieval() {
        long lastUsed = 100;
        mSharedPreferences.edit().putLong(WebappDataStorage.KEY_LAST_USED, lastUsed).apply();
        assertEquals(lastUsed, new WebappDataStorage("test").getLastUsedTimeMs());
    }

    @Test
    @Feature({"Webapp"})
    public void testSplashImageRetrieval() throws Exception {
        final Bitmap expected = createBitmap();
        mSharedPreferences
                .edit()
                .putString(
                        WebappDataStorage.KEY_SPLASH_ICON,
                        BitmapHelper.encodeBitmapAsString(expected))
                .apply();
        WebappDataStorage.open("test")
                .getSplashScreenImage(
                        new WebappDataStorage.FetchCallback<Bitmap>() {
                            @Override
                            public void onDataRetrieved(Bitmap actual) {
                                mCallbackCalled = true;
                                assertTrue(expected.sameAs(actual));
                            }
                        });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertTrue(mCallbackCalled);
    }

    @Test
    @Feature({"Webapp"})
    public void testSplashImageUpdate() throws Exception {
        Bitmap expectedImage = createBitmap();
        String imageAsString = BitmapHelper.encodeBitmapAsString(expectedImage);
        WebappDataStorage.open("test").updateSplashScreenImage(imageAsString);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                imageAsString,
                mSharedPreferences.getString(WebappDataStorage.KEY_SPLASH_ICON, null));
    }

    @Test
    @Feature({"Webapp"})
    public void testScopeRetrieval() {
        String scope = "http://drive.google.com";
        mSharedPreferences.edit().putString(WebappDataStorage.KEY_SCOPE, scope).apply();
        assertEquals(scope, new WebappDataStorage("test").getScope());
    }

    @Test
    @Feature({"Webapp"})
    public void testUrlRetrieval() {
        String url = "https://www.google.com";
        mSharedPreferences.edit().putString(WebappDataStorage.KEY_URL, url).apply();
        assertEquals(url, new WebappDataStorage("test").getUrl());
    }

    @Test
    @Feature({"Webapp"})
    public void testWasLaunchedRecently() {
        // Opening a data storage doesn't count as a launch.
        WebappDataStorage storage = WebappDataStorage.open("test");
        assertTrue(!storage.wasUsedRecently());

        // When the last used time is updated, then it is a launch.
        storage.updateLastUsedTime();
        assertTrue(storage.wasUsedRecently());

        long lastUsedTime =
                mSharedPreferences.getLong(
                        WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);

        assertTrue(lastUsedTime != WebappDataStorage.TIMESTAMP_INVALID);

        // Move the last used time one day in the past.
        mSharedPreferences
                .edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, lastUsedTime - TimeUnit.DAYS.toMillis(1L))
                .apply();
        assertTrue(storage.wasUsedRecently());

        // Move the last used time three days in the past.
        mSharedPreferences
                .edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, lastUsedTime - TimeUnit.DAYS.toMillis(3L))
                .apply();
        assertTrue(storage.wasUsedRecently());

        // Move the last used time one week in the past.
        mSharedPreferences
                .edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, lastUsedTime - TimeUnit.DAYS.toMillis(7L))
                .apply();
        assertTrue(storage.wasUsedRecently());

        // Move the last used time just under ten days in the past.
        mSharedPreferences
                .edit()
                .putLong(
                        WebappDataStorage.KEY_LAST_USED,
                        lastUsedTime - TimeUnit.DAYS.toMillis(10L) + 1)
                .apply();
        assertTrue(storage.wasUsedRecently());

        // Move the last used time to exactly ten days in the past.
        mSharedPreferences
                .edit()
                .putLong(
                        WebappDataStorage.KEY_LAST_USED, lastUsedTime - TimeUnit.DAYS.toMillis(10L))
                .apply();
        assertTrue(!storage.wasUsedRecently());
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappInfoUpdate() {
        final String id = "id";
        final String url = "url";
        final String scope = "scope";
        final String name = "name";
        final String shortName = "shortName";
        final String encodedIcon = BitmapHelper.encodeBitmapAsString(createBitmap());
        final @DisplayMode.EnumType int displayMode = DisplayMode.STANDALONE;
        final int orientation = 1;
        final long themeColor = 2;
        final long backgroundColor = 3;
        final boolean isIconGenerated = false;
        final boolean isIconAdaptive = false;
        Intent shortcutIntent =
                ShortcutHelper.createWebappShortcutIntent(
                        id,
                        url,
                        scope,
                        name,
                        shortName,
                        encodedIcon,
                        WebappConstants.WEBAPP_SHORTCUT_VERSION,
                        displayMode,
                        orientation,
                        themeColor,
                        backgroundColor,
                        isIconGenerated,
                        isIconAdaptive);
        BrowserServicesIntentDataProvider intentDataProvider =
                WebappIntentDataProviderFactory.create(shortcutIntent);
        assertNotNull(intentDataProvider);

        WebappDataStorage storage = WebappDataStorage.open("test");
        storage.updateFromWebappIntentDataProvider(intentDataProvider);

        assertEquals(url, mSharedPreferences.getString(WebappDataStorage.KEY_URL, null));
        assertEquals(scope, mSharedPreferences.getString(WebappDataStorage.KEY_SCOPE, null));
        assertEquals(name, mSharedPreferences.getString(WebappDataStorage.KEY_NAME, null));
        assertEquals(
                shortName, mSharedPreferences.getString(WebappDataStorage.KEY_SHORT_NAME, null));
        assertEquals(encodedIcon, mSharedPreferences.getString(WebappDataStorage.KEY_ICON, null));
        assertEquals(
                WebappConstants.WEBAPP_SHORTCUT_VERSION,
                mSharedPreferences.getInt(WebappDataStorage.KEY_VERSION, 0));
        assertEquals(orientation, mSharedPreferences.getInt(WebappDataStorage.KEY_ORIENTATION, 0));
        assertEquals(themeColor, mSharedPreferences.getLong(WebappDataStorage.KEY_THEME_COLOR, 0));
        assertEquals(
                backgroundColor,
                mSharedPreferences.getLong(WebappDataStorage.KEY_BACKGROUND_COLOR, 0));
        assertEquals(
                isIconGenerated,
                mSharedPreferences.getBoolean(WebappDataStorage.KEY_IS_ICON_GENERATED, true));
        assertEquals(
                isIconAdaptive,
                mSharedPreferences.getBoolean(WebappDataStorage.KEY_IS_ICON_ADAPTIVE, true));

        // Wipe out the data and ensure that it is all gone.
        mSharedPreferences
                .edit()
                .remove(WebappDataStorage.KEY_URL)
                .remove(WebappDataStorage.KEY_SCOPE)
                .remove(WebappDataStorage.KEY_NAME)
                .remove(WebappDataStorage.KEY_SHORT_NAME)
                .remove(WebappDataStorage.KEY_ICON)
                .remove(WebappDataStorage.KEY_VERSION)
                .remove(WebappDataStorage.KEY_ORIENTATION)
                .remove(WebappDataStorage.KEY_THEME_COLOR)
                .remove(WebappDataStorage.KEY_BACKGROUND_COLOR)
                .remove(WebappDataStorage.KEY_IS_ICON_GENERATED)
                .remove(WebappDataStorage.KEY_IS_ICON_ADAPTIVE)
                .apply();

        assertEquals(null, mSharedPreferences.getString(WebappDataStorage.KEY_URL, null));
        assertEquals(null, mSharedPreferences.getString(WebappDataStorage.KEY_SCOPE, null));
        assertEquals(null, mSharedPreferences.getString(WebappDataStorage.KEY_NAME, null));
        assertEquals(null, mSharedPreferences.getString(WebappDataStorage.KEY_SHORT_NAME, null));
        assertEquals(null, mSharedPreferences.getString(WebappDataStorage.KEY_ICON, null));
        assertEquals(0, mSharedPreferences.getInt(WebappDataStorage.KEY_VERSION, 0));
        assertEquals(0, mSharedPreferences.getInt(WebappDataStorage.KEY_ORIENTATION, 0));
        assertEquals(0, mSharedPreferences.getLong(WebappDataStorage.KEY_THEME_COLOR, 0));
        assertEquals(0, mSharedPreferences.getLong(WebappDataStorage.KEY_BACKGROUND_COLOR, 0));
        assertEquals(
                true, mSharedPreferences.getBoolean(WebappDataStorage.KEY_IS_ICON_GENERATED, true));
        assertEquals(
                true, mSharedPreferences.getBoolean(WebappDataStorage.KEY_IS_ICON_ADAPTIVE, true));

        // Update again from the WebappInfo and ensure that the data is restored.
        storage.updateFromWebappIntentDataProvider(intentDataProvider);

        assertEquals(url, mSharedPreferences.getString(WebappDataStorage.KEY_URL, null));
        assertEquals(scope, mSharedPreferences.getString(WebappDataStorage.KEY_SCOPE, null));
        assertEquals(name, mSharedPreferences.getString(WebappDataStorage.KEY_NAME, null));
        assertEquals(
                shortName, mSharedPreferences.getString(WebappDataStorage.KEY_SHORT_NAME, null));
        assertEquals(encodedIcon, mSharedPreferences.getString(WebappDataStorage.KEY_ICON, null));
        assertEquals(
                WebappConstants.WEBAPP_SHORTCUT_VERSION,
                mSharedPreferences.getInt(WebappDataStorage.KEY_VERSION, 0));
        assertEquals(orientation, mSharedPreferences.getInt(WebappDataStorage.KEY_ORIENTATION, 0));
        assertEquals(themeColor, mSharedPreferences.getLong(WebappDataStorage.KEY_THEME_COLOR, 0));
        assertEquals(
                backgroundColor,
                mSharedPreferences.getLong(WebappDataStorage.KEY_BACKGROUND_COLOR, 0));
        assertEquals(
                isIconGenerated,
                mSharedPreferences.getBoolean(WebappDataStorage.KEY_IS_ICON_GENERATED, true));
        assertEquals(
                isIconAdaptive,
                mSharedPreferences.getBoolean(WebappDataStorage.KEY_IS_ICON_GENERATED, true));
    }

    /**
     * Test that the WebAPK's shared preferences are populated as result of calling
     * {@link WebappDataStorage#updateFromWebappIntentDataProvider()} when the shared preferences
     * are initiially unset.
     */
    @Test
    @Feature({"Webapp"})
    public void testWebApkInfoUpdate() {
        String webApkPackageName = "org.chromium.webapk.random123";
        String url = "url";
        String scopeUrl = "scope";
        String manifestUrl = "manifest_url";
        int webApkVersionCode = 5;

        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(webApkPackageName, url)
                        .setScope(scopeUrl)
                        .setManifestUrl(manifestUrl)
                        .setWebApkVersionCode(webApkVersionCode)
                        .build();

        WebappDataStorage storage = WebappDataStorage.open("test");
        storage.updateFromWebappIntentDataProvider(intentDataProvider);

        assertEquals(webApkPackageName, storage.getWebApkPackageName());
        assertEquals(scopeUrl, storage.getScope());
        assertEquals(manifestUrl, storage.getWebApkManifestUrl());
        assertEquals(webApkVersionCode, storage.getWebApkVersionCode());
    }

    /**
     * Test that if the relax-update flag is set to true, the is-update-needed check is done after
     * the relaxed update interval (instead of the usual delay).
     */
    @Test
    public void testRelaxedUpdates() {
        assertTrue(WebappDataStorage.RELAXED_UPDATE_INTERVAL > WebappDataStorage.UPDATE_INTERVAL);

        WebappDataStorage storage = getStorage();

        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        storage.setRelaxedUpdates(true);

        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);
        assertFalse(storage.shouldCheckForUpdate());
        mClockRule.advanceMillis(
                WebappDataStorage.RELAXED_UPDATE_INTERVAL - WebappDataStorage.UPDATE_INTERVAL);
        assertTrue(storage.shouldCheckForUpdate());

        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        storage.setRelaxedUpdates(false);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);
        assertTrue(storage.shouldCheckForUpdate());
    }

    /**
     * Test that the is-update-needed check is done the first time that the user launches the WebAPK
     * after clearing Chrome's storage.
     */
    @Test
    public void testCheckUpdateAfterClearChromeStorage() {
        WebappDataStorage storage = getStorage();
        assertTrue(storage.shouldCheckForUpdate());
    }

    private WebappDataStorage getStorage() {
        return WebappDataStorage.open("test");
    }

    private static Bitmap createBitmap() {
        return Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
    }
}
