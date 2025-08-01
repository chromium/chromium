// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager.HomepageStateListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link NtpCustomizationConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpCustomizationConfigManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private HomepageStateListener mListener;
    @Captor private ArgumentCaptor<BitmapDrawable> mBitmapDrawableCaptor;

    private NtpCustomizationConfigManager mNtpCustomizationConfigManager;

    @Before
    public void setUp() {
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
        verify(mListener).onBackgroundChanged(eq(null));
        clearInvocations(mListener);

        Bitmap bitmap = createBitmap();
        mNtpCustomizationConfigManager.onBackgroundChanged(bitmap);

        assertNotNull(mNtpCustomizationConfigManager.getBackgroundImageDrawable());
        assertEquals(
                bitmap, mNtpCustomizationConfigManager.getBackgroundImageDrawable().getBitmap());

        verify(mListener).onBackgroundChanged(mBitmapDrawableCaptor.capture());
        assertEquals(bitmap, mBitmapDrawableCaptor.getValue().getBitmap());
    }

    @Test
    public void testOnBackgroundChanged_withNullBitmap() {
        mNtpCustomizationConfigManager.addListener(mListener);
        verify(mListener).onBackgroundChanged(eq(null));
        clearInvocations(mListener);

        mNtpCustomizationConfigManager.onBackgroundChanged(null);

        verify(mListener).onBackgroundChanged(eq(null));
    }

    @Test
    public void testAddAndRemoveBackgroundChangeListener() {
        // Verifies that onBackgroundChanged() is called for the listener when it is added.
        mNtpCustomizationConfigManager.addListener(mListener);
        verify(mListener).onBackgroundChanged(eq(null));
        clearInvocations(mListener);

        Bitmap bitmap = createBitmap();
        mNtpCustomizationConfigManager.onBackgroundChanged(bitmap);
        verify(mListener).onBackgroundChanged(mBitmapDrawableCaptor.capture());
        assertEquals(bitmap, mBitmapDrawableCaptor.getValue().getBitmap());
        assertEquals(
                NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK,
                mNtpCustomizationConfigManager.getBackgroundImageType());

        clearInvocations(mListener);
        mNtpCustomizationConfigManager.removeListener(mListener);
        mNtpCustomizationConfigManager.onBackgroundChanged(null);
        verify(mListener, never()).onBackgroundChanged(any());
        assertEquals(
                NtpCustomizationUtils.NtpBackgroundImageType.DEFAULT,
                mNtpCustomizationConfigManager.getBackgroundImageType());
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

    private Bitmap createBitmap() {
        return Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    }
}
