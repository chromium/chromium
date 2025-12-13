// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabAnimationHostView.AnimationType;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;

/** Unit tests for {@link NewBackgroundTabAnimationHostView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NewBackgroundTabAnimationHostViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ToggleTabStackButton mTabSwitcherButton;

    private Activity mActivity;
    private NewBackgroundTabAnimationHostView mHostView;
    private NewBackgroundTabFakeTabSwitcherButton mFakeTabSwitcherButton;
    private FrameLayout mFakeTabSwitcherInnerContainer;
    private ImageView mFakeTabSwitcherButtonView;
    private ImageView mLinkIconView;
    private Rect mTabSwitcherRect;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
        mTabSwitcherRect = new Rect(50, 0, 120, 100);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mHostView =
                (NewBackgroundTabAnimationHostView)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        R.layout.new_background_tab_animation_host_view,
                                        null,
                                        false);
        mLinkIconView = mHostView.findViewById(R.id.new_tab_background_animation_link_icon);
        mFakeTabSwitcherButton =
                mHostView.findViewById(R.id.new_background_tab_fake_tab_switcher_button);
        mFakeTabSwitcherInnerContainer =
                mFakeTabSwitcherButton.findViewById(R.id.new_tab_indicator_inner_container);
        mFakeTabSwitcherButtonView =
                mFakeTabSwitcherInnerContainer.findViewById(R.id.fake_tab_switcher_button);

        mActivity.setContentView(mHostView);
    }

    @Test
    public void testCalculateAnimationType() {
        assertEquals(
                AnimationType.DEFAULT,
                NewBackgroundTabAnimationHostView.calculateAnimationType(
                        /* tabSwitcherButtonIsVisible= */ true,
                        /* isNtp= */ false,
                        /* ntpToolbarTransitionPercentage= */ 0f));
        assertEquals(
                AnimationType.DEFAULT,
                NewBackgroundTabAnimationHostView.calculateAnimationType(
                        /* tabSwitcherButtonIsVisible= */ true,
                        /* isNtp= */ true,
                        /* ntpToolbarTransitionPercentage= */ 0f));
        assertEquals(
                AnimationType.NTP_PARTIAL_SCROLL,
                NewBackgroundTabAnimationHostView.calculateAnimationType(
                        /* tabSwitcherButtonIsVisible= */ false,
                        /* isNtp= */ true,
                        /* ntpToolbarTransitionPercentage= */ 0f));
        assertEquals(
                AnimationType.NTP_PARTIAL_SCROLL,
                NewBackgroundTabAnimationHostView.calculateAnimationType(
                        /* tabSwitcherButtonIsVisible= */ false,
                        /* isNtp= */ true,
                        /* ntpToolbarTransitionPercentage= */ 0.5f));
        assertEquals(
                AnimationType.NTP_FULL_SCROLL,
                NewBackgroundTabAnimationHostView.calculateAnimationType(
                        /* tabSwitcherButtonIsVisible= */ false,
                        /* isNtp= */ true,
                        /* ntpToolbarTransitionPercentage= */ 1f));
    }

    @Test
    public void testSetUpAnimation_Default() {
        when(mTabSwitcherButton.shouldShowNotificationIcon()).thenReturn(true);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                mTabSwitcherRect,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 12,
                /* toolbarHeight= */ 30,
                /* statusBarHeight= */ 5,
                /* xOffset= */ 3);

        assertDefaultSettings(
                /* tabCount= */ 12,
                /* topMargin= */ 25,
                /* leftMargin= */ 47,
                /* showNotificationIcon= */ true);

        when(mTabSwitcherButton.shouldShowNotificationIcon()).thenReturn(false);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                mTabSwitcherRect,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 12,
                /* toolbarHeight= */ 7,
                /* statusBarHeight= */ 10,
                /* xOffset= */ 3);
        assertFalse(mFakeTabSwitcherButton.getShowIconNotificationStatusForTesting());

        mHostView.setUpAnimation(
                mTabSwitcherButton,
                mTabSwitcherRect,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 56,
                /* toolbarHeight= */ 94,
                /* statusBarHeight= */ 10,
                /* xOffset= */ 5);

        assertDefaultSettings(
                /* tabCount= */ 56,
                /* topMargin= */ 84,
                /* leftMargin= */ 45,
                /* showNotificationIcon= */ false);
    }

    @Test
    public void testSetUpAnimation_NtpPartialScroll() {
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                mTabSwitcherRect,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                NewBackgroundTabAnimationHostView.AnimationType.NTP_PARTIAL_SCROLL,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 38,
                /* toolbarHeight= */ 7,
                /* statusBarHeight= */ 3,
                /* xOffset= */ 1);

        assertNtpSettings(
                AnimationType.NTP_PARTIAL_SCROLL,
                /* tabCount= */ 38,
                /* topMargin= */ 4,
                /* leftMargin= */ 49);
    }

    @Test
    public void testSetUpAnimation_NtpFullScroll() {
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                mTabSwitcherRect,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                NewBackgroundTabAnimationHostView.AnimationType.NTP_FULL_SCROLL,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 9,
                /* toolbarHeight= */ 12,
                /* statusBarHeight= */ 10,
                /* xOffset= */ 15);
        assertNtpSettings(
                AnimationType.NTP_FULL_SCROLL,
                /* tabCount= */ 9,
                /* topMargin= */ 2,
                /* leftMargin= */ 35);
    }

    @Test(expected = AssertionError.class)
    public void testGetAnimatorSet_Uninitialized() {
        mHostView.getAnimatorSet(/* originX= */ 0, /* originY= */ 0);
    }

    @Test
    public void testGetAnimatorSet_Default() {
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                mTabSwitcherRect,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 9,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0);

        AnimatorSet animatorSet = mHostView.getAnimatorSet(/* originX= */ -1, /* originY= */ -1);
        ArrayList<Animator> animators = animatorSet.getChildAnimations();
        assertEquals(3, animators.size());
        AnimatorSet transitionAnimator = (AnimatorSet) animators.get(0);
        assertEquals(3, transitionAnimator.getChildAnimations().size());
    }

    @Test
    public void testGetAnimatorSet_NtpPartialScroll() {
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                mTabSwitcherRect,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                NewBackgroundTabAnimationHostView.AnimationType.NTP_PARTIAL_SCROLL,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 9,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0);

        AnimatorSet animatorSet = mHostView.getAnimatorSet(/* originX= */ -1, /* originY= */ -1);
        ArrayList<Animator> animators = animatorSet.getChildAnimations();
        assertEquals(4, animators.size());
        AnimatorSet transitionAnimator = (AnimatorSet) animators.get(1);
        assertEquals(2, transitionAnimator.getChildAnimations().size());
    }

    @Test
    public void testGetAnimatorSet_NtpFullScroll() {
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                mTabSwitcherRect,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                NewBackgroundTabAnimationHostView.AnimationType.NTP_FULL_SCROLL,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 9,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0);
        AnimatorSet animatorSet = mHostView.getAnimatorSet(/* originX= */ -1, /* originY= */ -1);
        ArrayList<Animator> animators = animatorSet.getChildAnimations();
        assertEquals(4, animators.size());
        AnimatorSet transitionAnimator = (AnimatorSet) animators.get(1);
        assertEquals(2, transitionAnimator.getChildAnimations().size());
    }

    @Test
    @Config(qualifiers = "night")
    public void testNightMode() {
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                mTabSwitcherRect,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 0,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0);

        GradientDrawable roundedRect = (GradientDrawable) mLinkIconView.getBackground();
        assertEquals(
                ColorStateList.valueOf(SemanticColorUtils.getColorSurfaceContainerHigh(mActivity)),
                roundedRect.getColor());
    }

    private void assertCommonElements(int tabCount, int leftMargin) {
        assertEquals(tabCount, mFakeTabSwitcherButton.getTabCountForTesting());

        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFakeTabSwitcherInnerContainer.getLayoutParams();
        assertEquals(leftMargin, params.leftMargin);
    }

    private void assertDefaultSettings(
            int tabCount, int topMargin, int leftMargin, boolean showNotificationIcon) {
        assertEquals(
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT,
                mHostView.getAnimationTypeForTesting());
        assertCommonElements(tabCount, leftMargin);
        assertEquals(
                showNotificationIcon,
                mFakeTabSwitcherButton.getShowIconNotificationStatusForTesting());
        assertEquals(1f, mFakeTabSwitcherButton.getAlpha(), MathUtils.EPSILON);

        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFakeTabSwitcherButton.getLayoutParams();
        assertEquals(topMargin, params.topMargin);
    }

    private void assertNtpSettings(
            @AnimationType int animationType, int tabCount, int topMargin, int leftMargin) {
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFakeTabSwitcherButton.getLayoutParams();
        // For Ntp, the tabCount increases when calling {@link
        // mFakeTabSwitcherButton#setUpNtpAnimation}.
        assertCommonElements(tabCount + 1, leftMargin);
        int height = topMargin;
        if (animationType == NewBackgroundTabAnimationHostView.AnimationType.NTP_FULL_SCROLL) {
            height +=
                    Math.round(
                            mActivity
                                    .getResources()
                                    .getDimension(R.dimen.toolbar_height_no_shadow));
        }
        assertEquals(height, params.topMargin);
        assertEquals(0f, mFakeTabSwitcherButtonView.getAlpha(), MathUtils.EPSILON);
    }
}
