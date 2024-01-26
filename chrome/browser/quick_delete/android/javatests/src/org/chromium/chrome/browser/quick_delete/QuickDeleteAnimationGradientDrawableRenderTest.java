// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.animation.ObjectAnimator;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.Locale;

/** Render tests for Quick Delete animation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class QuickDeleteAnimationGradientDrawableRenderTest {
    private static final int VIEW_HEIGHT = 2000;
    private static final int VIEW_WIDTH = 500;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.PRIVACY)
                    .build();

    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        setUpTestView();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testWipeAnimation() throws Exception {
        View testView = mActivity.getActivityTab().getContentView();
        QuickDeleteAnimationGradientDrawable drawable =
                QuickDeleteAnimationGradientDrawable.createQuickDeleteWipeAnimationDrawable(
                        mActivityTestRule.getActivity(), VIEW_HEIGHT);
        testView.setForeground(drawable);
        ObjectAnimator animator = (ObjectAnimator) drawable.createWipeAnimator(VIEW_HEIGHT);

        runAnimationForRenderTest("quick_delete_wipe_animation", animator, testView);
    }

    private void runAnimationForRenderTest(String testcaseName, ObjectAnimator animator, View view)
            throws Exception {
        for (int i = 0; i < 5; i++) {
            final float fraction = 0.15F * i;
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        animator.setCurrentFraction(fraction);
                    });
            mRenderTestRule.render(
                    view, String.format(Locale.ENGLISH, "%s_step_%d", testcaseName, i));
        }
    }

    private void setUpTestView() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View view = new View(mActivity);
                    view.setBackgroundColor(Color.WHITE);
                    ViewGroup.LayoutParams layoutParams =
                            new ViewGroup.LayoutParams(VIEW_WIDTH, VIEW_HEIGHT);
                    mActivityTestRule.runOnUiThread(
                            () -> mActivity.setContentView(view, layoutParams));
                });
    }
}
