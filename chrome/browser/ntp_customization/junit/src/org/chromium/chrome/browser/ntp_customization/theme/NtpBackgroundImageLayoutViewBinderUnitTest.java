// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import androidx.annotation.ColorInt;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NtpBackgroundImageLayoutViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpBackgroundImageLayoutViewBinderUnitTest {
    private NtpBackgroundImageLayout mBackgroundImageLayout;
    private ImageView mImageView;
    private View mGradientView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mBackgroundImageLayout =
                (NtpBackgroundImageLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.ntp_customization_background_image_layout, null);
        mImageView = mBackgroundImageLayout.findViewById(R.id.image_view);
        mGradientView = mBackgroundImageLayout.findViewById(R.id.gradient_view);

        mModel = new PropertyModel(NtpBackgroundImageProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mBackgroundImageLayout, NtpBackgroundImageLayoutViewBinder::bind);
    }

    @Test
    public void testSetBackgroundImage() {
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        mModel.set(NtpBackgroundImageProperties.BACKGROUND_IMAGE, bitmap);

        assertEquals(bitmap, ((BitmapDrawable) mImageView.getDrawable()).getBitmap());
        assertEquals(View.VISIBLE, mGradientView.getVisibility());

        mModel.set(NtpBackgroundImageProperties.BACKGROUND_IMAGE, null);
        assertNull(((BitmapDrawable) mImageView.getDrawable()).getBitmap());
        assertEquals(View.GONE, mGradientView.getVisibility());
    }

    @Test
    public void testSetImageMatrix() {
        // The image matrix only takes effect on a laid out view with a drawable and the MATRIX
        // scale type.
        mBackgroundImageLayout.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        mBackgroundImageLayout.layout(0, 0, 100, 100);
        mModel.set(
                NtpBackgroundImageProperties.BACKGROUND_IMAGE,
                Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888));
        mModel.set(NtpBackgroundImageProperties.IMAGE_SCALE_TYPE, ScaleType.MATRIX);

        Matrix matrix = new Matrix();
        matrix.setScale(2.0f, 2.0f);
        mModel.set(NtpBackgroundImageProperties.IMAGE_MATRIX, matrix);

        float[] expectedValues = new float[9];
        matrix.getValues(expectedValues);
        float[] actualValues = new float[9];
        mImageView.getImageMatrix().getValues(actualValues);
        assertArrayEquals(expectedValues, actualValues, 0f);
    }

    @Test
    public void testSetScaleType() {
        ScaleType scaleType = ScaleType.MATRIX;
        mModel.set(NtpBackgroundImageProperties.IMAGE_SCALE_TYPE, scaleType);

        assertEquals(scaleType, mImageView.getScaleType());
    }

    @Test
    public void testSetBackgroundColor() {
        @ColorInt int color = Color.BLACK;
        mModel.set(NtpBackgroundImageProperties.BACKGROUND_COLOR, color);

        assertEquals(color, ((ColorDrawable) mBackgroundImageLayout.getBackground()).getColor());
    }

    @Test
    public void testSetDensity() {
        Bitmap bitmap = Bitmap.createBitmap(30, 30, Bitmap.Config.ARGB_8888);
        mModel.set(NtpBackgroundImageProperties.BACKGROUND_IMAGE, bitmap);
        int density = 480;
        assertNotEquals(density, bitmap.getDensity());
        assertEquals(30, mImageView.getDrawable().getIntrinsicWidth());

        mModel.set(NtpBackgroundImageProperties.DENSITY, density);

        assertEquals(density, bitmap.getDensity());
        // The bitmap is re-applied so the drawable's intrinsic size reflects the new density
        // (480dpi bitmap on a 160dpi display).
        assertEquals(bitmap, ((BitmapDrawable) mImageView.getDrawable()).getBitmap());
        assertEquals(10, mImageView.getDrawable().getIntrinsicWidth());
    }
}
