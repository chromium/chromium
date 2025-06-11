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
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;

/** Unit tests for {@link NewBackgroundTabAnimationHostView}. */
@RunWith(BaseRobolectricTestRunner.class)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE
})
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
    public void testSetUpAnimation_Default() {
        // Default for non-NTP tab (button visibility at the start of the animation is irrelevant).
        setButtonVisibility(false);
        when(mTabSwitcherButton.shouldShowNotificationIcon()).thenReturn(true);

        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.WHITE,
                /* tabCount= */ 12,
                /* toolbarHeight= */ 30,
                /* statusBarHeight= */ 5,
                /* xOffset= */ 3,
                /* ntpToolbarTransitionPercentage= */ 1f);

        assertDefaultSettings(
                /* buttonColor= */ Color.WHITE,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 12,
                /* topMargin= */ 25,
                /* leftMargin= */ 47,
                /* showNotificationIcon= */ true);

        when(mTabSwitcherButton.shouldShowNotificationIcon()).thenReturn(false);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.WHITE,
                /* tabCount= */ 12,
                /* toolbarHeight= */ 7,
                /* statusBarHeight= */ 10,
                /* xOffset= */ 3,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertFalse(mFakeTabSwitcherButton.getShowIconNotificationStatusForTesting());

        // Default for NTP.
        setButtonVisibility(true);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.WHITE,
                /* tabCount= */ 56,
                /* toolbarHeight= */ 94,
                /* statusBarHeight= */ 10,
                /* xOffset= */ 5,
                /* ntpToolbarTransitionPercentage= */ 1f);

        assertDefaultSettings(
                Color.WHITE,
                BrandedColorScheme.APP_DEFAULT,
                /* tabCount= */ 56,
                /* topMargin= */ 84,
                /* leftMargin= */ 45,
                /* showNotificationIcon= */ false);
    }

    @Test
    public void testSetUpAnimation_NtpPartialScroll() {
        setButtonVisibility(false);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                /* tabCount= */ 38,
                /* toolbarHeight= */ 7,
                /* statusBarHeight= */ 3,
                /* xOffset= */ 1,
                /* ntpToolbarTransitionPercentage= */ 0.5f);

        assertNtpSettings(
                NewBackgroundTabAnimationHostView.AnimationType.NTP_PARTIAL_SCROLL,
                /* tabCount= */ 38,
                /* topMargin= */ 4,
                /* leftMargin= */ 49);
    }

    @Test
    public void testSetUpAnimation_NtpFullScroll() {
        setButtonVisibility(false);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                /* tabCount= */ 9,
                /* toolbarHeight= */ 12,
                /* statusBarHeight= */ 10,
                /* xOffset= */ 15,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertNtpSettings(
                NewBackgroundTabAnimationHostView.AnimationType.NTP_FULL_SCROLL,
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
        setButtonVisibility(true);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                /* tabCount= */ 9,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);

        AnimatorSet animatorSet = mHostView.getAnimatorSet(/* originX= */ -1, /* originY= */ -1);
        ArrayList<Animator> animators = animatorSet.getChildAnimations();
        assertEquals(3, animators.size());
        AnimatorSet transitionAnimator = (AnimatorSet) animators.get(0);
        assertEquals(3, transitionAnimator.getChildAnimations().size());
    }

    @Test
    public void testGetAnimatorSet_NtpPartialScroll() {
        setButtonVisibility(false);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                /* tabCount= */ 9,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 0.4f);

        AnimatorSet animatorSet = mHostView.getAnimatorSet(/* originX= */ -1, /* originY= */ -1);
        ArrayList<Animator> animators = animatorSet.getChildAnimations();
        assertEquals(4, animators.size());
        AnimatorSet transitionAnimator = (AnimatorSet) animators.get(1);
        assertEquals(2, transitionAnimator.getChildAnimations().size());
    }

    @Test
    public void testGetAnimatorSet_NtpFullScroll() {
        setButtonVisibility(false);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ true,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.CYAN,
                /* tabCount= */ 9,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        AnimatorSet animatorSet = mHostView.getAnimatorSet(/* originX= */ -1, /* originY= */ -1);
        ArrayList<Animator> animators = animatorSet.getChildAnimations();
        assertEquals(4, animators.size());
        AnimatorSet transitionAnimator = (AnimatorSet) animators.get(1);
        assertEquals(2, transitionAnimator.getChildAnimations().size());
    }

    @Test
    public void testBrandedColorScheme() {
        setButtonVisibility(true);
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.WHITE,
                /* tabCount= */ 0,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertEquals(
                BrandedColorScheme.APP_DEFAULT,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());

        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ false,
                /* isIncognito= */ true,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.WHITE,
                /* tabCount= */ 0,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertEquals(
                BrandedColorScheme.INCOGNITO,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());

        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.GREEN,
                /* tabCount= */ 0,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertEquals(
                BrandedColorScheme.LIGHT_BRANDED_THEME,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());

        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.RED,
                /* tabCount= */ 0,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 1f);
        assertEquals(
                BrandedColorScheme.DARK_BRANDED_THEME,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());
    }

    @Test
    @Config(qualifiers = "night")
    public void testNightMode() {
        mHostView.setUpAnimation(
                mTabSwitcherButton,
                /* isNtp= */ false,
                /* isIncognito= */ false,
                /* isTopToolbar= */ false,
                /* backgroundColor= */ Color.GREEN,
                /* tabCount= */ 0,
                /* toolbarHeight= */ 0,
                /* statusBarHeight= */ 0,
                /* xOffset= */ 0,
                /* ntpToolbarTransitionPercentage= */ 0f);

        GradientDrawable roundedRect = (GradientDrawable) mLinkIconView.getBackground();
        assertEquals(
                ColorStateList.valueOf(SemanticColorUtils.getColorSurfaceContainerHigh(mActivity)),
                roundedRect.getColor());
    }

    private void setButtonVisibility(boolean isVisible) {
        doAnswer(
                        invocation -> {
                            Rect rect = invocation.getArgument(0);
                            rect.set(mTabSwitcherRect);
                            return isVisible;
                        })
                .when(mTabSwitcherButton)
                .getGlobalVisibleRect(any());
    }

    private void assertCommonElements(
            @NewBackgroundTabAnimationHostView.AnimationType int animationType,
            int tabCount,
            int leftMargin) {
        assertEquals(animationType, mHostView.getAnimationTypeForTesting());
        assertEquals(tabCount, mFakeTabSwitcherButton.getTabCountForTesting());

        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFakeTabSwitcherInnerContainer.getLayoutParams();
        assertEquals(leftMargin, params.leftMargin);
    }

    private void assertDefaultSettings(
            @ColorInt int buttonColor,
            @BrandedColorScheme int brandedColorScheme,
            int tabCount,
            int topMargin,
            int leftMargin,
            boolean showNotificationIcon) {
        assertCommonElements(
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT, tabCount, leftMargin);
        assertEquals(buttonColor, mFakeTabSwitcherButton.getButtonColorForTesting());
        assertEquals(brandedColorScheme, mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());
        assertEquals(
                showNotificationIcon,
                mFakeTabSwitcherButton.getShowIconNotificationStatusForTesting());
        assertEquals(1f, mFakeTabSwitcherButton.getAlpha(), MathUtils.EPSILON);

        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFakeTabSwitcherButton.getLayoutParams();
        assertEquals(topMargin, params.topMargin);
    }

    private void assertNtpSettings(
            @NewBackgroundTabAnimationHostView.AnimationType int animationType,
            int tabCount,
            int topMargin,
            int leftMargin) {
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFakeTabSwitcherButton.getLayoutParams();
        // For Ntp, the tabCount increases when calling {@link
        // mFakeTabSwitcherButton#setUpNtpAnimation}.
        assertCommonElements(animationType, tabCount + 1, leftMargin);
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
