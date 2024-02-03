// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.Locale;

/** Render tests for Quick Delete animation. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class QuickDeleteAnimationGradientDrawableRenderTest {
    private static final int VIEW_HEIGHT = 2000;
    private static final int VIEW_WIDTH = 500;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.PRIVACY)
                    .build();

    private Activity mActivity;
    private FrameLayout mFrameLayout;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity = sActivityTestRule.getActivity();
                    mFrameLayout = new FrameLayout(mActivity);
                    ViewGroup.LayoutParams layoutParams =
                            new ViewGroup.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT);
                    mActivity.setContentView(mFrameLayout, layoutParams);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testWipeAnimation() throws Exception {
        View testView = setUpTestView();
        QuickDeleteAnimationGradientDrawable drawable =
                QuickDeleteAnimationGradientDrawable.createQuickDeleteWipeAnimationDrawable(
                        mActivity, VIEW_HEIGHT);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    testView.setForeground(drawable);
                });
        ObjectAnimator animator = drawable.createWipeAnimator(VIEW_HEIGHT);

        runAnimationForRenderTest("quick_delete_wipe_animation", 0.15F, animator, testView);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testFadeAnimation() throws Exception {
        View testView = setUpTestView();
        QuickDeleteAnimationGradientDrawable drawable =
                QuickDeleteAnimationGradientDrawable.createQuickDeleteFadeAnimationDrawable(
                        mActivity, VIEW_HEIGHT);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    testView.setForeground(drawable);
                });
        ObjectAnimator animator = drawable.createFadeAnimator(VIEW_HEIGHT);

        runAnimationForRenderTest("quick_delete_fade_animation", 0.25F, animator, testView);
    }

    private void runAnimationForRenderTest(
            String testcaseName, float stepFraction, ObjectAnimator animator, View view)
            throws Exception {
        for (int i = 0; i < 5; i++) {
            final float animatorFraction = stepFraction * i;
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        animator.setCurrentFraction(animatorFraction);
                    });
            mRenderTestRule.render(
                    view, String.format(Locale.ENGLISH, "%s_step_%d", testcaseName, i));
        }
    }

    private View setUpTestView() {
        View view = new View(mActivity);
        view.setBackgroundColor(Color.BLACK);
        ViewGroup.LayoutParams layoutParams = new ViewGroup.LayoutParams(VIEW_WIDTH, VIEW_HEIGHT);
        TestThreadUtils.runOnUiThreadBlocking(() -> mFrameLayout.addView(view, layoutParams));
        return view;
    }
}
