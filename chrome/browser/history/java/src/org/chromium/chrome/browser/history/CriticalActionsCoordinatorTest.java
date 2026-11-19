// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.animation.ValueAnimator;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;

import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link CriticalActionsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.CRITICAL_ACTION_HISTORY)
public class CriticalActionsCoordinatorTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private ImageView mExpandIcon;
    private FrameLayout mPanelContainer;
    private CriticalActionsCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mExpandIcon = new ImageView(activity);
                            mExpandIcon.setVisibility(View.GONE);
                            mPanelContainer = new FrameLayout(activity);
                            mPanelContainer.setVisibility(View.GONE);
                            mCoordinator =
                                    new CriticalActionsCoordinator(
                                            activity, mExpandIcon, mPanelContainer);
                        });
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    public void testAttachAndToggle() {
        AtomicReference<Boolean> recordedExpanded = new AtomicReference<>();
        mCoordinator.attachToHistoryItem(
                List.of("Filled a form", "Used a password"),
                /* expanded= */ false,
                /* selectionActive= */ false,
                recordedExpanded::set);

        assertEquals(View.VISIBLE, mExpandIcon.getVisibility());
        assertEquals("2 key browsing actions", mExpandIcon.getContentDescription());
        assertEquals(0f, mExpandIcon.getRotation(), 0f);
        assertEquals(View.GONE, mPanelContainer.getVisibility());
        assertEquals("Expand section", getClickActionLabel(mExpandIcon));
        assertTrue(mExpandIcon.hasOnClickListeners());

        // Expand by clicking the chevron.
        mExpandIcon.performClick();
        endExpansionAnimation();
        assertEquals(Boolean.TRUE, recordedExpanded.get());
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        assertEquals(180f, mExpandIcon.getRotation(), 0f);
        assertEquals("Collapse section", getClickActionLabel(mExpandIcon));

        ViewGroup list = mPanelContainer.findViewById(R.id.critical_actions_list);
        assertNotNull(list);
        assertEquals(2, list.getChildCount());
        assertEquals(
                "Filled a form",
                ((TextView) list.getChildAt(0).findViewById(R.id.critical_action_label)).getText());
        assertEquals(
                "Used a password",
                ((TextView) list.getChildAt(1).findViewById(R.id.critical_action_label)).getText());

        // Collapse by clicking the chevron again.
        mExpandIcon.performClick();
        endExpansionAnimation();
        assertEquals(Boolean.FALSE, recordedExpanded.get());
        assertEquals(View.GONE, mPanelContainer.getVisibility());
        assertEquals(0f, mExpandIcon.getRotation(), 0f);
        assertEquals("Expand section", getClickActionLabel(mExpandIcon));
    }

    @Test
    public void testDestroy_ClearsExpandIconClickHandler() {
        AtomicReference<Boolean> recordedExpanded = new AtomicReference<>();
        mCoordinator.attachToHistoryItem(
                List.of("Filled a form"),
                /* expanded= */ false,
                /* selectionActive= */ false,
                recordedExpanded::set);
        assertTrue(mExpandIcon.hasOnClickListeners());
        assertEquals("Expand section", getClickActionLabel(mExpandIcon));

        // The chevron no longer toggles the critical actions.
        mCoordinator.destroy();
        assertFalse(mExpandIcon.hasOnClickListeners());
        assertNull(getClickActionLabel(mExpandIcon));
        mExpandIcon.performClick();
        assertNull(recordedExpanded.get());
        assertEquals(View.GONE, mPanelContainer.getVisibility());
    }

    @Test
    public void testReattachEndsAnimation() {
        List<String> labels = List.of("Filled a form");
        mCoordinator.attachToHistoryItem(
                labels,
                /* expanded= */ false,
                /* selectionActive= */ false,
                /* onExpansionChanged= */ null);
        assertTrue(mExpandIcon.hasOnClickListeners());
        mExpandIcon.performClick();
        assertNotNull(mCoordinator.getExpansionAnimatorForTesting());

        // Reattaching the row to an item without actions shows it collapsed right away.
        mCoordinator.attachToHistoryItem(
                List.of(),
                /* expanded= */ false,
                /* selectionActive= */ false,
                /* onExpansionChanged= */ null);
        assertNull(mCoordinator.getExpansionAnimatorForTesting());
        assertEquals(View.GONE, mPanelContainer.getVisibility());

        // Reattaching it to an expanded item shows it expanded right away.
        mCoordinator.attachToHistoryItem(
                labels,
                /* expanded= */ true,
                /* selectionActive= */ false,
                /* onExpansionChanged= */ null);
        assertNull(mCoordinator.getExpansionAnimatorForTesting());
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        assertEquals(180f, mExpandIcon.getRotation(), 0f);
    }

    @Test
    public void testReattachWithSameStateEndsAnimation() {
        List<String> labels = List.of("Filled a form");
        mCoordinator.attachToHistoryItem(
                labels,
                /* expanded= */ false,
                /* selectionActive= */ false,
                /* onExpansionChanged= */ null);
        mExpandIcon.performClick();
        assertNotNull(mCoordinator.getExpansionAnimatorForTesting());

        // Reattaching the same item, now expanded, finishes expanding it right away.
        mCoordinator.attachToHistoryItem(
                labels,
                /* expanded= */ true,
                /* selectionActive= */ false,
                /* onExpansionChanged= */ null);
        assertNull(mCoordinator.getExpansionAnimatorForTesting());
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        assertEquals(ViewGroup.LayoutParams.WRAP_CONTENT, mPanelContainer.getLayoutParams().height);
        assertEquals(180f, mExpandIcon.getRotation(), 0f);
    }

    @Test
    public void testSelectionModeHidesExpandIcon() {
        mCoordinator.attachToHistoryItem(
                List.of("Filled a form"),
                /* expanded= */ false,
                /* selectionActive= */ false,
                /* onExpansionChanged= */ null);
        assertEquals(View.VISIBLE, mExpandIcon.getVisibility());
        assertEquals("Expand section", getClickActionLabel(mExpandIcon));

        mCoordinator.setSelectionActive(true);
        assertEquals(View.GONE, mExpandIcon.getVisibility());
        assertNull(getClickActionLabel(mExpandIcon));

        mCoordinator.setSelectionActive(false);
        assertEquals(View.VISIBLE, mExpandIcon.getVisibility());
        assertEquals("Expand section", getClickActionLabel(mExpandIcon));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CRITICAL_ACTION_HISTORY)
    public void testFeatureDisabledDoesNotInflatePanel() {
        mCoordinator.attachToHistoryItem(
                List.of("Filled a form"),
                /* expanded= */ true,
                /* selectionActive= */ false,
                /* onExpansionChanged= */ null);
        assertEquals(View.GONE, mExpandIcon.getVisibility());
        assertEquals(View.GONE, mPanelContainer.getVisibility());
        assertEquals(0, mPanelContainer.getChildCount());
        assertFalse(mExpandIcon.hasOnClickListeners());
        assertNull(getClickActionLabel(mExpandIcon));
    }

    private void endExpansionAnimation() {
        ValueAnimator animator = mCoordinator.getExpansionAnimatorForTesting();
        assertNotNull(animator);
        animator.end();
    }

    private static @Nullable CharSequence getClickActionLabel(View view) {
        for (AccessibilityNodeInfo.AccessibilityAction action :
                view.createAccessibilityNodeInfo().getActionList()) {
            if (action.getId() == AccessibilityNodeInfo.ACTION_CLICK) return action.getLabel();
        }
        return null;
    }
}
