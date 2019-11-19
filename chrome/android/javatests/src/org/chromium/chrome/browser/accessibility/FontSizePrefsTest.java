// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.accessibility.FontSizePrefs.FontSizePrefsObserver;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link FontSizePrefs}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class FontSizePrefsTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final float EPSILON = 0.001f;
    private FontSizePrefs mFontSizePrefs;

    @Before
    public void setUp() {
        resetSharedPrefs();
        Context context = InstrumentationRegistry.getTargetContext();
        mFontSizePrefs = getFontSizePrefs(context);
        setSystemFontScaleForTest(1.0f);
    }

    private void resetSharedPrefs() {
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.remove(FontSizePrefs.PREF_USER_SET_FORCE_ENABLE_ZOOM);
        editor.remove(FontSizePrefs.PREF_USER_FONT_SCALE_FACTOR);
        editor.apply();
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
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.remove(FontSizePrefs.PREF_USER_FONT_SCALE_FACTOR).apply();

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
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                Assert.assertEquals(getUserFontScaleFactor(), mUserFontScaleFactor, EPSILON);
                Assert.assertEquals(getFontScaleFactor(), mFontScaleFactor, EPSILON);
                Assert.assertEquals(getForceEnableZoom(), mForceEnableZoom);
            });
        }
    }

    private FontSizePrefs getFontSizePrefs(final Context context) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> FontSizePrefs.getInstance());
    }

    private TestingObserver createAndAddFontSizePrefsObserver() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            TestingObserver observer = new TestingObserver();
            mFontSizePrefs.addObserver(observer);
            return observer;
        });
    }

    private void setUserFontScaleFactor(final float fontsize) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mFontSizePrefs.setUserFontScaleFactor(fontsize));
    }

    private float getUserFontScaleFactor() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mFontSizePrefs.getUserFontScaleFactor());
    }

    private float getFontScaleFactor() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mFontSizePrefs.getFontScaleFactor());
    }

    private void setForceEnableZoomFromUser(final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mFontSizePrefs.setForceEnableZoomFromUser(enabled));
    }

    private boolean getForceEnableZoom() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mFontSizePrefs.getForceEnableZoom());
    }

    private void setSystemFontScaleForTest(final float systemFontScale) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFontSizePrefs.setSystemFontScaleForTest(systemFontScale);
            mFontSizePrefs.onSystemFontScaleChanged();
        });
    }
}
