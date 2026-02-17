// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.widget.ImageView.ScaleType;

import androidx.annotation.ColorInt;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NtpBackgroundImageLayoutViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundImageLayoutViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NtpBackgroundImageLayout mBackgroundImageLayout;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel(NtpBackgroundImageProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mBackgroundImageLayout, NtpBackgroundImageLayoutViewBinder::bind);
    }

    @Test
    public void testSetBackgroundImage() {
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        mModel.set(NtpBackgroundImageProperties.BACKGROUND_IMAGE, bitmap);

        verify(mBackgroundImageLayout).setBitmap(eq(bitmap));

        mModel.set(NtpBackgroundImageProperties.BACKGROUND_IMAGE, null);
        verify(mBackgroundImageLayout).setBitmap(eq(null));
    }

    @Test
    public void testSetImageMatrix() {
        Matrix matrix = new Matrix();
        matrix.setScale(2.0f, 2.0f);
        mModel.set(NtpBackgroundImageProperties.IMAGE_MATRIX, matrix);

        verify(mBackgroundImageLayout).setImageMatrix(eq(matrix));
    }

    @Test
    public void testSetScaleType() {
        ScaleType scaleType = ScaleType.MATRIX;
        mModel.set(NtpBackgroundImageProperties.IMAGE_SCALE_TYPE, scaleType);

        verify(mBackgroundImageLayout).setScaleType(eq(scaleType));
    }

    @Test
    public void testSetBackgroundColor() {
        @ColorInt int color = Color.BLACK;
        mModel.set(NtpBackgroundImageProperties.BACKGROUND_COLOR, color);

        verify(mBackgroundImageLayout).setBackgroundColor(eq(color));
    }

    @Test
    public void testSetDensity() {
        int density = 160;
        mModel.set(NtpBackgroundImageProperties.DENSITY, density);

        verify(mBackgroundImageLayout).setDensity(eq(density));
    }
}
