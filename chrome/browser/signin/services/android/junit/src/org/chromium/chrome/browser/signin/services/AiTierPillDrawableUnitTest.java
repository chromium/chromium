// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.RectF;
import android.graphics.Shader;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link AiTierPillDrawable}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AiTierPillDrawableUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SubscriptionTierBrandingDelegate mBrandingDelegate;
    @Mock private Canvas mCanvas;

    private AiTierPillDrawable mDrawable;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        RuntimeEnvironment.getApplication(), R.style.Theme_BrowserUI_DayNight);
        mDrawable = new AiTierPillDrawable(context, mBrandingDelegate);
    }

    @Test
    public void testDelegateGetsPillBounds() {
        Shader waveShader =
                new LinearGradient(0, 0, 1, 0, Color.RED, Color.BLUE, Shader.TileMode.CLAMP);
        when(mBrandingDelegate.getTierPillWaveShader(any())).thenReturn(waveShader);
        when(mBrandingDelegate.getTierPillWavePath(any())).thenReturn(new Path());
        when(mBrandingDelegate.getTierPillShadowColor()).thenReturn(0x80000000);

        mDrawable.setBounds(10, 20, 110, 42);

        RectF pillBounds = new RectF(0, 0, 100, 22);
        verify(mBrandingDelegate).getTierPillWaveShader(eq(pillBounds));
        verify(mBrandingDelegate).getTierPillWavePath(eq(pillBounds));
        verify(mBrandingDelegate).getTierPillShadowColor();
    }

    @Test
    public void testDrawsAtBoundsOrigin() {
        mDrawable.setBounds(10, 20, 110, 42);

        mDrawable.draw(mCanvas);

        verify(mCanvas).drawBitmap(any(Bitmap.class), eq(10f), eq(20f), any(Paint.class));
    }

    @Test
    public void testDrawsWithoutWaveOrShadow() {
        // The mock delegate returns no wave and a transparent shadow color.
        mDrawable.setBounds(0, 0, 100, 22);

        mDrawable.draw(mCanvas);

        verify(mCanvas).drawBitmap(any(Bitmap.class), eq(0f), eq(0f), any(Paint.class));
    }

    @Test
    public void testEmptyBoundsDrawNothing() {
        mDrawable.setBounds(0, 0, 100, 0);

        mDrawable.draw(mCanvas);

        verifyNoInteractions(mBrandingDelegate);
        verify(mCanvas, never()).drawBitmap(any(Bitmap.class), anyFloat(), anyFloat(), any());
    }

    @Test
    public void testEmptyBoundsClearTheBitmap() {
        mDrawable.setBounds(0, 0, 100, 22);
        mDrawable.draw(mCanvas);

        mDrawable.setBounds(0, 0, 100, 0);
        mDrawable.draw(mCanvas);

        // Only the first draw() drew the pill.
        verify(mCanvas, times(1)).drawBitmap(any(Bitmap.class), anyFloat(), anyFloat(), any());
    }

    @Test
    public void testAlphaAndColorFilter() {
        ColorFilter colorFilter = new PorterDuffColorFilter(Color.RED, PorterDuff.Mode.SRC_IN);
        mDrawable.setBounds(0, 0, 100, 22);

        mDrawable.setAlpha(128);
        mDrawable.setColorFilter(colorFilter);
        mDrawable.draw(mCanvas);

        ArgumentCaptor<Paint> paintCaptor = ArgumentCaptor.forClass(Paint.class);
        verify(mCanvas).drawBitmap(any(Bitmap.class), eq(0f), eq(0f), paintCaptor.capture());
        assertEquals(128, paintCaptor.getValue().getAlpha());
        assertEquals(colorFilter, paintCaptor.getValue().getColorFilter());
        assertEquals(128, mDrawable.getAlpha());
        assertEquals(colorFilter, mDrawable.getColorFilter());
        assertEquals(PixelFormat.TRANSLUCENT, mDrawable.getOpacity());
    }

    @Test
    public void testMovingKeepsTheBitmap() {
        mDrawable.setBounds(0, 0, 100, 22);
        Canvas canvas = mock(Canvas.class);
        mDrawable.draw(canvas);
        ArgumentCaptor<Bitmap> bitmapCaptor = ArgumentCaptor.forClass(Bitmap.class);
        verify(canvas).drawBitmap(bitmapCaptor.capture(), eq(0f), eq(0f), any(Paint.class));
        Bitmap bitmap = bitmapCaptor.getValue();

        mDrawable.setBounds(10, 20, 110, 42);
        mDrawable.draw(mCanvas);

        verify(mBrandingDelegate, times(1)).getTierPillShadowColor();
        verify(mCanvas).drawBitmap(same(bitmap), eq(10f), eq(20f), any(Paint.class));
        assertFalse(bitmap.isRecycled());
    }

    @Test
    public void testRedrawsWhenBoundsChange() {
        mDrawable.setBounds(0, 0, 100, 22);
        Canvas canvas = mock(Canvas.class);
        mDrawable.draw(canvas);
        ArgumentCaptor<Bitmap> bitmapCaptor = ArgumentCaptor.forClass(Bitmap.class);
        verify(canvas).drawBitmap(bitmapCaptor.capture(), eq(0f), eq(0f), any(Paint.class));
        assertEquals(100, bitmapCaptor.getValue().getWidth());

        mDrawable.setBounds(0, 0, 200, 44);
        mDrawable.draw(mCanvas);

        verify(mCanvas).drawBitmap(bitmapCaptor.capture(), eq(0f), eq(0f), any(Paint.class));
        assertEquals(200, bitmapCaptor.getValue().getWidth());
        assertEquals(44, bitmapCaptor.getValue().getHeight());
        // The old bitmap isn't recycled, since a frame being drawn may still use it.
        assertFalse(bitmapCaptor.getAllValues().get(0).isRecycled());
    }
}
