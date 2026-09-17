// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.R.style.Theme_BrowserUI_DayNight;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.context_sharing.R;

/** Unit tests for {@link LegacyResizingPlaceholderCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LegacyResizingPlaceholderCoordinatorUnitTest {
    private static final float EPSILON = 0.01f;

    private Context mContext;
    private LegacyResizingPlaceholderCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(), Theme_BrowserUI_DayNight);
        mCoordinator = new LegacyResizingPlaceholderCoordinator(mContext, Color.WHITE);
        mCoordinator
                .getView()
                .setLayoutParams(
                        new ViewGroup.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT));
    }

    @Test
    public void testInitialization() {
        View view = mCoordinator.getView();
        assertNotNull(view);
        assertEquals(View.INVISIBLE, view.getVisibility());
    }

    @Test
    public void testUpdateVisibleHeight_AlphaRamp() {
        View placeholder = mCoordinator.getView();
        View content = mCoordinator.getResizingContentForTesting();

        content.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(60, View.MeasureSpec.EXACTLY));
        content.layout(0, 0, 100, 60);

        int minHeight =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_peek_height_total);
        int fadeOffset =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_resizing_fade_offset);
        int maxHeight = 60 + fadeOffset;

        // 1. visibleHeight <= minHeight -> alpha = 0.0f
        mCoordinator.updateVisibleHeight(minHeight - 10);
        assertEquals(0.0f, content.getAlpha(), EPSILON);
        assertEquals(minHeight - 10, placeholder.getLayoutParams().height);

        mCoordinator.updateVisibleHeight(minHeight);
        assertEquals(0.0f, content.getAlpha(), EPSILON);

        // 2. visibleHeight between minHeight and maxHeight -> interpolated alpha
        int midHeight = (minHeight + maxHeight) / 2;
        mCoordinator.updateVisibleHeight(midHeight);
        float expectedMidAlpha = (float) (midHeight - minHeight) / (maxHeight - minHeight);
        assertEquals(expectedMidAlpha, content.getAlpha(), EPSILON);

        // 3. visibleHeight >= maxHeight -> alpha = 1.0f
        mCoordinator.updateVisibleHeight(maxHeight);
        assertEquals(1.0f, content.getAlpha(), EPSILON);

        mCoordinator.updateVisibleHeight(maxHeight + 50);
        assertEquals(1.0f, content.getAlpha(), EPSILON);
        assertEquals(maxHeight + 50, placeholder.getLayoutParams().height);

        // 4. Edge case: maxHeight <= minHeight (e.g. content height is 0)
        content.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY));
        content.layout(0, 0, 100, 0);
        // fadeOffset (40dp) <= minHeight (72dp), so maxHeight (40dp) <= minHeight (72dp)
        mCoordinator.updateVisibleHeight(minHeight + 20);
        assertEquals(1.0f, content.getAlpha(), EPSILON);
    }

    @Test
    public void testUpdateVisibleHeight_NullLayoutParams_DoesNotCrash() {
        LegacyResizingPlaceholderCoordinator coordinator =
                new LegacyResizingPlaceholderCoordinator(mContext, Color.WHITE);
        // When view is not attached to container, getLayoutParams() is null.
        // updateVisibleHeight should gracefully update alpha without crashing on null LayoutParams.
        coordinator.updateVisibleHeight(100);
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();
        assertNotNull(mCoordinator.getView());
    }
}
