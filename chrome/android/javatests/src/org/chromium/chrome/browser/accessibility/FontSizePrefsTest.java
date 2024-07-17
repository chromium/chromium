// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs.FontSizePrefsObserver;

/**
 * Tests for {@link FontSizePrefs}.
 *
 * <p>TODO(crbug.com/40214849): This tests the class in //components/browser_ui, but we don't have a
 * good way of testing with native code there.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class FontSizePrefsTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final float EPSILON = 0.001f;
    private FontSizePrefs mFontSizePrefs;

    @Before
    public void setUp() {
        resetSharedPrefs();
        Context context = ApplicationProvider.getApplicationContext();
        mFontSizePrefs = getFontSizePrefs(context);
        setSystemFontScaleForTest(1.0f);
    }

    private void resetSharedPrefs() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.removeKey(ChromePreferenceKeys.FONT_USER_SET_FORCE_ENABLE_ZOOM);
        prefs.removeKey(ChromePreferenceKeys.FONT_USER_FONT_SCALE_FACTOR);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testForceEnableZoom() {
        Assert.assertEquals(false, getForceEnableZoom());

        TestingObserver observer = createAndAddFontSizePrefsObserver();

        setUserFontScaleFactor(1.5f);
        Assert.assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(0.7f);
        Assert.assertEquals(false, getForceEnableZoom());
        observer.assertConsistent();

        setForceEnableZoomFromUser(true);
        Assert.assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(1.5f);
        Assert.assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(0.7f);
        Assert.assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();

        setForceEnableZoomFromUser(false);
        Assert.assertEquals(false, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(1.5f);
        Assert.assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setUserFontScaleFactor(0.7f);
        Assert.assertEquals(false, getForceEnableZoom());
        observer.assertConsistent();

        // Force enable zoom should depend on fontScaleFactor, not on userFontScaleFactor.
        setSystemFontScaleForTest(2.0f);
        Assert.assertEquals(true, getForceEnableZoom());
        observer.assertConsistent();
        setSystemFontScaleForTest(1.0f);
        Assert.assertEquals(false, getForceEnableZoom());
        observer.assertConsistent();
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testFontScaleFactor() {
        Assert.assertEquals(1f, getUserFontScaleFactor(), EPSILON);
        Assert.assertEquals(1f, getFontScaleFactor(), EPSILON);

        TestingObserver observer = createAndAddFontSizePrefsObserver();

        setUserFontScaleFactor(1.5f);
        Assert.assertEquals(1.5f, getUserFontScaleFactor(), EPSILON);
        Assert.assertEquals(1.5f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        setUserFontScaleFactor(0.7f);
        Assert.assertEquals(0.7f, getUserFontScaleFactor(), EPSILON);
        Assert.assertEquals(0.7f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        // Force enable zoom shouldn't affect font scale factor.
        setForceEnableZoomFromUser(true);
        Assert.assertEquals(0.7f, getUserFontScaleFactor(), EPSILON);
        Assert.assertEquals(0.7f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        setForceEnableZoomFromUser(false);
        Assert.assertEquals(0.7f, getUserFontScaleFactor(), EPSILON);
        Assert.assertEquals(0.7f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        // System font scale should affect fontScaleFactor, but not userFontScaleFactor.
        setSystemFontScaleForTest(1.3f);
        Assert.assertEquals(0.7f, getUserFontScaleFactor(), EPSILON);
        Assert.assertEquals(0.7f * 1.3f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        setUserFontScaleFactor(1.5f);
        Assert.assertEquals(1.5f, getUserFontScaleFactor(), EPSILON);
        Assert.assertEquals(1.5f * 1.3f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();

        setSystemFontScaleForTest(0.8f);
        Assert.assertEquals(1.5f, getUserFontScaleFactor(), EPSILON);
        Assert.assertEquals(1.5f * 0.8f, getFontScaleFactor(), EPSILON);
        observer.assertConsistent();
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testUpgradeToUserFontScaleFactor() {
        setSystemFontScaleForTest(1.3f);
        setUserFontScaleFactor(1.5f);

        // Delete PREF_USER_FONT_SCALE_FACTOR. This simulates the condition just after upgrading to
        // M51, when userFontScaleFactor was added.
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.removeKey(ChromePreferenceKeys.FONT_USER_FONT_SCALE_FACTOR);

        // Intial userFontScaleFactor should be set to fontScaleFactor / systemFontScale.
        Assert.assertEquals(1.5f, getUserFontScaleFactor(), EPSILON);
        Assert.assertEquals(1.5f * 1.3f, getFontScaleFactor(), EPSILON);
    }

    private class TestingObserver implements FontSizePrefsObserver {
        private float mUserFontScaleFactor = getUserFontScaleFactor();
        private float mFontScaleFactor = getFontScaleFactor();
        private boolean mForceEnableZoom = getForceEnableZoom();

        @Override
        public void onFontScaleFactorChanged(float fontScaleFactor, float userFontScaleFactor) {
            mFontScaleFactor = fontScaleFactor;
            mUserFontScaleFactor = userFontScaleFactor;
        }

        @Override
        public void onForceEnableZoomChanged(boolean enabled) {
            mForceEnableZoom = enabled;
        }

        private void assertConsistent() {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Assert.assertEquals(
                                getUserFontScaleFactor(), mUserFontScaleFactor, EPSILON);
                        Assert.assertEquals(getFontScaleFactor(), mFontScaleFactor, EPSILON);
                        Assert.assertEquals(getForceEnableZoom(), mForceEnableZoom);
                    });
        }
    }

    private FontSizePrefs getFontSizePrefs(final Context context) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> FontSizePrefs.getInstance(ProfileManager.getLastUsedRegularProfile()));
    }

    private TestingObserver createAndAddFontSizePrefsObserver() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TestingObserver observer = new TestingObserver();
                    mFontSizePrefs.addObserver(observer);
                    return observer;
                });
    }

    private void setUserFontScaleFactor(final float fontsize) {
        ThreadUtils.runOnUiThreadBlocking(() -> mFontSizePrefs.setUserFontScaleFactor(fontsize));
    }

    private float getUserFontScaleFactor() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mFontSizePrefs.getUserFontScaleFactor());
    }

    private float getFontScaleFactor() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mFontSizePrefs.getFontScaleFactor());
    }

    private void setForceEnableZoomFromUser(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(() -> mFontSizePrefs.setForceEnableZoomFromUser(enabled));
    }

    private boolean getForceEnableZoom() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mFontSizePrefs.getForceEnableZoom());
    }

    private void setSystemFontScaleForTest(final float systemFontScale) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFontSizePrefs.setSystemFontScaleForTest(systemFontScale);
                    mFontSizePrefs.onSystemFontScaleChanged();
                });
    }
}
