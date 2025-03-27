// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.animation.AnimatorSet;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;

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
        setButtonVisibility(true);
        when(mTabSwitcherButton.shouldShowNotificationIcon()).thenReturn(true);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 12,
                /* backgroundColor= */ Color.WHITE,
                /* isIncognito= */ false,
                /* yOffset= */ 7);
        assertFakeTabSwitcherButton(
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
                /* isIncognito= */ false,
                /* yOffset= */ 7);
        assertFalse(mFakeTabSwitcherButton.getShowIconNotificationStatusForTesting());
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
                /* isIncognito= */ false,
                /* yOffset= */ 0);
        AnimatorSet animatorSet =
                mHostView.getAnimatorSet(
                        /* originX= */ -1, /* originY= */ -1, /* statusBarHeight= */ 0);
        assertEquals(3, animatorSet.getChildAnimations().size());
    }

    @Test
    public void testBrandedColorScheme() {
        setButtonVisibility(true);
        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 0,
                /* backgroundColor= */ Color.WHITE,
                /* isIncognito= */ false,
                /* yOffset= */ 0);
        assertEquals(
                BrandedColorScheme.APP_DEFAULT,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());

        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 0,
                /* backgroundColor= */ Color.WHITE,
                /* isIncognito= */ true,
                /* yOffset= */ 0);
        assertEquals(
                BrandedColorScheme.INCOGNITO,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());

        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 0,
                /* backgroundColor= */ Color.GREEN,
                /* isIncognito= */ false,
                /* yOffset= */ 0);
        assertEquals(
                BrandedColorScheme.LIGHT_BRANDED_THEME,
                mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());

        mHostView.updateFakeTabSwitcherButton(
                mTabSwitcherButton,
                /* tabCount= */ 0,
                /* backgroundColor= */ Color.RED,
                /* isIncognito= */ false,
                /* yOffset= */ 0);
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

    private void assertFakeTabSwitcherButton(
            @NewBackgroundTabAnimationHostView.AnimationType int animationType,
            int tabCount,
            @ColorInt int buttonColor,
            @BrandedColorScheme int brandedColorScheme,
            boolean showNotificationIcon,
            int yOffset) {
        assertEquals(animationType, mHostView.getAnimationTypeForTesting());
        assertEquals(tabCount, mFakeTabSwitcherButton.getTabCountForTesting());

        if (animationType == NewBackgroundTabAnimationHostView.AnimationType.DEFAULT) {
            FrameLayout.LayoutParams params =
                    (FrameLayout.LayoutParams) mFakeTabSwitcherButton.getLayoutParams();
            assertEquals(46, params.leftMargin);
            assertEquals(yOffset, params.topMargin);
            assertEquals(buttonColor, mFakeTabSwitcherButton.getButtonColorForTesting());
            assertEquals(
                    brandedColorScheme, mFakeTabSwitcherButton.getBrandedColorSchemeForTesting());
            assertEquals(
                    showNotificationIcon,
                    mFakeTabSwitcherButton.getShowIconNotificationStatusForTesting());
            assertEquals(View.VISIBLE, mFakeTabSwitcherButton.getVisibility());
        }
    }
}
