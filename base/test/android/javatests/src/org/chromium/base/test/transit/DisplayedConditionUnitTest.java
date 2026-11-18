// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.is;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link DisplayedCondition} layout-pending checks. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class DisplayedConditionUnitTest {
    private Activity mActivity;
    private LinearLayout mContainer;
    private TextView mTargetView;
    private TextView mSiblingView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mContainer = new LinearLayout(mActivity);
        mContainer.setOrientation(LinearLayout.VERTICAL);

        mSiblingView = new TextView(mActivity);
        mSiblingView.setText("Sibling");
        mContainer.addView(
                mSiblingView,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        mTargetView = new TextView(mActivity);
        mTargetView.setText("Initial");
        mContainer.addView(
                mTargetView,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        mActivity.setContentView(mContainer);
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testUnfulfilledWhileMatchedViewLayoutRequested() throws Exception {
        DisplayedCondition<TextView> condition =
                new DisplayedCondition<>(
                        withText("Updated longer label"),
                        TextView.class,
                        () -> RootSpec.activityRoot(mActivity),
                        DisplayedCondition.newOptions().build());
        condition.onStartMonitoring();

        mTargetView.setText("Updated longer label");
        assertThat(mTargetView.isLayoutRequested()).isTrue();

        ConditionStatus statusBeforeLayout = condition.check();
        assertThat(statusBeforeLayout.isFulfilled()).isFalse();
        assertThat(statusBeforeLayout.getMessage()).contains("layout requested");

        ShadowLooper.idleMainLooper();
        assertThat(mTargetView.isLayoutRequested()).isFalse();

        ConditionStatus statusAfterLayout = condition.check();
        assertThat(statusAfterLayout.isFulfilled()).isTrue();
    }

    @Test
    public void testUnfulfilledWhileSiblingRequestsRootLayout() throws Exception {
        DisplayedCondition<TextView> condition =
                new DisplayedCondition<>(
                        is(mTargetView),
                        TextView.class,
                        () -> RootSpec.activityRoot(mActivity),
                        DisplayedCondition.newOptions().build());
        condition.onStartMonitoring();

        assertThat(condition.check().isFulfilled()).isTrue();

        mSiblingView.setText("Updated sibling");
        assertThat(mTargetView.isLayoutRequested()).isFalse();
        assertThat(mActivity.getWindow().getDecorView().isLayoutRequested()).isTrue();

        ConditionStatus statusBeforeLayout = condition.check();
        assertThat(statusBeforeLayout.isFulfilled()).isFalse();
        assertThat(statusBeforeLayout.getMessage()).contains("root layout requested");

        ShadowLooper.idleMainLooper();

        ConditionStatus statusAfterLayout = condition.check();
        assertThat(statusAfterLayout.isFulfilled()).isTrue();
    }

    @Test
    public void testExpectGoneWaitsForRootLayoutButIgnoresChildLayoutRequestedFlag()
            throws Exception {
        DisplayedCondition<TextView> condition =
                new DisplayedCondition<>(
                        is(mTargetView),
                        TextView.class,
                        () -> RootSpec.activityRoot(mActivity),
                        DisplayedCondition.newOptions()
                                .withEffectiveVisibility(View.GONE)
                                .withDisplayingAtLeast(0)
                                .build());
        condition.onStartMonitoring();

        mTargetView.setVisibility(View.GONE);
        assertThat(mTargetView.isLayoutRequested()).isTrue();
        assertThat(mActivity.getWindow().getDecorView().isLayoutRequested()).isTrue();

        ConditionStatus statusBeforeLayout = condition.check();
        assertThat(statusBeforeLayout.isFulfilled()).isFalse();
        assertThat(statusBeforeLayout.getMessage()).contains("root layout requested");

        ShadowLooper.idleMainLooper();
        // ViewGroup skips layout() on GONE children, so mTargetView.isLayoutRequested() stays true
        // while the root DecorView clears its flag.
        assertThat(mTargetView.isLayoutRequested()).isTrue();
        assertThat(mActivity.getWindow().getDecorView().isLayoutRequested()).isFalse();

        ConditionStatus statusAfterLayout = condition.check();
        assertThat(statusAfterLayout.isFulfilled()).isTrue();
    }
}
