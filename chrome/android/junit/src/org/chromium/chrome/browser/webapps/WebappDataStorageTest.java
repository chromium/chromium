// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;

import java.util.concurrent.TimeUnit;

/**
 * Tests the WebappDataStorage class by ensuring that it persists data to
 * SharedPreferences as expected.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {BackgroundShadowAsyncTask.class})
public class WebappDataStorageTest {
    @Rule
    public MockWebappDataStorageClockRule mClockRule = new MockWebappDataStorageClockRule();

    private SharedPreferences mSharedPreferences;
    private boolean mCallbackCalled;

    private class FetchCallback<T> implements WebappDataStorage.FetchCallback<T> {
        T mExpected;

        FetchCallback(T expected) {
            mExpected = expected;
        }

        @Override
        public void onDataRetrieved(T readObject) {
            mCallbackCalled = true;
            assertEquals(mExpected, readObject);
        }
    }

    @Before
    public void setUp() throws Exception {
        mSharedPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "test", Context.MODE_PRIVATE);

        // Set the last_used as if the web app had been registered by WebappRegistry.
        mSharedPreferences.edit().putLong(WebappDataStorage.KEY_LAST_USED, 0).apply();

        mCallbackCalled = false;
    }

    @After
    public void tearDown() {
        mSharedPreferences.edit().clear().apply();
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
        assertEquals("splash_screen_url", WebappDataStorage.KEY_SPLASH_SCREEN_URL);
        assertEquals("source", WebappDataStorage.KEY_SOURCE);
        assertEquals("action", WebappDataStorage.KEY_ACTION);
        assertEquals("is_icon_generated", WebappDataStorage.KEY_IS_ICON_GENERATED);
        assertEquals("version", WebappDataStorage.KEY_VERSION);
    }

    @Test
    @Feature({"Webapp"})
    public void testLastUsedRetrieval() throws Exception {
        long lastUsed = 100;
        mSharedPreferences.edit().putLong(WebappDataStorage.KEY_LAST_USED, lastUsed).apply();
        assertEquals(lastUsed, new WebappDataStorage("test").getLastUsedTimeMs());
    }

    @Test
    @Feature({"Webapp"})
    public void testSplashImageRetrieval() throws Exception {
        final Bitmap expected = createBitmap();
        mSharedPreferences.edit()
                .putString(WebappDataStorage.KEY_SPLASH_ICON,
                        ShortcutHelper.encodeBitmapAsString(expected))
                .apply();
        WebappDataStorage.open("test").getSplashScreenImage(
                new WebappDataStorage.FetchCallback<Bitmap>() {
                    @Override
                    public void onDataRetrieved(Bitmap actual) {
                        mCallbackCalled = true;

                        // TODO(lalitm) - once the Robolectric bug is fixed change to
                        // assertTrue(expected.sameAs(actual)).
                        // See bitmapEquals(Bitmap, Bitmap) for more information.
                        assertTrue(bitmapEquals(expected, actual));
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
        String imageAsString = ShortcutHelper.encodeBitmapAsString(expectedImage);
        WebappDataStorage.open("test").updateSplashScreenImage(imageAsString);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertEquals(imageAsString,
                mSharedPreferences.getString(WebappDataStorage.KEY_SPLASH_ICON, null));
    }

    @Test
    @Feature({"Webapp"})
    public void testScopeRetrieval() throws Exception {
        String scope = "http://drive.google.com";
        mSharedPreferences.edit().putString(WebappDataStorage.KEY_SCOPE, scope).apply();
        assertEquals(scope, new WebappDataStorage("test").getScope());
    }

    @Test
    @Feature({"Webapp"})
    public void testUrlRetrieval() throws Exception {
        String url = "https://www.google.com";
        mSharedPreferences.edit().putString(WebappDataStorage.KEY_URL, url).apply();
        assertEquals(url, new WebappDataStorage("test").getUrl());
    }

    @Test
    @Feature({"Webapp"})
    public void testWasLaunchedRecently() throws Exception {
        // Opening a data storage doesn't count as a launch.
        WebappDataStorage storage = WebappDataStorage.open("test");
        assertTrue(!storage.wasUsedRecently());

        // When the last used time is updated, then it is a launch.
        storage.updateLastUsedTime();
        assertTrue(storage.wasUsedRecently());

        long lastUsedTime = mSharedPreferences.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.TIMESTAMP_INVALID);

        assertTrue(lastUsedTime != WebappDataStorage.TIMESTAMP_INVALID);

        // Move the last used time one day in the past.
        mSharedPreferences.edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, lastUsedTime - TimeUnit.DAYS.toMillis(1L))
                .apply();
        assertTrue(storage.wasUsedRecently());

        // Move the last used time three days in the past.
        mSharedPreferences.edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, lastUsedTime - TimeUnit.DAYS.toMillis(3L))
                .apply();
        assertTrue(storage.wasUsedRecently());

        // Move the last used time one week in the past.
        mSharedPreferences.edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, lastUsedTime - TimeUnit.DAYS.toMillis(7L))
                .apply();
        assertTrue(storage.wasUsedRecently());

        // Move the last used time just under ten days in the past.
        mSharedPreferences.edit().putLong(WebappDataStorage.KEY_LAST_USED,
                lastUsedTime - TimeUnit.DAYS.toMillis(10L) + 1).apply();
        assertTrue(storage.wasUsedRecently());

        // Move the last used time to exactly ten days in the past.
        mSharedPreferences.edit().putLong(WebappDataStorage.KEY_LAST_USED,
                lastUsedTime - TimeUnit.DAYS.toMillis(10L)).apply();
        assertTrue(!storage.wasUsedRecently());
    }

    @Test
    @Feature({"Webapp"})
    public void testIntentUpdate() throws Exception {
        final String id = "id";
        final String action = "action";
        final String url = "url";
        final String scope = "scope";
        final String name = "name";
        final String shortName = "shortName";
        final String encodedIcon = ShortcutHelper.encodeBitmapAsString(createBitmap());
        final @WebDisplayMode int displayMode = WebDisplayMode.STANDALONE;
        final int orientation = 1;
        final long themeColor = 2;
        final long backgroundColor = 3;
        final String splashScreenUrl = "splashy";
        final boolean isIconGenerated = false;
        Intent shortcutIntent = ShortcutHelper.createWebappShortcutIntent(id, action, url, scope,
                name, shortName, encodedIcon, ShortcutHelper.WEBAPP_SHORTCUT_VERSION, displayMode,
                orientation, themeColor, backgroundColor, splashScreenUrl, isIconGenerated);

        WebappDataStorage storage = WebappDataStorage.open("test");
        storage.updateFromShortcutIntent(shortcutIntent);

        assertEquals(action, mSharedPreferences.getString(WebappDataStorage.KEY_ACTION, null));
        assertEquals(url, mSharedPreferences.getString(WebappDataStorage.KEY_URL, null));
        assertEquals(scope, mSharedPreferences.getString(WebappDataStorage.KEY_SCOPE, null));
        assertEquals(name, mSharedPreferences.getString(WebappDataStorage.KEY_NAME, null));
        assertEquals(shortName,
                mSharedPreferences.getString(WebappDataStorage.KEY_SHORT_NAME, null));
        assertEquals(encodedIcon, mSharedPreferences.getString(WebappDataStorage.KEY_ICON, null));
        assertEquals(ShortcutHelper.WEBAPP_SHORTCUT_VERSION,
                mSharedPreferences.getInt(WebappDataStorage.KEY_VERSION, 0));
        assertEquals(orientation, mSharedPreferences.getInt(WebappDataStorage.KEY_ORIENTATION, 0));
        assertEquals(themeColor, mSharedPreferences.getLong(WebappDataStorage.KEY_THEME_COLOR, 0));
        assertEquals(backgroundColor,
                mSharedPreferences.getLong(WebappDataStorage.KEY_BACKGROUND_COLOR, 0));
        assertEquals(splashScreenUrl,
                mSharedPreferences.getString(WebappDataStorage.KEY_SPLASH_SCREEN_URL, null));
        assertEquals(isIconGenerated,
                mSharedPreferences.getBoolean(WebappDataStorage.KEY_IS_ICON_GENERATED, true));

        // Wipe out the data and ensure that it is all gone.
        mSharedPreferences.edit()
                .remove(WebappDataStorage.KEY_ACTION)
                .remove(WebappDataStorage.KEY_URL)
                .remove(WebappDataStorage.KEY_SCOPE)
                .remove(WebappDataStorage.KEY_NAME)
                .remove(WebappDataStorage.KEY_SHORT_NAME)
                .remove(WebappDataStorage.KEY_ICON)
                .remove(WebappDataStorage.KEY_VERSION)
                .remove(WebappDataStorage.KEY_ORIENTATION)
                .remove(WebappDataStorage.KEY_THEME_COLOR)
                .remove(WebappDataStorage.KEY_BACKGROUND_COLOR)
                .remove(WebappDataStorage.KEY_SPLASH_SCREEN_URL)
                .remove(WebappDataStorage.KEY_IS_ICON_GENERATED)
                .apply();

        assertEquals(null, mSharedPreferences.getString(WebappDataStorage.KEY_ACTION, null));
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
                null, mSharedPreferences.getString(WebappDataStorage.KEY_SPLASH_SCREEN_URL, null));
        assertEquals(true,
                mSharedPreferences.getBoolean(WebappDataStorage.KEY_IS_ICON_GENERATED, true));

        // Update again from the intent and ensure that the data is restored.
        storage.updateFromShortcutIntent(shortcutIntent);

        assertEquals(action, mSharedPreferences.getString(WebappDataStorage.KEY_ACTION, null));
        assertEquals(url, mSharedPreferences.getString(WebappDataStorage.KEY_URL, null));
        assertEquals(scope, mSharedPreferences.getString(WebappDataStorage.KEY_SCOPE, null));
        assertEquals(name, mSharedPreferences.getString(WebappDataStorage.KEY_NAME, null));
        assertEquals(shortName,
                mSharedPreferences.getString(WebappDataStorage.KEY_SHORT_NAME, null));
        assertEquals(encodedIcon, mSharedPreferences.getString(WebappDataStorage.KEY_ICON, null));
        assertEquals(ShortcutHelper.WEBAPP_SHORTCUT_VERSION,
                mSharedPreferences.getInt(WebappDataStorage.KEY_VERSION, 0));
        assertEquals(orientation, mSharedPreferences.getInt(WebappDataStorage.KEY_ORIENTATION, 0));
        assertEquals(themeColor, mSharedPreferences.getLong(WebappDataStorage.KEY_THEME_COLOR, 0));
        assertEquals(backgroundColor,
                mSharedPreferences.getLong(WebappDataStorage.KEY_BACKGROUND_COLOR, 0));
        assertEquals(splashScreenUrl,
                mSharedPreferences.getString(WebappDataStorage.KEY_SPLASH_SCREEN_URL, null));
        assertEquals(isIconGenerated,
                mSharedPreferences.getBoolean(WebappDataStorage.KEY_IS_ICON_GENERATED, true));
    }

    /**
     * Test that if the WebAPK update failed (e.g. because the WebAPK server is not reachable) that
     * the is-update-needed check is retried after less time than if the WebAPK update had
     * succeeded. The is-update-needed check is the first step in retrying to update the WebAPK.
     */
    @Test
    public void testCheckUpdateMoreFrequentlyIfUpdateFails() {
        assertTrue(WebappDataStorage.UPDATE_INTERVAL > WebappDataStorage.RETRY_UPDATE_DURATION);

        WebappDataStorage storage = getStorage();

        storage.updateTimeOfLastWebApkUpdateRequestCompletion();
        storage.updateDidLastWebApkUpdateRequestSucceed(true);

        assertFalse(storage.shouldCheckForUpdate());
        mClockRule.advance(WebappDataStorage.RETRY_UPDATE_DURATION);
        assertFalse(storage.shouldCheckForUpdate());

        // Advance all of the time stamps.
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        storage.updateTimeOfLastWebApkUpdateRequestCompletion();
        storage.updateDidLastWebApkUpdateRequestSucceed(false);

        assertFalse(storage.shouldCheckForUpdate());
        mClockRule.advance(WebappDataStorage.RETRY_UPDATE_DURATION);
        assertTrue(storage.shouldCheckForUpdate());

        // Verifies that {@link WebappDataStorage#shouldCheckForUpdate()} returns true because the
        // previous update failed, no matter whether we want to check update less frequently.
        storage.setRelaxedUpdates(true);
        assertTrue(storage.shouldCheckForUpdate());
    }

    /**
     * Test that if there was no previous WebAPK update attempt that the is-update-needed check is
     * done after the usual delay (as opposed to the shorter delay if the previous WebAPK update
     * failed.)
     */
    @Test
    public void testRegularCheckIntervalIfNoPriorWebApkUpdate() {
        assertTrue(WebappDataStorage.UPDATE_INTERVAL > WebappDataStorage.RETRY_UPDATE_DURATION);

        WebappDataStorage storage = getStorage();

        assertFalse(storage.shouldCheckForUpdate());
        mClockRule.advance(WebappDataStorage.RETRY_UPDATE_DURATION);
        assertFalse(storage.shouldCheckForUpdate());
        mClockRule.advance(
                WebappDataStorage.UPDATE_INTERVAL - WebappDataStorage.RETRY_UPDATE_DURATION);
        assertTrue(storage.shouldCheckForUpdate());
    }

    /**
     * Test that if there was no previous WebAPK update attempt and the relax-update flag is set to
     * true, the is-update-needed check is done after the relaxed update interval (as opposed to the
     * usual delay.)
     */
    @Test
    public void testRelaxedUpdates() {
        assertTrue(WebappDataStorage.RELAXED_UPDATE_INTERVAL > WebappDataStorage.UPDATE_INTERVAL);

        WebappDataStorage storage = getStorage();

        storage.setRelaxedUpdates(true);

        mClockRule.advance(WebappDataStorage.UPDATE_INTERVAL);
        assertFalse(storage.shouldCheckForUpdate());
        mClockRule.advance(
                WebappDataStorage.RELAXED_UPDATE_INTERVAL - WebappDataStorage.UPDATE_INTERVAL);
        assertTrue(storage.shouldCheckForUpdate());
    }

    private WebappDataStorage getStorage() {
        WebappDataStorage storage = WebappDataStorage.open("test");

        // Done when WebAPK is registered in {@link WebApkActivity}.
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        return storage;
    }

    // TODO(lalitm) - There seems to be a bug in Robolectric where a Bitmap
    // produced from a byte stream is hardcoded to be a 100x100 bitmap with
    // ARGB_8888 pixel format. Because of this, we need to work around the
    // equality check of bitmaps. Remove this once the bug is fixed.
    private static boolean bitmapEquals(Bitmap expected, Bitmap actual) {
        if (actual.getWidth() != 100) return false;
        if (actual.getHeight() != 100) return false;
        if (!actual.getConfig().equals(Bitmap.Config.ARGB_8888)) return false;

        for (int i = 0; i < actual.getWidth(); i++) {
            for (int j = 0; j < actual.getHeight(); j++) {
                if (actual.getPixel(i, j) != 0) return false;
            }
        }
        return true;
    }

    private static Bitmap createBitmap() {
        return Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
    }
}
