// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
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

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
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
        mFakeTabSwitcherButton =
                mHostView.findViewById(R.id.new_background_tab_fake_tab_switcher_button);

        mActivity.setContentView(mHostView);
    }

    @Test
    public void testUpdateFakeTabSwitcherButton_Default() {
        // Default for non-NTP tab (button visibility at the start of the animation is irrelevant).
        setButtonVisibility(false);
        when(mTabSwitcherButton.shouldShowNotificationIcon()).thenReturn(true);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 12,
                /* backgroundColor= */ Color.WHITE,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* yOffset= */ 7,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertDefaultSettings(
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT,
                /* tabCount= */ 12,
                /* buttonColor= */ Color.WHITE,
                /* brandedColorScheme= */ BrandedColorScheme.APP_DEFAULT,
                /* showNotificationIcon= */ true,
                /* yOffset= */ 7);

        when(mTabSwitcherButton.shouldShowNotificationIcon()).thenReturn(false);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 12,
                /* backgroundColor= */ Color.WHITE,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* yOffset= */ 7,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertFalse(mFakeTabSwitcherButton.getShowIconNotificationStatusForTesting());

        // Default for NTP.
        setButtonVisibility(true);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 12,
                /* backgroundColor= */ Color.WHITE,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* yOffset= */ 7,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertDefaultSettings(
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT,
                /* tabCount= */ 12,
                /* buttonColor= */ Color.WHITE,
                /* brandedColorScheme= */ BrandedColorScheme.APP_DEFAULT,
                /* showNotificationIcon= */ false,
                /* yOffset= */ 7);
    }

    @Test
    public void testUpdateFakeTabSwitcherButton_NtpPartialScroll() {
        setButtonVisibility(false);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 9,
                /* backgroundColor= */ Color.CYAN,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* yOffset= */ 7,
                /* ntpToolbarTransitionPercentage= */ 0.5f);
        assertNtpSettings(
                NewBackgroundTabAnimationHostView.AnimationType.NTP_PARTIAL_SCROLL,
                /* tabCount= */ 9,
                /* yOffset= */ 7);
    }

    @Test
    public void testUpdateFakeTabSwitcherButton_NtpFullScroll() {
        setButtonVisibility(false);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 9,
                /* backgroundColor= */ Color.CYAN,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* yOffset= */ 7,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertNtpSettings(
                NewBackgroundTabAnimationHostView.AnimationType.NTP_FULL_SCROLL,
                /* tabCount= */ 9,
                /* yOffset= */ 7);
    }

    @Test(expected = AssertionError.class)
    public void testGetAnimatorSet_Uninitialized() {
        mHostView.getAnimatorSet(/* originX= */ 0, /* originY= */ 0, /* statusBarHeight= */ 0);
    }

    @Test
    public void testGetAnimatorSet_Default() {
        setButtonVisibility(true);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 9,
                /* backgroundColor= */ Color.CYAN,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* yOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        AnimatorSet animatorSet =
                mHostView.getAnimatorSet(
                        /* originX= */ -1, /* originY= */ -1, /* statusBarHeight= */ 0);
        ArrayList<Animator> animators = animatorSet.getChildAnimations();
        assertEquals(3, animators.size());
        AnimatorSet transitionAnimator = (AnimatorSet) animators.get(1);
        assertEquals(3, transitionAnimator.getChildAnimations().size());
    }

    @Test
    public void testGetAnimatorSet_NtpPartialScroll() {
        setButtonVisibility(false);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 9,
                /* backgroundColor= */ Color.CYAN,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* yOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 0.4f);
        AnimatorSet animatorSet =
                mHostView.getAnimatorSet(
                        /* originX= */ -1, /* originY= */ -1, /* statusBarHeight= */ 0);
        ArrayList<Animator> animators = animatorSet.getChildAnimations();
        assertEquals(4, animators.size());
        AnimatorSet transitionAnimator = (AnimatorSet) animators.get(1);
        assertEquals(4, transitionAnimator.getChildAnimations().size());
    }

    @Test
    public void testGetAnimatorSet_NtpFullScroll() {
        setButtonVisibility(false);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 9,
                /* backgroundColor= */ Color.CYAN,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* yOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        AnimatorSet animatorSet =
                mHostView.getAnimatorSet(
                        /* originX= */ -1, /* originY= */ -1, /* statusBarHeight= */ 0);
        ArrayList<Animator> animators = animatorSet.getChildAnimations();
        assertEquals(4, animators.size());
        AnimatorSet transitionAnimator = (AnimatorSet) animators.get(1);
        assertEquals(4, transitionAnimator.getChildAnimations().size());
    }

    @Test
    public void testBrandedColorScheme() {
        setButtonVisibility(true);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 0,
                /* backgroundColor= */ Color.WHITE,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* yOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertEquals(
                BrandedColorScheme.APP_DEFAULT,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());

        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 0,
                /* backgroundColor= */ Color.WHITE,
                /* isNtp= */ false,
                /* isIncognito= */ true,
                /* yOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertEquals(
                BrandedColorScheme.INCOGNITO,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());

        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 0,
                /* backgroundColor= */ Color.GREEN,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* yOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertEquals(
                BrandedColorScheme.LIGHT_BRANDED_THEME,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());

        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 0,
                /* backgroundColor= */ Color.RED,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* yOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertEquals(
                BrandedColorScheme.DARK_BRANDED_THEME,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());
    }

    private void setButtonVisibility(boolean isVisible) {
        doAnswer(
                        invocation -> {
                            Rect rect = invocation.getArgument(0);
                            rect.set(new Rect(50, 0, 120, 100));
                            return isVisible;
                        })
                .when(mTabSwitcherButton)
                .getGlobalVisibleRect(any());
    }

    private void assertCommonElements(
            @NewBackgroundTabAnimationHostView.AnimationType int animationType,
            int tabCount,
            FrameLayout.LayoutParams params) {
        assertEquals(animationType, mHostView.getAnimationTypeForTesting());
        assertEquals(tabCount, mFakeTabSwitcherButton.getTabCountForTesting());
        assertEquals(46, params.leftMargin);
    }

    private void assertDefaultSettings(
            @NewBackgroundTabAnimationHostView.AnimationType int animationType,
            int tabCount,
            @ColorInt int buttonColor,
            @BrandedColorScheme int brandedColorScheme,
            boolean showNotificationIcon,
            int yOffset) {
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFakeTabSwitcherButton.getLayoutParams();
        assertCommonElements(animationType, tabCount, params);
        assertEquals(yOffset, params.topMargin);
        assertEquals(buttonColor, mFakeTabSwitcherButton.getButtonColorForTesting());
        assertEquals(brandedColorScheme, mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());
        assertEquals(
                showNotificationIcon,
                mFakeTabSwitcherButton.getShowIconNotificationStatusForTesting());
        assertEquals(1f, mFakeTabSwitcherButton.getAlpha(), MathUtils.EPSILON);
    }

    private void assertNtpSettings(
            @NewBackgroundTabAnimationHostView.AnimationType int animationType,
            int tabCount,
            int yOffset) {
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFakeTabSwitcherButton.getLayoutParams();
        assertCommonElements(animationType, tabCount, params);
        int height = yOffset;
        if (animationType == NewBackgroundTabAnimationHostView.AnimationType.NTP_FULL_SCROLL) {
            height +=
                    Math.round(
                            mActivity
                                    .getResources()
                                    .getDimension(R.dimen.toolbar_height_no_shadow));
        }
        assertEquals(height, params.topMargin);
        assertEquals(0f, mFakeTabSwitcherButton.getAlpha(), MathUtils.EPSILON);
    }
}
