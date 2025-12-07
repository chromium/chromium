// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.ColorInt;

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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link NtpBackgroundImageCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundImageCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    private static final @ColorInt int COLOR = Color.BLACK;

    @Mock private ViewGroup mRootView;
    @Mock private UiConfig mUiConfig;
    @Mock private Bitmap mBitmap;
    @Captor ArgumentCaptor<DisplayStyleObserver> mDisplayStyleObserverArgumentCaptor;

    private Activity mActivity;
    private NtpBackgroundImageCoordinator mCoordinator;
    private PropertyModel mPropertyModel;
    private Matrix mPortraitMatrix;
    private Matrix mLandscapeMatrix;
    private BackgroundImageInfo mBackgroundImageInfo;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mPortraitMatrix = new Matrix();
        mLandscapeMatrix = new Matrix();
        mLandscapeMatrix.setScale(2.0f, 2.0f); // Make it different from portrait.
        mBackgroundImageInfo = new BackgroundImageInfo(mPortraitMatrix, mLandscapeMatrix);

        mCoordinator = new NtpBackgroundImageCoordinator(mActivity, mRootView, mUiConfig, COLOR);
        mPropertyModel = mCoordinator.getPropertyModelForTesting();
    }

    @Test
    public void testConstructor() {
        verify(mUiConfig, never()).addObserver(any(DisplayStyleObserver.class));
        assertEquals(COLOR, mPropertyModel.get(NtpBackgroundImageProperties.BACKGROUND_COLOR));
        verify(mRootView).addView(any(View.class));
    }

    @Test
    public void testMaybeAddDisplayStyleObserver() {
        verify(mUiConfig, never()).addObserver(any(DisplayStyleObserver.class));

        mCoordinator.setBackground(
                mBitmap, mBackgroundImageInfo, NtpBackgroundImageType.THEME_COLLECTION);
        verify(mUiConfig).addObserver(any(DisplayStyleObserver.class));

        clearInvocations(mUiConfig);
        // Verifies observer won't be added again.
        mCoordinator.setBackground(
                mBitmap, mBackgroundImageInfo, NtpBackgroundImageType.IMAGE_FROM_DISK);
        verify(mUiConfig, never()).addObserver(any(DisplayStyleObserver.class));
    }

    @Test
    public void testSetBackground() {
        // Background type is IMAGE_FROM_DISK:
        mCoordinator.setBackground(
                mBitmap, mBackgroundImageInfo, NtpBackgroundImageType.IMAGE_FROM_DISK);
        assertEquals(mBitmap, mPropertyModel.get(NtpBackgroundImageProperties.BACKGROUND_IMAGE));
        assertEquals(
                ImageView.ScaleType.MATRIX,
                mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_SCALE_TYPE));

        // Background type is THEME_COLLECTION:
        mCoordinator.setBackground(
                mBitmap, mBackgroundImageInfo, NtpBackgroundImageType.THEME_COLLECTION);
        assertEquals(mBitmap, mPropertyModel.get(NtpBackgroundImageProperties.BACKGROUND_IMAGE));
        assertEquals(
                ImageView.ScaleType.MATRIX,
                mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_SCALE_TYPE));
    }

    @Test
    public void testCleanBackground() {
        mCoordinator.clearBackground();

        assertNull(mPropertyModel.get(NtpBackgroundImageProperties.BACKGROUND_IMAGE));
    }

    @Test
    public void testDestroy() {
        mCoordinator.setBackground(
                mBitmap, mBackgroundImageInfo, NtpBackgroundImageType.THEME_COLLECTION);
        verify(mUiConfig).addObserver(mDisplayStyleObserverArgumentCaptor.capture());
        mCoordinator.destroy();

        // Verify that UiConfig.removeObserver() was called with the exact same observer instance.
        verify(mUiConfig).removeObserver(eq(mDisplayStyleObserverArgumentCaptor.getValue()));
    }

    @Test
    public void testDestroy_noObserverAddedBefore() {
        verify(mUiConfig, never()).addObserver(any(DisplayStyleObserver.class));
        mCoordinator.destroy();

        // Verify that UiConfig.removeObserver() wasn't called since no observer was added before.
        verify(mUiConfig, never()).removeObserver(any(DisplayStyleObserver.class));
    }

    @Test
    @Config(qualifiers = "port")
    public void testSetImageBackgroundWithMatrices_portrait() {
        mCoordinator.setBackground(mBitmap, mBackgroundImageInfo, IMAGE_FROM_DISK);

        assertEquals(
                mPortraitMatrix, mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_MATRIX));
    }

    @Test
    @Config(qualifiers = "land")
    public void testSetImageBackgroundWithMatrices_landscape() {
        mCoordinator.setBackground(mBitmap, mBackgroundImageInfo, IMAGE_FROM_DISK);

        assertEquals(
                mLandscapeMatrix, mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_MATRIX));
    }

    @Test
    @Config(qualifiers = "port")
    public void testSetImageBackgroundWithMatrices_doesNothingIfBitmapIsNull() {
        mCoordinator.setBackground(null, mBackgroundImageInfo, IMAGE_FROM_DISK);

        assertNull(mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_MATRIX));
    }

    @Test
    @Config(qualifiers = "port")
    public void testSetImageBackgroundWithMatrices_doesNothingIfInfoIsNull() {
        mCoordinator.setBackground(mBitmap, null, IMAGE_FROM_DISK);

        assertNull(mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_MATRIX));
    }
}
