// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

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
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mNtpCustomizationConfigManager = NtpCustomizationConfigManager.getInstance();
    }

    @After
    public void tearDown() {
        // Clean up listeners to not affect other tests.
        mNtpCustomizationConfigManager.removeListener(mListener);
    }

    @Test
    public void testOnBackgroundChanged_withBitmap() {
        mNtpCustomizationConfigManager.addListener(mListener);
        Bitmap bitmap = createBitmap();

        mNtpCustomizationConfigManager.onBackgroundChanged(mContext, bitmap);

        assertNotNull(mNtpCustomizationConfigManager.getBackgroundImageDrawable());
        assertEquals(
                bitmap, mNtpCustomizationConfigManager.getBackgroundImageDrawable().getBitmap());

        verify(mListener).onBackgroundChanged(mBitmapDrawableCaptor.capture());
        assertNotNull(mBitmapDrawableCaptor.getValue());
        assertEquals(bitmap, mBitmapDrawableCaptor.getValue().getBitmap());
    }

    @Test
    public void testOnBackgroundChanged_withNullBitmap() {
        mNtpCustomizationConfigManager.addListener(mListener);

        mNtpCustomizationConfigManager.onBackgroundChanged(mContext, null);

        assertNull(mNtpCustomizationConfigManager.getBackgroundImageDrawable());
        verify(mListener).onBackgroundChanged(mBitmapDrawableCaptor.capture());
        assertNull(mBitmapDrawableCaptor.getValue());
    }

    @Test
    public void testAddAndRemoveListener() {
        mNtpCustomizationConfigManager.addListener(mListener);
        Bitmap bitmap = createBitmap();

        mNtpCustomizationConfigManager.onBackgroundChanged(mContext, bitmap);
        verify(mListener).onBackgroundChanged(mBitmapDrawableCaptor.capture());

        mNtpCustomizationConfigManager.removeListener(mListener);
        mNtpCustomizationConfigManager.onBackgroundChanged(mContext, null);
        verify(mListener).onBackgroundChanged(mBitmapDrawableCaptor.capture());
    }

    private Bitmap createBitmap() {
        return Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    }
}
