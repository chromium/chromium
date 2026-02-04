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

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo.matrixToString;

import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.CropImageUtils;
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
    @Captor ArgumentCaptor<FrameLayout> mBackgroundImageLayoutCaptor;

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

        int orientation = mActivity.getResources().getConfiguration().orientation;
        Point portraitWindowSize;
        Point landscapeWindowSize;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            portraitWindowSize = CropImageUtils.getCurrentWindowDimensions(mActivity);
            landscapeWindowSize = new Point(portraitWindowSize.y, portraitWindowSize.x);
        } else {
            landscapeWindowSize = CropImageUtils.getCurrentWindowDimensions(mActivity);
            portraitWindowSize = new Point(landscapeWindowSize.y, landscapeWindowSize.x);
        }

        mBackgroundImageInfo =
                new BackgroundImageInfo(
                        mPortraitMatrix, mLandscapeMatrix, portraitWindowSize, landscapeWindowSize);

        mCoordinator = new NtpBackgroundImageCoordinator(mActivity, mRootView, mUiConfig, COLOR);
        mPropertyModel = mCoordinator.getPropertyModelForTesting();
    }

    @Test
    public void testConstructor() {
        verify(mUiConfig, never()).addObserver(any(DisplayStyleObserver.class));
        assertEquals(COLOR, mPropertyModel.get(NtpBackgroundImageProperties.BACKGROUND_COLOR));
        verify(mRootView).addView(mBackgroundImageLayoutCaptor.capture());

        View backgroundImageLayout = mBackgroundImageLayoutCaptor.getValue();
        View gradientView = backgroundImageLayout.findViewById(R.id.gradient_view);
        assertEquals(View.GONE, gradientView.getVisibility());
    }

    @Test
    public void testMaybeAddDisplayStyleObserver() {
        verify(mUiConfig, never()).addObserver(any(DisplayStyleObserver.class));

        mCoordinator.setBackground(
                mBitmap, mBackgroundImageInfo, NtpBackgroundType.THEME_COLLECTION);
        verify(mUiConfig).addObserver(any(DisplayStyleObserver.class));

        clearInvocations(mUiConfig);
        // Verifies observer won't be added again.
        mCoordinator.setBackground(
                mBitmap, mBackgroundImageInfo, NtpBackgroundType.IMAGE_FROM_DISK);
        verify(mUiConfig, never()).addObserver(any(DisplayStyleObserver.class));
    }

    @Test
    public void testSetBackground() {
        // Background type is IMAGE_FROM_DISK:
        mCoordinator.setBackground(
                mBitmap, mBackgroundImageInfo, NtpBackgroundType.IMAGE_FROM_DISK);
        assertEquals(mBitmap, mPropertyModel.get(NtpBackgroundImageProperties.BACKGROUND_IMAGE));
        assertEquals(
                ImageView.ScaleType.MATRIX,
                mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_SCALE_TYPE));

        // Background type is THEME_COLLECTION:
        mCoordinator.setBackground(
                mBitmap, mBackgroundImageInfo, NtpBackgroundType.THEME_COLLECTION);
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
                mBitmap, mBackgroundImageInfo, NtpBackgroundType.THEME_COLLECTION);
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
    public void testSetImageBackgroundWithMatrices_cacheHit_portrait() {
        mCoordinator.setBackground(mBitmap, mBackgroundImageInfo, IMAGE_FROM_DISK);

        // Manually triggers the observer callback
        verify(mUiConfig).addObserver(mDisplayStyleObserverArgumentCaptor.capture());
        mDisplayStyleObserverArgumentCaptor.getValue().onDisplayStyleChanged(null);

        assertEquals(
                mPortraitMatrix, mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_MATRIX));
    }

    @Test
    @Config(qualifiers = "land")
    public void testSetImageBackgroundWithMatrices_cacheHit_landscape() {
        mCoordinator.setBackground(mBitmap, mBackgroundImageInfo, IMAGE_FROM_DISK);

        // Manually triggers the observer callback
        verify(mUiConfig).addObserver(mDisplayStyleObserverArgumentCaptor.capture());
        mDisplayStyleObserverArgumentCaptor.getValue().onDisplayStyleChanged(null);

        assertEquals(
                mLandscapeMatrix, mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_MATRIX));
    }

    @Test
    @Config(qualifiers = "port")
    public void testSetImageBackgroundWithMatrices_doesNothingIfBitmapIsNull() {
        mCoordinator.setBackground(null, mBackgroundImageInfo, IMAGE_FROM_DISK);

        verify(mUiConfig).addObserver(mDisplayStyleObserverArgumentCaptor.capture());
        mDisplayStyleObserverArgumentCaptor.getValue().onDisplayStyleChanged(null);

        assertNull(mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_MATRIX));
    }

    @Test
    @Config(qualifiers = "port")
    public void testSetImageBackgroundWithMatrices_doesNothingIfInfoIsNull() {
        mCoordinator.setBackground(mBitmap, null, IMAGE_FROM_DISK);

        verify(mUiConfig).addObserver(mDisplayStyleObserverArgumentCaptor.capture());
        mDisplayStyleObserverArgumentCaptor.getValue().onDisplayStyleChanged(null);

        assertNull(mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_MATRIX));
    }

    @Test
    @Config(qualifiers = "port")
    public void testSetImageBackground_cacheMiss_calculatesAndUpdatesCache() {
        Point currentScreenSize = CropImageUtils.getCurrentWindowDimensions(mActivity);

        Matrix invalidMatrix = new Matrix();
        invalidMatrix.setTranslate(currentScreenSize.x + 100f, 0);

        // Creates BackgroundImageInfo with a wrong window size to force a cache miss.
        // We ensure the "wrong" size is numerically different from the currentScreenSize.
        Point wrongEstimatedSize = new Point(currentScreenSize.x + 1, currentScreenSize.y + 1);
        Point landscapeSize = new Point(currentScreenSize.y + 1, currentScreenSize.x + 1);

        mBackgroundImageInfo =
                new BackgroundImageInfo(
                        invalidMatrix, // Passing the invalid matrix
                        mLandscapeMatrix,
                        wrongEstimatedSize, // Passing wrong size to force recalculation
                        landscapeSize);

        Matrix expectedMatrix = new Matrix(invalidMatrix);
        float[] matrixValues = new float[9];
        expectedMatrix.getValues(matrixValues);
        CropImageUtils.validateMatrix(
                expectedMatrix, currentScreenSize.x, currentScreenSize.y, mBitmap, matrixValues);

        mCoordinator.setBackground(mBitmap, mBackgroundImageInfo, IMAGE_FROM_DISK);

        verify(mUiConfig).addObserver(mDisplayStyleObserverArgumentCaptor.capture());
        mDisplayStyleObserverArgumentCaptor.getValue().onDisplayStyleChanged(null);

        // Verifies values changed
        Matrix resultMatrix = mPropertyModel.get(NtpBackgroundImageProperties.IMAGE_MATRIX);
        assertEquals(
                "Coordinator should have applied validateMatrix correctly",
                matrixToString(expectedMatrix),
                matrixToString(resultMatrix));

        // Verifies cache updated
        BackgroundImageInfo cachedInfo = mCoordinator.getCachedBackgroundImageInfoForTesting();
        assertEquals(resultMatrix, cachedInfo.getPortraitMatrix());
    }
}
