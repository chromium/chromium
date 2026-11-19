// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.animation.ValueAnimator;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link CriticalActionsExpansionAnimator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CriticalActionsExpansionAnimatorTest {
    private static final int PANEL_HEIGHT = 100;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private ImageView mExpandIcon;
    private FrameLayout mPanelContainer;
    private View mPanel;
    private CriticalActionsExpansionAnimator mAnimator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mExpandIcon = new ImageView(mActivity);
        mPanelContainer = new FrameLayout(mActivity);
        mPanelContainer.setVisibility(View.GONE);
        mPanel = new View(mActivity);
        mPanel.setMinimumHeight(PANEL_HEIGHT);
        mPanelContainer.addView(
                mPanel,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mAnimator = new CriticalActionsExpansionAnimator(mExpandIcon, mPanelContainer);
    }

    private ValueAnimator getRunningAnimator() {
        ValueAnimator animator = mAnimator.getAnimatorForTesting();
        assertNotNull(animator);
        return animator;
    }

    private int getContainerHeight() {
        return mPanelContainer.getLayoutParams().height;
    }

    private void assertWrapsContent() {
        assertEquals(LayoutParams.WRAP_CONTENT, getContainerHeight());
        assertEquals(LayoutParams.WRAP_CONTENT, mPanel.getLayoutParams().height);
    }

    @Test
    public void testNotAnimated() {
        mAnimator.setExpanded(/* expanded= */ true, /* animate= */ false);
        assertNull(mAnimator.getAnimatorForTesting());
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        assertEquals(180f, mExpandIcon.getRotation(), 0f);
        assertWrapsContent();

        mAnimator.setExpanded(/* expanded= */ false, /* animate= */ false);
        assertNull(mAnimator.getAnimatorForTesting());
        assertEquals(View.GONE, mPanelContainer.getVisibility());
        assertEquals(0f, mExpandIcon.getRotation(), 0f);
        assertWrapsContent();
    }

    @Test
    public void testAnimated() {
        // Expanding starts with the container shown, but with no height yet.
        mAnimator.setExpanded(/* expanded= */ true, /* animate= */ true);
        ValueAnimator animator = getRunningAnimator();
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        assertEquals(0, getContainerHeight());
        // The panel keeps its full height while the container grows, so it isn't squeezed.
        assertEquals(PANEL_HEIGHT, mPanel.getLayoutParams().height);

        // Then the container grows gradually, while the icon rotates.
        animator.setCurrentFraction(0.5f);
        int halfwayHeight = getContainerHeight();
        assertTrue(0 < halfwayHeight && halfwayHeight < PANEL_HEIGHT);
        float rotation = mExpandIcon.getRotation();
        assertTrue(0f < rotation && rotation < 180f);
        animator.end();
        assertNull(mAnimator.getAnimatorForTesting());
        assertEquals(180f, mExpandIcon.getRotation(), 0f);
        assertWrapsContent();

        // Collapsing keeps the container shown until it has no height left.
        mAnimator.setExpanded(/* expanded= */ false, /* animate= */ true);
        animator = getRunningAnimator();
        assertEquals(PANEL_HEIGHT, getContainerHeight());
        animator.setCurrentFraction(0.5f);
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        halfwayHeight = getContainerHeight();
        assertTrue(0 < halfwayHeight && halfwayHeight < PANEL_HEIGHT);
        animator.end();
        assertEquals(View.GONE, mPanelContainer.getVisibility());
        assertEquals(0f, mExpandIcon.getRotation(), 0f);
        assertWrapsContent();
    }

    @Test
    public void testReversedMidway() {
        mAnimator.setExpanded(/* expanded= */ true, /* animate= */ false);
        mAnimator.setExpanded(/* expanded= */ false, /* animate= */ true);
        getRunningAnimator().setCurrentFraction(0.5f);
        int halfwayHeight = getContainerHeight();

        // Expanding it again before it's collapsed continues from where it was.
        mAnimator.setExpanded(/* expanded= */ true, /* animate= */ true);
        ValueAnimator animator = getRunningAnimator();
        assertEquals(halfwayHeight, getContainerHeight());
        animator.end();
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        assertWrapsContent();
    }

    @Test
    public void testReversedBeforeFirstFrame_NotAnimated() {
        // Expanding starts at 0, so collapsing again before the first frame has nothing to animate.
        mAnimator.setExpanded(/* expanded= */ true, /* animate= */ true);
        mAnimator.setExpanded(/* expanded= */ false, /* animate= */ true);
        assertNull(mAnimator.getAnimatorForTesting());
        assertEquals(View.GONE, mPanelContainer.getVisibility());
        assertEquals(0f, mExpandIcon.getRotation(), 0f);
        assertWrapsContent();
    }

    @Test
    public void testDestroy() {
        mActivity.setContentView(mPanelContainer);
        mAnimator.setExpanded(/* expanded= */ true, /* animate= */ true);
        getRunningAnimator();

        // Destroying cancels the animation, and detaching afterwards changes nothing.
        mAnimator.destroy();
        assertNull(mAnimator.getAnimatorForTesting());
        int height = getContainerHeight();
        ((ViewGroup) mPanelContainer.getParent()).removeView(mPanelContainer);
        assertEquals(height, getContainerHeight());
    }

    @Test
    public void testDetachEndsAnimation() {
        mActivity.setContentView(mPanelContainer);
        mAnimator.setExpanded(/* expanded= */ true, /* animate= */ true);
        getRunningAnimator();

        ((ViewGroup) mPanelContainer.getParent()).removeView(mPanelContainer);
        assertNull(mAnimator.getAnimatorForTesting());
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        assertWrapsContent();
    }
}
