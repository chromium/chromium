// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Color;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.ui.animation.AnimationHandler;

import java.util.List;

/** Unit tests for {@link TabBottomSheetSkeletonView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabBottomSheetSkeletonViewUnitTest {
    private static final float EPSILON = 0.001f;

    private Context mContext;
    private TabBottomSheetSkeletonView mSkeletonView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        org.chromium.chrome.R.style.Theme_BrowserUI_DayNight);
        mSkeletonView =
                (TabBottomSheetSkeletonView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.tab_bottom_sheet_skeleton_view, null);
    }

    @Test
    public void testInflationHeaderIconAndPlaceholderElemColor() {
        FrameLayout headerContainer = mSkeletonView.getHeaderContainerForTesting();
        assertNotNull(headerContainer);
        assertNotNull(mSkeletonView.getBottomGroupForTesting());

        TextView titleView = headerContainer.findViewById(R.id.peek_title);
        assertNotNull(titleView);
        assertTrue(TextUtils.isEmpty(titleView.getText()));

        TabBottomSheetPeekView peekView = mSkeletonView.getPeekViewForTesting();
        assertNotNull(peekView);
        mSkeletonView.setHeaderIcon(R.drawable.ic_spark_24dp);
        ImageView headerIcon = peekView.findViewById(R.id.peek_icon);
        assertNotNull(headerIcon);
        assertNotNull(headerIcon.getDrawable());

        mSkeletonView.setPlaceholderElemColor(Color.CYAN);
        assertEquals(
                Color.CYAN,
                mSkeletonView
                        .findViewById(R.id.skeleton_bar_1)
                        .getBackgroundTintList()
                        .getDefaultColor());
        assertEquals(
                Color.CYAN,
                mSkeletonView
                        .findViewById(R.id.skeleton_bar_2)
                        .getBackgroundTintList()
                        .getDefaultColor());
        assertEquals(
                Color.CYAN,
                mSkeletonView
                        .findViewById(R.id.skeleton_bar_3)
                        .getBackgroundTintList()
                        .getDefaultColor());
        assertEquals(
                Color.CYAN,
                mSkeletonView
                        .findViewById(R.id.skeleton_pill)
                        .getBackgroundTintList()
                        .getDefaultColor());
    }

    @Test
    public void testParentClippingDisabledAndRestored() {
        FrameLayout parent = new FrameLayout(mContext);
        parent.setClipChildren(true);
        parent.addView(mSkeletonView);

        assertTrue(parent.getClipChildren());

        mSkeletonView.onAttachedToWindow();
        assertFalse(parent.getClipChildren());

        mSkeletonView.onDetachedFromWindow();
        assertTrue(parent.getClipChildren());
    }

    @Test
    public void testElementAlphaArbitrator() {
        View view = new View(mContext);
        TabBottomSheetSkeletonView.ElementAlphaArbitrator arbitrator =
                new TabBottomSheetSkeletonView.ElementAlphaArbitrator(view);

        arbitrator.setResizeAlpha(0.5f);
        assertEquals(0.5f, view.getAlpha(), EPSILON);

        arbitrator.setWaveAlpha(0.6f);
        assertEquals(0.3f, view.getAlpha(), EPSILON);

        arbitrator.setResizeAlpha(0.0f);
        arbitrator.setWaveAlpha(0.8f);
        assertEquals(0.0f, view.getAlpha(), EPSILON);
    }

    @Test
    public void testUpdateChildAlphas() {
        mSkeletonView.measure(
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        mSkeletonView.layout(0, 0, 500, 1000);

        ViewGroup bottomGroup = mSkeletonView.getBottomGroupForTesting();
        assertNotNull(bottomGroup);
        int baseHeight = mSkeletonView.getHeaderHeight() + mSkeletonView.getBottomGroupHeight();
        int bar2Min = baseHeight - bottomGroup.getChildAt(1).getTop();
        int pillMin = baseHeight - bottomGroup.getChildAt(3).getTop();

        mSkeletonView.updateChildAlphas(baseHeight + 24, 24);
        assertEquals(1.0f, bottomGroup.getChildAt(0).getAlpha(), EPSILON);
        assertEquals(1.0f, bottomGroup.getChildAt(3).getAlpha(), EPSILON);

        mSkeletonView.updateChildAlphas(baseHeight, 24);
        assertEquals(0.0f, bottomGroup.getChildAt(0).getAlpha(), EPSILON);
        assertEquals(1.0f, bottomGroup.getChildAt(1).getAlpha(), EPSILON);

        mSkeletonView.updateChildAlphas(bar2Min, 24);
        assertEquals(0.0f, bottomGroup.getChildAt(1).getAlpha(), EPSILON);
        assertEquals(1.0f, bottomGroup.getChildAt(2).getAlpha(), EPSILON);

        mSkeletonView.updateChildAlphas(pillMin, 24);
        assertEquals(0.0f, bottomGroup.getChildAt(3).getAlpha(), EPSILON);
    }

    @Test
    public void testGetHeaderAndBottomGroupHeights() {
        assertEquals(0, mSkeletonView.getHeaderHeight());
        assertEquals(0, mSkeletonView.getBottomGroupHeight());

        mSkeletonView.getHeaderContainerForTesting().layout(0, 0, 400, 120);
        mSkeletonView.getBottomGroupForTesting().layout(0, 500, 400, 800);

        assertEquals(120, mSkeletonView.getHeaderHeight());
        assertEquals(300, mSkeletonView.getBottomGroupHeight());
    }

    @Test
    public void testWaveAnimation_setupAndStaggeredDelays() {
        AnimatorSet animatorSet = mSkeletonView.createWaveAnimatorSet();
        assertNotNull(animatorSet);

        List<Animator> childAnimations = animatorSet.getChildAnimations();
        assertEquals(4, childAnimations.size());

        for (int i = 0; i < childAnimations.size(); i++) {
            Animator animator = childAnimations.get(i);
            assertTrue(animator instanceof ValueAnimator);
            ValueAnimator pulse = (ValueAnimator) animator;
            assertEquals(
                    (long) i * TabBottomSheetSkeletonView.FADE_STAGGER_MS, pulse.getStartDelay());
            assertEquals(TabBottomSheetSkeletonView.FADE_DURATION_MS, pulse.getDuration());
            assertEquals(ValueAnimator.INFINITE, shadowOf(pulse).getActualRepeatCount());
            assertEquals(ValueAnimator.REVERSE, pulse.getRepeatMode());
        }
    }

    @Test
    public void testWaveAnimation_startsAndStopsWithIsResizingAndAlpha() {
        mSkeletonView.measure(
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        mSkeletonView.layout(0, 0, 500, 1000);

        AnimationHandler animationHandler = mSkeletonView.getAnimationHandlerForTesting();
        ViewGroup bottomGroup = mSkeletonView.getBottomGroupForTesting();
        assertNotNull(animationHandler);
        assertNotNull(bottomGroup);

        // Initially not resizing: animation is not present.
        assertFalse(animationHandler.isAnimationPresent());

        // Collapse height so all children have resizeAlpha == 0.0f before entering resizing mode.
        mSkeletonView.updateChildAlphas(50, 24);
        assertFalse(animationHandler.isAnimationPresent());

        // Enter resizing mode while all children are faded out: still not started.
        mSkeletonView.setIsResizing(true);
        assertFalse(animationHandler.isAnimationPresent());

        // Increase height so children are visible while resizing: animation starts.
        mSkeletonView.updateChildAlphas(1000, 24);
        assertTrue(animationHandler.isAnimationPresent());

        // Simulate mid-animation child alphas and verify collapsing height finishes & resets wave
        // alpha.
        for (int i = 0; i < bottomGroup.getChildCount(); i++) {
            bottomGroup.getChildAt(i).setAlpha(TabBottomSheetSkeletonView.LOW_OPACITY);
        }
        mSkeletonView.updateChildAlphas(50, 24);
        assertFalse(animationHandler.isAnimationPresent());
        for (int i = 0; i < bottomGroup.getChildCount(); i++) {
            assertEquals(0.0f, bottomGroup.getChildAt(i).getAlpha(), EPSILON);
        }

        // Restart animation and verify wave alpha was reset to HIGH_OPACITY and
        // setIsResizing(false)
        // finishes & resets child alphas.
        mSkeletonView.updateChildAlphas(1000, 24);
        assertTrue(animationHandler.isAnimationPresent());
        for (int i = 0; i < bottomGroup.getChildCount(); i++) {
            assertEquals(
                    TabBottomSheetSkeletonView.HIGH_OPACITY,
                    bottomGroup.getChildAt(i).getAlpha(),
                    EPSILON);
        }
        for (int i = 0; i < bottomGroup.getChildCount(); i++) {
            bottomGroup.getChildAt(i).setAlpha(TabBottomSheetSkeletonView.LOW_OPACITY);
        }
        mSkeletonView.setIsResizing(false);
        assertFalse(animationHandler.isAnimationPresent());
        for (int i = 0; i < bottomGroup.getChildCount(); i++) {
            assertEquals(
                    TabBottomSheetSkeletonView.HIGH_OPACITY,
                    bottomGroup.getChildAt(i).getAlpha(),
                    EPSILON);
        }
    }
}
