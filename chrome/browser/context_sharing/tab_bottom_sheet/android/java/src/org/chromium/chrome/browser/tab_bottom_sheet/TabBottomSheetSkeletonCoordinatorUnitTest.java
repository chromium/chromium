// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.context_sharing.R;

/** Unit tests for {@link TabBottomSheetSkeletonCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabBottomSheetSkeletonCoordinatorUnitTest {
    private static final float EPSILON = 0.01f;

    private Context mContext;
    private TabBottomSheetSkeletonCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        org.chromium.chrome.R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new TabBottomSheetSkeletonCoordinator(
                        mContext, Color.WHITE, Color.LTGRAY, R.drawable.ic_spark_24dp);
        TabBottomSheetSkeletonView skeletonView = mCoordinator.getSkeletonViewForTesting();
        skeletonView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        skeletonView.measure(
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        skeletonView.layout(0, 0, 500, 1000);
    }

    @Test
    public void testInitialization() {
        View view = mCoordinator.getView();
        assertNotNull(view);
        assertSame(mCoordinator.getSkeletonViewForTesting(), view);
        assertEquals(View.INVISIBLE, view.getVisibility());
        ColorDrawable background = (ColorDrawable) view.getBackground();
        assertNotNull(background);
        assertEquals(Color.WHITE, background.getColor());
        ImageView headerIcon = view.findViewById(R.id.peek_icon);
        assertNotNull(headerIcon);
        assertNotNull(headerIcon.getDrawable());
        assertEquals(
                Color.LTGRAY,
                mCoordinator
                        .getSkeletonViewForTesting()
                        .getBottomGroupForTesting()
                        .getChildAt(0)
                        .getBackgroundTintList()
                        .getDefaultColor());
    }

    @Test
    public void testUpdateVisibleHeight_BelowOrAtPeekHeight() {
        TabBottomSheetSkeletonView skeletonView = mCoordinator.getSkeletonViewForTesting();
        ViewGroup bottomGroup = skeletonView.getBottomGroupForTesting();
        assertNotNull(bottomGroup);

        int peekHeight = mCoordinator.getDefaultPeekHeightForTesting();

        mCoordinator.updateVisibleHeight(peekHeight);
        assertEquals(peekHeight, skeletonView.getLayoutParams().height);
        assertEquals(0.0f, bottomGroup.getAlpha(), EPSILON);

        mCoordinator.updateVisibleHeight(peekHeight - 20);
        assertEquals(peekHeight - 20, skeletonView.getLayoutParams().height);
        assertEquals(0.0f, bottomGroup.getAlpha(), EPSILON);
    }

    @Test
    public void testUpdateVisibleHeight_AboveOrAtCollisionHeight() {
        TabBottomSheetSkeletonView skeletonView = mCoordinator.getSkeletonViewForTesting();
        ViewGroup bottomGroup = skeletonView.getBottomGroupForTesting();
        assertNotNull(bottomGroup);

        int collisionHeight = mCoordinator.getCollisionHeightForTesting();

        mCoordinator.updateVisibleHeight(collisionHeight);
        assertEquals(collisionHeight, skeletonView.getLayoutParams().height);
        assertEquals(1.0f, bottomGroup.getAlpha(), EPSILON);

        mCoordinator.updateVisibleHeight(collisionHeight + 100);
        assertEquals(collisionHeight + 100, skeletonView.getLayoutParams().height);
        assertEquals(1.0f, bottomGroup.getAlpha(), EPSILON);
    }

    @Test
    public void testUpdateVisibleHeight_MidpointInterpolation() {
        TabBottomSheetSkeletonView skeletonView = mCoordinator.getSkeletonViewForTesting();
        ViewGroup bottomGroup = skeletonView.getBottomGroupForTesting();
        assertNotNull(bottomGroup);

        int peekHeight = mCoordinator.getDefaultPeekHeightForTesting();
        int collisionHeight = mCoordinator.getCollisionHeightForTesting();
        int midHeight = (collisionHeight + peekHeight) / 2;

        mCoordinator.updateVisibleHeight(midHeight);
        assertEquals(midHeight, skeletonView.getLayoutParams().height);

        float expectedAlpha = (float) (midHeight - peekHeight) / (collisionHeight - peekHeight);
        assertEquals(expectedAlpha, bottomGroup.getAlpha(), EPSILON);
    }

    @Test
    public void testUpdateAlpha_OnLayoutChangeListener() {
        TabBottomSheetSkeletonCoordinator unlaidOutCoordinator =
                new TabBottomSheetSkeletonCoordinator(
                        mContext, Color.WHITE, Color.LTGRAY, R.drawable.ic_spark_24dp);
        TabBottomSheetSkeletonView skeletonView = unlaidOutCoordinator.getSkeletonViewForTesting();
        skeletonView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        int peekHeight = unlaidOutCoordinator.getDefaultPeekHeightForTesting();

        // Call updateVisibleHeight while unlaid out (heights == 0); alpha remains default (1.0f).
        unlaidOutCoordinator.updateVisibleHeight(peekHeight);
        ViewGroup bottomGroup = skeletonView.getBottomGroupForTesting();
        assertNotNull(bottomGroup);
        assertEquals(1.0f, bottomGroup.getAlpha(), EPSILON);

        // Measure and lay out skeletonView so children are laid out and size-changed listener
        // fires.
        skeletonView.measure(
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        skeletonView.layout(0, 0, 500, 1000);

        // OnLayoutChangeListener re-evaluates updateAlpha() using laid-out heights -> alpha 0.0f.
        assertEquals(0.0f, bottomGroup.getAlpha(), EPSILON);
    }

    @Test
    public void testDestroy_RemovesLayoutChangeListener() {
        TabBottomSheetSkeletonCoordinator coordinator =
                new TabBottomSheetSkeletonCoordinator(
                        mContext, Color.WHITE, Color.LTGRAY, R.drawable.ic_spark_24dp);
        TabBottomSheetSkeletonView skeletonView = coordinator.getSkeletonViewForTesting();
        skeletonView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        int peekHeight = coordinator.getDefaultPeekHeightForTesting();
        coordinator.updateVisibleHeight(peekHeight);

        // Destroy removes the layout change listener.
        coordinator.destroy();

        skeletonView.measure(
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        skeletonView.layout(0, 0, 500, 1000);

        // Because the listener was removed on destroy(), layout change does not update alpha.
        ViewGroup bottomGroup = skeletonView.getBottomGroupForTesting();
        assertNotNull(bottomGroup);
        assertEquals(1.0f, bottomGroup.getAlpha(), EPSILON);
    }
}
