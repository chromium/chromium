// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.content;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Paint.FontMetrics;
import android.text.TextPaint;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link TitleBitmapFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TitleBitmapFactoryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final float MAX_TEXT_HEIGHT = 100.f;
    private static final float VALID_TEXT_HEIGHT = 10.f;
    private static final float INVALID_TEXT_HEIGHT = 150.f;

    @Mock private TextPaint mTextPaint;
    private FontMetrics mFontMetrics;

    @Before
    public void setup() {
        mFontMetrics = new FontMetrics();
    }

    @Test
    public void testEnforceMaxTextHeight_AlreadyValid() {
        // Fake a valid text height.
        when(mTextPaint.getFontMetrics()).thenReturn(mFontMetrics);
        when(mTextPaint.getTextSize()).thenReturn(VALID_TEXT_HEIGHT);
        mFontMetrics.bottom = VALID_TEXT_HEIGHT;
        mFontMetrics.top = 0.f;

        // Verify we don't adjust the text height if it's already valid.
        TitleBitmapFactory.enforceMaxTextHeight(mTextPaint, MAX_TEXT_HEIGHT);
        verify(mTextPaint, never()).setTextSize(anyFloat());
    }

    @Test
    public void testEnforceMaxTextHeight_Clamped() {
        // Fake an invalid text height.
        when(mTextPaint.getFontMetrics()).thenReturn(mFontMetrics);
        when(mTextPaint.getTextSize()).thenReturn(INVALID_TEXT_HEIGHT);
        mFontMetrics.bottom = INVALID_TEXT_HEIGHT;
        mFontMetrics.top = 0.f;

        // Verify we adjust the text height as it's invalid.
        TitleBitmapFactory.enforceMaxTextHeight(mTextPaint, MAX_TEXT_HEIGHT);
        verify(mTextPaint).setTextSize(eq(MAX_TEXT_HEIGHT));
    }

    @Test
    public void testGetMaxHeightOfFont() {
        mFontMetrics.bottom = 200.f;
        mFontMetrics.top = 150.f;

        // bottom - top = 200.f - 150.f = 50.f
        float expectedMaxHeight = 50.f;
        assertEquals(
                "Unexpected calculated max height.",
                expectedMaxHeight,
                TitleBitmapFactory.getMaxHeightOfFont(mFontMetrics),
                /* delta= */ 0.f);
    }
}
