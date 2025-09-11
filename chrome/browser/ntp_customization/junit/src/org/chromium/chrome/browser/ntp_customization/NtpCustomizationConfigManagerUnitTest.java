// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.core.content.ContextCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager.HomepageStateListener;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link NtpCustomizationConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpCustomizationConfigManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private HomepageStateListener mListener;
    @Captor private ArgumentCaptor<BitmapDrawable> mBitmapDrawableCaptor;

    private Context mContext;
    private NtpCustomizationConfigManager mNtpCustomizationConfigManager;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mNtpCustomizationConfigManager = NtpCustomizationConfigManager.getInstance());
    }

    @After
    public void tearDown() {
        // Clean up listeners to not affect other tests.
        mNtpCustomizationConfigManager.removeListener(mListener);
        mNtpCustomizationConfigManager.setBackgroundImageDrawableForTesting(null);

        // Removes the newly generated file and cleans up SharedPreference.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        NtpCustomizationUtils.deleteBackgroundImageFileImpl();
    }

    @Test
    public void testOnBackgroundChanged_withBitmap() {
        mNtpCustomizationConfigManager.addListener(mListener);
        clearInvocations(mListener);

        mNtpCustomizationConfigManager.setBackgroundImageTypeFroTesting(
                NtpBackgroundImageType.DEFAULT);
        Bitmap bitmap = createBitmap();
        mNtpCustomizationConfigManager.onBackgroundChanged(bitmap);

        assertNotNull(mNtpCustomizationConfigManager.getBackgroundImageDrawable());
        assertEquals(
                bitmap, mNtpCustomizationConfigManager.getBackgroundImageDrawable().getBitmap());

        verify(mListener)
                .onBackgroundChanged(
                        mBitmapDrawableCaptor.capture(),
                        eq(false),
                        eq(NtpBackgroundImageType.DEFAULT),
                        eq(NtpBackgroundImageType.IMAGE_FROM_DISK));
        assertEquals(bitmap, mBitmapDrawableCaptor.getValue().getBitmap());
    }

    @Test
    public void testAddAndRemoveBackgroundChangeListener() {
        // Verifies that onBackgroundChanged() is called for the listener when it is added.
        mNtpCustomizationConfigManager.addListener(mListener);
        clearInvocations(mListener);

        Bitmap bitmap = createBitmap();
        mNtpCustomizationConfigManager.onBackgroundChanged(bitmap);
        verify(mListener)
                .onBackgroundChanged(
                        mBitmapDrawableCaptor.capture(),
                        eq(false),
                        eq(NtpBackgroundImageType.DEFAULT),
                        eq(NtpBackgroundImageType.IMAGE_FROM_DISK));
        assertEquals(bitmap, mBitmapDrawableCaptor.getValue().getBitmap());
        assertEquals(
                NtpBackgroundImageType.IMAGE_FROM_DISK,
                mNtpCustomizationConfigManager.getBackgroundImageType());

        clearInvocations(mListener);
        mNtpCustomizationConfigManager.removeListener(mListener);
        mNtpCustomizationConfigManager.onBackgroundChanged(bitmap);
        verify(mListener, never()).onBackgroundChanged(any(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testAddAndRemoveMvtVisibilityListener() {
        // Verifies the listener added is notified when the visibility if changed.
        mNtpCustomizationConfigManager.addListener(mListener);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ true);
        verify(mListener).onMvtToggleChanged();

        // Removes listener and verifies it's not called.
        clearInvocations(mListener);
        mNtpCustomizationConfigManager.removeListener(mListener);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ true);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ false);
        verify(mListener, never()).onMvtToggleChanged();
    }

    @Test
    public void testSetAndGetPrefMvtVisibility() {
        // Verifies setPrefIsMvtVisible() sets the ChromeSharedPreferences properly and
        // getPrefIsMvtVisible() gets the right value.
        mNtpCustomizationConfigManager.addListener(mListener);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ false);
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.IS_MVT_VISIBLE, /* defaultValue= */ true));
        assertFalse(mNtpCustomizationConfigManager.getPrefIsMvtToggleOn());
        verify(mListener).onMvtToggleChanged();

        clearInvocations(mListener);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ true);
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.IS_MVT_VISIBLE, /* defaultValue= */ true));
        assertTrue(mNtpCustomizationConfigManager.getPrefIsMvtToggleOn());
        verify(mListener).onMvtToggleChanged();
    }

    @Test
    public void testDefaultPrefMvtVisibility() {
        // Verifies the default value is true.
        assertTrue(mNtpCustomizationConfigManager.getPrefIsMvtToggleOn());
    }

    @Test
    public void testOnBackgroundColorChanged() {
        mNtpCustomizationConfigManager.addListener(mListener);
        clearInvocations(mListener);

        @ColorRes int colorResId = R.color.default_red;
        NtpThemeColorInfo colorInfo =
                new NtpThemeColorInfo(
                        mContext,
                        NtpThemeColorInfo.NtpThemeColorId.BLUE,
                        colorResId,
                        R.color.default_bg_color_blue,
                        R.drawable.chrome_color_theme_icon_blue);
        @ColorInt int color = ContextCompat.getColor(mContext, colorResId);
        @ColorInt
        int defaultColor = ContextCompat.getColor(mContext, R.color.home_surface_background_color);

        assertEquals(
                defaultColor, mNtpCustomizationConfigManager.getDefaultBackgroundColor(mContext));
        mNtpCustomizationConfigManager.setBackgroundImageTypeFroTesting(
                NtpBackgroundImageType.DEFAULT);

        // Test case for choosing a new customized color.
        mNtpCustomizationConfigManager.onBackgroundColorChanged(
                mContext, colorInfo, NtpBackgroundImageType.CHROME_COLOR);
        assertEquals(color, mNtpCustomizationConfigManager.getBackgroundColor(mContext));
        assertEquals(
                color, NtpCustomizationUtils.getBackgroundColorFromSharedPreference(defaultColor));
        verify(mListener)
                .onBackgroundColorChanged(
                        eq(color),
                        eq(false),
                        eq(NtpBackgroundImageType.DEFAULT),
                        eq(NtpBackgroundImageType.CHROME_COLOR));

        clearInvocations(mListener);
        // Test case for resetting to the default color.
        mNtpCustomizationConfigManager.onBackgroundColorChanged(
                mContext, /* colorInfo= */ null, NtpBackgroundImageType.DEFAULT);
        assertEquals(defaultColor, mNtpCustomizationConfigManager.getBackgroundColor(mContext));

        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        assertFalse(prefsManager.contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR));

        verify(mListener)
                .onBackgroundColorChanged(
                        eq(defaultColor),
                        eq(false),
                        eq(NtpBackgroundImageType.CHROME_COLOR),
                        eq(NtpBackgroundImageType.DEFAULT));
    }

    private Bitmap createBitmap() {
        return Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    }
}
