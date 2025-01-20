// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabFakeTabSwitcherButton.ANIMATION_DURATION_MS;

import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorSet;
import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabFakeTabSwitcherButton.TranslateDirection;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class NewBackgroundTabFakeTabSwitcherButtonRenderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .setRevision(0)
                    .build();

    private FrameLayout mRootView;
    private NewBackgroundTabFakeTabSwitcherButton mFakeTabSwitcherButton;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(false);
        mRenderTestRule.setNightModeEnabled(false);

        CallbackHelper onFirstLayout = new CallbackHelper();
        runOnUiThreadBlocking(
                () -> {
                    mRootView = new FrameLayout(sActivity);
                    sActivity.setContentView(
                            mRootView,
                            new ViewGroup.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT));
                    mFakeTabSwitcherButton =
                            (NewBackgroundTabFakeTabSwitcherButton)
                                    LayoutInflater.from(sActivity)
                                            .inflate(
                                                    R.layout
                                                            .new_background_tab_animation_tab_switcher_icon,
                                                    mRootView,
                                                    false);
                    mRootView.addView(mFakeTabSwitcherButton);
                    mFakeTabSwitcherButton.runOnNextLayout(onFirstLayout::notifyCalled);
                });

        onFirstLayout.waitForOnly();
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFakeTabSwitcherButton() throws Exception {
        Runnable updateRunnable =
                () -> {
                    mFakeTabSwitcherButton.setTabCount(/* tabCount= */ 5, /* isIncognito= */ false);
                };
        runUpdateOnUiThreadAndPerformRender(updateRunnable, "fake_tab_switcher_button_5");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFakeTabSwitcherButton_Notification() throws Exception {
        Runnable updateRunnable =
                () -> {
                    mFakeTabSwitcherButton.setTabCount(/* tabCount= */ 3, /* isIncognito= */ false);
                    mFakeTabSwitcherButton.setNotificationIconStatus(/* shouldShow= */ true);
                };
        runUpdateOnUiThreadAndPerformRender(
                updateRunnable, "fake_tab_switcher_button_3_notification");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFakeTabSwitcherButton_Incognito() throws Exception {
        Runnable updateRunnable =
                () -> {
                    mFakeTabSwitcherButton.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
                    mFakeTabSwitcherButton.setTabCount(/* tabCount= */ 4, /* isIncognito= */ true);
                };
        runUpdateOnUiThreadAndPerformRender(updateRunnable, "fake_tab_switcher_button_4_incognito");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFakeTabSwitcherButton_Rotate() throws Exception {
        AnimatorSet animator =
                runOnUiThreadBlocking(
                        () -> {
                            mFakeTabSwitcherButton.setTabCount(
                                    /* tabCount= */ 1, /* isIncognito= */ false);
                            return mFakeTabSwitcherButton.getRotateAnimator(
                                    /* incrementCount= */ true);
                        });
        runAnimatorAndPerformRenders(animator, "fake_tab_switcher_button_rotate_1+");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFakeTabSwitcherButton_TranslateUp() throws Exception {
        AnimatorSet animator =
                runOnUiThreadBlocking(
                        () -> {
                            mFakeTabSwitcherButton.setTabCount(
                                    /* tabCount= */ 10, /* isIncognito= */ false);
                            return mFakeTabSwitcherButton.getTranslateAnimator(
                                    TranslateDirection.UP, /* incrementCount= */ true);
                        });
        runAnimatorAndPerformRenders(animator, "fake_tab_switcher_button_translate_up_10+");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFakeTabSwitcherButton_TranslateDown() throws Exception {
        AnimatorSet animator =
                runOnUiThreadBlocking(
                        () -> {
                            mFakeTabSwitcherButton.setTabCount(
                                    /* tabCount= */ 7, /* isIncognito= */ false);
                            return mFakeTabSwitcherButton.getTranslateAnimator(
                                    TranslateDirection.DOWN, /* incrementCount= */ false);
                        });
        runAnimatorAndPerformRenders(animator, "fake_tab_switcher_button_translate_down_7");
    }

    private void runUpdateOnUiThreadAndPerformRender(Runnable r, String renderTitle)
            throws Exception {
        CallbackHelper onInvalidated = new CallbackHelper();
        runOnUiThreadBlocking(
                () -> {
                    r.run();

                    mFakeTabSwitcherButton.postInvalidate();
                    mFakeTabSwitcherButton.post(onInvalidated::notifyCalled);
                });

        onInvalidated.waitForOnly();
        mRenderTestRule.render(mFakeTabSwitcherButton, renderTitle);
    }

    private void runAnimatorAndPerformRenders(AnimatorSet animator, String renderTitlePrefix)
            throws Exception {
        // Use a linear interpolator so the behavior is easier to follow.
        animator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        Callback<Long> advanceToTime =
                (time) -> {
                    assertTrue(mFakeTabSwitcherButton.hasOutstandingAnimator());
                    animator.setCurrentPlayTime(time);
                };
        Runnable startRunnable =
                () -> {
                    // Calling start will cause the animation to begin playing which will mean we
                    // can't capture consistent stills. We will manually advance the animator
                    // instead.
                    for (AnimatorListener listener : animator.getListeners()) {
                        listener.onAnimationStart(animator);
                    }
                    advanceToTime.onResult(0L);
                };
        runUpdateOnUiThreadAndPerformRender(startRunnable, renderTitlePrefix + "_start");

        runUpdateOnUiThreadAndPerformRender(
                advanceToTime.bind((long) Math.round(ANIMATION_DURATION_MS / 2f)),
                renderTitlePrefix + "_midpoint");

        runUpdateOnUiThreadAndPerformRender(
                advanceToTime.bind(ANIMATION_DURATION_MS), renderTitlePrefix + "_before_end");

        Runnable endRunnable =
                () -> {
                    // This calls the listeners and resets everything.
                    animator.end();
                    assertFalse(mFakeTabSwitcherButton.hasOutstandingAnimator());
                };
        runUpdateOnUiThreadAndPerformRender(endRunnable, renderTitlePrefix + "_after_reset");
    }
}
