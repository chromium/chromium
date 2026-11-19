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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.history.CriticalActionsProperties.Expansion;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link CriticalActionsViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CriticalActionsViewBinderTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private ImageView mExpandIcon;
    private FrameLayout mPanelContainer;
    private CriticalActionsExpansionAnimator mExpansionAnimator;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor<
                    PropertyModel, CriticalActionsViewBinder.ViewHolder, PropertyKey>
            mProcessor;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mExpandIcon = new ImageView(mActivity);
        mPanelContainer = new FrameLayout(mActivity);
        mPanelContainer.setVisibility(View.GONE);
        mExpansionAnimator = new CriticalActionsExpansionAnimator(mExpandIcon, mPanelContainer);
        mModel = new PropertyModel(CriticalActionsProperties.ALL_KEYS);
        mProcessor =
                PropertyModelChangeProcessor.create(
                        mModel,
                        new CriticalActionsViewBinder.ViewHolder(mExpandIcon, mExpansionAnimator),
                        CriticalActionsViewBinder::bindRow);
    }

    @After
    public void tearDown() {
        mProcessor.destroy();
    }

    @Test
    public void testExpandIcon() {
        mModel.set(CriticalActionsProperties.EXPAND_ICON_VISIBLE, true);
        assertEquals(View.VISIBLE, mExpandIcon.getVisibility());
        mModel.set(CriticalActionsProperties.EXPAND_ICON_VISIBLE, false);
        assertEquals(View.GONE, mExpandIcon.getVisibility());

        mModel.set(
                CriticalActionsProperties.EXPAND_ICON_CONTENT_DESCRIPTION,
                "2 key browsing actions");
        assertEquals("2 key browsing actions", mExpandIcon.getContentDescription());
    }

    @Test
    public void testExpanded() {
        mModel.set(CriticalActionsProperties.EXPANSION, Expansion.EXPANDED);
        assertNull(mExpansionAnimator.getAnimatorForTesting());
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        assertEquals(180f, mExpandIcon.getRotation(), 0f);

        mModel.set(CriticalActionsProperties.EXPANSION, Expansion.COLLAPSED);
        assertNull(mExpansionAnimator.getAnimatorForTesting());
        assertEquals(View.GONE, mPanelContainer.getVisibility());
        assertEquals(0f, mExpandIcon.getRotation(), 0f);
    }

    @Test
    public void testExpanded_Animated() {
        mModel.set(CriticalActionsProperties.EXPANSION, Expansion.ANIMATE_TO_EXPANDED);
        ValueAnimator animator = mExpansionAnimator.getAnimatorForTesting();
        assertNotNull(animator);
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());

        animator.end();
        assertEquals(180f, mExpandIcon.getRotation(), 0f);
    }

    @Test
    public void testExpandIconClick() {
        int[] clickCount = new int[1];
        mModel.set(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER, () -> clickCount[0]++);
        assertTrue(mExpandIcon.isClickable());
        mExpandIcon.performClick();
        assertEquals(1, clickCount[0]);

        mModel.set(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER, null);
        assertFalse(mExpandIcon.hasOnClickListeners());
        assertFalse(mExpandIcon.isClickable());
        mExpandIcon.performClick();
        assertEquals(1, clickCount[0]);
    }

    @Test
    public void testExpandIconClickAccessibilityLabel() {
        mModel.set(
                CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL, "Expand section");
        assertEquals("Expand section", getClickActionLabel(mExpandIcon));
        mModel.set(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL, null);
        assertNull(getClickActionLabel(mExpandIcon));
    }

    @Test
    public void testBindActionItem() {
        TextView view = new TextView(mActivity);
        PropertyModel itemModel =
                new PropertyModel.Builder(CriticalActionsProperties.ACTION_ITEM_KEYS)
                        .with(CriticalActionsProperties.LABEL, "Filled a form")
                        .build();
        PropertyModelChangeProcessor.create(
                itemModel, view, CriticalActionsViewBinder::bindActionItem);

        assertEquals("Filled a form", view.getText().toString());
    }

    private static @Nullable CharSequence getClickActionLabel(View view) {
        for (AccessibilityNodeInfo.AccessibilityAction action :
                view.createAccessibilityNodeInfo().getActionList()) {
            if (action.getId() == AccessibilityNodeInfo.ACTION_CLICK) return action.getLabel();
        }
        return null;
    }
}
