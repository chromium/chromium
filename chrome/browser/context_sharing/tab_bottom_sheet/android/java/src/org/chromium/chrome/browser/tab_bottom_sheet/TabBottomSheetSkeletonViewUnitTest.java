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
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Color;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
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
    public void testSetBottomGroupAlpha() {
        ViewGroup bottomGroup = mSkeletonView.getBottomGroupForTesting();
        assertNotNull(bottomGroup);
        assertEquals(1.0f, bottomGroup.getAlpha(), EPSILON);

        mSkeletonView.setBottomGroupAlpha(0.5f);
        assertEquals(0.5f, bottomGroup.getAlpha(), EPSILON);

        mSkeletonView.setBottomGroupAlpha(0.0f);
        assertEquals(0.0f, bottomGroup.getAlpha(), EPSILON);
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
            assertTrue(animator instanceof ObjectAnimator);
            ObjectAnimator pulse = (ObjectAnimator) animator;
            assertEquals("alpha", pulse.getPropertyName());
            assertEquals(
                    (long) i * TabBottomSheetSkeletonView.FADE_STAGGER_MS, pulse.getStartDelay());
            assertEquals(TabBottomSheetSkeletonView.FADE_DURATION_MS, pulse.getDuration());
            assertEquals(ValueAnimator.INFINITE, shadowOf(pulse).getActualRepeatCount());
            assertEquals(ValueAnimator.REVERSE, pulse.getRepeatMode());
        }
    }

    @Test
    public void testWaveAnimation_startsAndStopsWithIsResizingAndAlpha() {
        AnimationHandler animationHandler = mSkeletonView.getAnimationHandlerForTesting();
        ViewGroup bottomGroup = mSkeletonView.getBottomGroupForTesting();
        assertNotNull(animationHandler);
        assertNotNull(bottomGroup);

        // Initially not resizing: animation is not present.
        assertFalse(animationHandler.isAnimationPresent());

        // Set alpha to 0.0f before entering resizing mode.
        mSkeletonView.setBottomGroupAlpha(0.0f);
        assertFalse(animationHandler.isAnimationPresent());

        // Enter resizing mode while alpha is 0.0f: still not started.
        mSkeletonView.setIsResizing(true);
        assertFalse(animationHandler.isAnimationPresent());

        // Increase alpha > 0f while resizing: animation starts.
        mSkeletonView.setBottomGroupAlpha(0.5f);
        assertTrue(animationHandler.isAnimationPresent());

        // Simulate mid-animation child alphas and verify setBottomGroupAlpha(0.0f) finishes &
        // resets.
        for (int i = 0; i < bottomGroup.getChildCount(); i++) {
            bottomGroup.getChildAt(i).setAlpha(TabBottomSheetSkeletonView.LOW_OPACITY);
        }
        mSkeletonView.setBottomGroupAlpha(0.0f);
        assertFalse(animationHandler.isAnimationPresent());
        for (int i = 0; i < bottomGroup.getChildCount(); i++) {
            assertEquals(
                    TabBottomSheetSkeletonView.HIGH_OPACITY,
                    bottomGroup.getChildAt(i).getAlpha(),
                    EPSILON);
        }

        // Restart animation and verify setIsResizing(false) finishes & resets child alphas.
        mSkeletonView.setBottomGroupAlpha(0.8f);
        assertTrue(animationHandler.isAnimationPresent());
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
