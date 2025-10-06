// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.THEME_COLLECTION;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

/** Unit tests for {@link NtpBackgroundImageView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpBackgroundImageViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Bitmap mBitmap;
    private Matrix mPortraitMatrix;
    private Matrix mLandscapeMatrix;
    private BackgroundImageInfo mBackgroundImageInfo;
    private NtpBackgroundImageView mBackgroundImageViewSpy;
    @Captor ArgumentCaptor<DisplayStyleObserver> mDisplayStyleObserverArgumentCaptor;
    @Mock UiConfig mUiConfig;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        mPortraitMatrix = new Matrix();
        mLandscapeMatrix = new Matrix();
        mLandscapeMatrix.setScale(2.0f, 2.0f); // Make it different from portrait.
        mBackgroundImageInfo = new BackgroundImageInfo(mPortraitMatrix, mLandscapeMatrix);
        mBackgroundImageViewSpy = spy(new NtpBackgroundImageView(activity, mUiConfig));
    }

    @Test
    public void testSetBackground_withChromeTheme() {
        mBackgroundImageViewSpy.setBackground(mBitmap, null, THEME_COLLECTION);

        verify(mBackgroundImageViewSpy).setScaleType(eq(ImageView.ScaleType.CENTER_CROP));
        verify(mBackgroundImageViewSpy).setImageBitmap(eq(mBitmap));
        verify(mBackgroundImageViewSpy, never()).setImageBackgroundWithMatrices();
    }

    @Test
    public void testSetBackground_withImageFromDisk() {
        mBackgroundImageViewSpy.setBackground(mBitmap, mBackgroundImageInfo, IMAGE_FROM_DISK);

        verify(mBackgroundImageViewSpy).setScaleType(eq(ImageView.ScaleType.MATRIX));
        verify(mBackgroundImageViewSpy).setImageBitmap(eq(mBitmap));
        verify(mBackgroundImageViewSpy).setImageBackgroundWithMatrices();
    }

    @Test
    @Config(qualifiers = "port")
    public void testSetImageBackgroundWithMatrices_portrait() {
        mBackgroundImageViewSpy.setBackground(mBitmap, mBackgroundImageInfo, IMAGE_FROM_DISK);

        verify(mBackgroundImageViewSpy).setImageMatrix(eq(mPortraitMatrix));
        verify(mBackgroundImageViewSpy, never()).setImageMatrix(eq(mLandscapeMatrix));
    }

    @Test
    @Config(qualifiers = "land")
    public void testSetImageBackgroundWithMatrices_landscape() {
        mBackgroundImageViewSpy.setBackground(mBitmap, mBackgroundImageInfo, IMAGE_FROM_DISK);

        verify(mBackgroundImageViewSpy).setImageMatrix(eq(mLandscapeMatrix));
        verify(mBackgroundImageViewSpy, never()).setImageMatrix(eq(mPortraitMatrix));
    }

    @Test
    @Config(qualifiers = "port")
    public void testSetImageBackgroundWithMatrices_doesNothingIfBitmapIsNull() {
        mBackgroundImageViewSpy.setBackground(null, mBackgroundImageInfo, IMAGE_FROM_DISK);

        verify(mBackgroundImageViewSpy, never()).setImageMatrix(eq(mPortraitMatrix));
    }

    @Test
    @Config(qualifiers = "port")
    public void testSetImageBackgroundWithMatrices_doesNothingIfInfoIsNull() {
        mBackgroundImageViewSpy.setBackground(mBitmap, null, IMAGE_FROM_DISK);

        verify(mBackgroundImageViewSpy, never()).setImageMatrix(eq(mPortraitMatrix));
    }

    @Test
    public void testDestroy_removesObserver() {
        verify(mUiConfig).addObserver(mDisplayStyleObserverArgumentCaptor.capture());

        // Call destroy() on the view.
        mBackgroundImageViewSpy.destroy();

        // Verify that UiConfig.removeObserver() was called with the exact same observer instance.
        verify(mUiConfig).removeObserver(eq(mDisplayStyleObserverArgumentCaptor.getValue()));
    }
}
