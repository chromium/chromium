// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabAnimationHostView.CROSS_FADE_DURATION_MS;
import static org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabFakeTabSwitcherButton.SCALE_DOWN_DURATION_MS;
import static org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabFakeTabSwitcherButton.SHRINK_DURATION_MS;
import static org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabFakeTabSwitcherButton.TRANSLATE_DURATION_MS;

import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorSet;
import android.app.Activity;
import android.content.res.Resources;
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
                    .setRevision(2)
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

                    // {@link NewBackgroundTabFakeTabSwitcherButton} has "match_parent" for width
                    // and height. To avoid large renders, we limit the content view to match {@link
                    // NewBackgroundTabFakeTabSwitcherButton#mInnerContainer}.
                    Resources res = sActivity.getResources();
                    int width = res.getDimensionPixelSize(R.dimen.toolbar_button_width);
                    int height = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
                    sActivity.setContentView(mRootView, new ViewGroup.LayoutParams(width, height));

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
    public void testRenderFakeTabSwitcherButton_Ntp() throws Exception {
        Runnable updateRunnable =
                () -> {
                    mFakeTabSwitcherButton.setTabCount(/* tabCount= */ 7, /* isIncognito= */ false);
                    mFakeTabSwitcherButton.setUpNtpAnimation(/* incrementCount= */ false);
                    mFakeTabSwitcherButton.setTabSwitcherButtonViewAlphaForTesting(1f);
                };
        runUpdateOnUiThreadAndPerformRender(updateRunnable, "fake_tab_switcher_button_ntp_7");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFakeTabSwitcherButton_Shrink() throws Exception {
        AnimatorSet animator =
                runOnUiThreadBlocking(
                        () -> {
                            mFakeTabSwitcherButton.setTabCount(
                                    /* tabCount= */ 1, /* isIncognito= */ false);
                            return mFakeTabSwitcherButton.getShrinkAnimator(
                                    /* incrementCount= */ true);
                        });
        runAnimatorAndPerformRenders(
                animator, "fake_tab_switcher_button_shrink_1+", SHRINK_DURATION_MS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFakeTabSwitcherButton_ScaleFade() throws Exception {
        AnimatorSet animator =
                runOnUiThreadBlocking(
                        () -> {
                            mFakeTabSwitcherButton.setTabCount(
                                    /* tabCount= */ 2, /* isIncognito= */ false);
                            mFakeTabSwitcherButton.setUpNtpAnimation(/* incrementCount= */ false);

                            return mFakeTabSwitcherButton.getScaleFadeAnimator();
                        });
        runAnimatorAndPerformRenders(
                animator, "fake_tab_switcher_button_scale_fade_2", CROSS_FADE_DURATION_MS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFakeTabSwitcherButton_ScaleDown() throws Exception {
        AnimatorSet animator =
                runOnUiThreadBlocking(
                        () -> {
                            mFakeTabSwitcherButton.setTabCount(
                                    /* tabCount= */ 2, /* isIncognito= */ false);
                            mFakeTabSwitcherButton.setUpNtpAnimation(/* incrementCount= */ true);
                            mFakeTabSwitcherButton.setTabSwitcherButtonViewAlphaForTesting(1f);

                            return mFakeTabSwitcherButton.getScaleDownAnimator();
                        });
        runAnimatorAndPerformRenders(
                animator, "fake_tab_switcher_button_scale_down_2+", SCALE_DOWN_DURATION_MS);
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
                            mFakeTabSwitcherButton.setUpNtpAnimation(/* incrementCount= */ true);
                            mFakeTabSwitcherButton.setTabSwitcherButtonViewAlphaForTesting(1f);

                            return mFakeTabSwitcherButton.getTranslateAnimator(
                                    TranslateDirection.UP);
                        });
        runAnimatorAndPerformRenders(
                animator, "fake_tab_switcher_button_translate_up_10+", TRANSLATE_DURATION_MS);
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
                            mFakeTabSwitcherButton.setUpNtpAnimation(/* incrementCount= */ false);
                            mFakeTabSwitcherButton.setTabSwitcherButtonViewAlphaForTesting(1f);

                            return mFakeTabSwitcherButton.getTranslateAnimator(
                                    TranslateDirection.DOWN);
                        });
        runAnimatorAndPerformRenders(
                animator, "fake_tab_switcher_button_translate_down_7", TRANSLATE_DURATION_MS);
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

    private void runAnimatorAndPerformRenders(
            AnimatorSet animator, String renderTitlePrefix, long duration) throws Exception {
        // Use a linear interpolator so the behavior is easier to follow.
        animator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        Callback<Long> advanceToTime = animator::setCurrentPlayTime;
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
                advanceToTime.bind((long) Math.round(duration / 2f)),
                renderTitlePrefix + "_midpoint");

        Runnable endRunnable =
                () -> {
                    advanceToTime.onResult(duration);
                    animator.end();
                };
        runUpdateOnUiThreadAndPerformRender(endRunnable, renderTitlePrefix + "_end");
    }
}
