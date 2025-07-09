// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
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

/** Unit tests for {@link NtpCustomizationConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
/** Unit test for {@link NtpCustomizationConfigManager}. */
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
    public void testAddAndRemoveListener() {
        // Verifies that onBackgroundChanged() is called for the listener when it is added.
        mNtpCustomizationConfigManager.addListener(mListener);
        verify(mListener).onBackgroundChanged(eq(null));
        clearInvocations(mListener);

        Bitmap bitmap = createBitmap();
        mNtpCustomizationConfigManager.onBackgroundChanged(bitmap);
        verify(mListener).onBackgroundChanged(mBitmapDrawableCaptor.capture());
        assertEquals(bitmap, mBitmapDrawableCaptor.getValue().getBitmap());

        clearInvocations(mListener);
        mNtpCustomizationConfigManager.removeListener(mListener);
        mNtpCustomizationConfigManager.onBackgroundChanged(null);
        verify(mListener, never()).onBackgroundChanged(any());
    }

    private Bitmap createBitmap() {
        return Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    }
}
