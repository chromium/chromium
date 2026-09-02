// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Point;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

/** Unit tests for {@link TabDragShadowBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabDragShadowBuilderUnitTest {
    private static final int SHADOW_WIDTH = 300;
    private static final int SHADOW_HEIGHT = 200;
    private static final int OFFSET_X = 150;
    private static final int OFFSET_Y = 40;
    private static final int CARD_LEFT = 10;
    private static final int CARD_TOP = 10;
    private static final int CARD_RIGHT = 290;
    private static final int CARD_BOTTOM = 190;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Canvas mCanvas;

    private View mDragSourceView;
    private FrameLayout mShadowView;
    private View mCardView;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mDragSourceView = spy(new View(activity));
        mShadowView = spy(new FrameLayout(activity));
        mCardView = new View(activity);
        mCardView.setId(R.id.card_view);
        mShadowView.addView(mCardView);
        when(mShadowView.findViewById(R.id.card_view)).thenReturn(mCardView);
    }

    @Test
    public void testProvideShadowMetrics() {
        mShadowView.layout(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        Point offset = new Point(OFFSET_X, OFFSET_Y);
        TabDragShadowBuilder builder =
                new TabDragShadowBuilder(mDragSourceView, mShadowView, offset);

        Point sizeOut = new Point();
        Point touchOut = new Point();
        builder.onProvideShadowMetrics(sizeOut, touchOut);

        assertEquals("Shadow width should match shadow view width.", SHADOW_WIDTH, sizeOut.x);
        assertEquals("Shadow height should match shadow view height.", SHADOW_HEIGHT, sizeOut.y);
        assertEquals("Touch point X should match offset.", OFFSET_X, touchOut.x);
        assertEquals("Touch point Y should match offset.", OFFSET_Y, touchOut.y);
    }

    @Test
    public void testOnDrawShadow_WhenShown() {
        mShadowView.layout(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        mCardView.layout(CARD_LEFT, CARD_TOP, CARD_RIGHT, CARD_BOTTOM);
        Point offset = new Point(OFFSET_X, OFFSET_Y);
        TabDragShadowBuilder builder =
                new TabDragShadowBuilder(mDragSourceView, mShadowView, offset);

        builder.update(/* show= */ true);
        builder.onDrawShadow(mCanvas);

        verify(mCanvas)
                .drawRoundRect(
                        eq((float) CARD_LEFT),
                        eq((float) CARD_TOP),
                        eq((float) CARD_RIGHT),
                        eq((float) CARD_BOTTOM),
                        anyFloat(),
                        anyFloat(),
                        any(Paint.class));
        verify(mShadowView).draw(mCanvas);
    }

    @Test
    public void testOnDrawShadow_WhenHidden() {
        mShadowView.layout(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        mCardView.layout(CARD_LEFT, CARD_TOP, CARD_RIGHT, CARD_BOTTOM);
        Point offset = new Point(OFFSET_X, OFFSET_Y);
        TabDragShadowBuilder builder =
                new TabDragShadowBuilder(mDragSourceView, mShadowView, offset);

        builder.update(/* show= */ false);
        builder.onDrawShadow(mCanvas);

        verify(mCanvas, never())
                .drawRoundRect(
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        any(Paint.class));
        verify(mShadowView, never()).draw(mCanvas);
    }

    @Test
    public void testOnDrawShadow_WhenCardViewNull_UsesFallbackDraw() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout emptyShadowView = spy(new FrameLayout(activity));
        emptyShadowView.layout(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        Point offset = new Point(OFFSET_X, OFFSET_Y);
        TabDragShadowBuilder builder =
                new TabDragShadowBuilder(mDragSourceView, emptyShadowView, offset);

        builder.update(/* show= */ true);
        builder.onDrawShadow(mCanvas);

        verify(mCanvas, never())
                .drawRoundRect(
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        any(Paint.class));
        verify(emptyShadowView).draw(mCanvas);
    }

    @Test
    public void testUpdate_TogglesVisibilityAndDispatchesUpdate() {
        when(mDragSourceView.isAttachedToWindow()).thenReturn(true);
        Point offset = new Point(OFFSET_X, OFFSET_Y);
        TabDragShadowBuilder builder =
                new TabDragShadowBuilder(mDragSourceView, mShadowView, offset);

        assertFalse("Shadow should not be shown initially.", builder.getShowDragShadow());

        builder.update(/* show= */ true);
        assertTrue("Shadow should be shown after update(true).", builder.getShowDragShadow());
        verify(mDragSourceView).updateDragShadow(builder);

        builder.update(/* show= */ false);
        assertFalse("Shadow should be hidden after update(false).", builder.getShowDragShadow());
        verify(mDragSourceView, times(2)).updateDragShadow(builder);
    }
}
