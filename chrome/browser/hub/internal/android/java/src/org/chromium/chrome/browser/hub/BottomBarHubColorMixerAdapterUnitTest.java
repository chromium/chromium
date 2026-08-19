// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.Animator;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.widget.ImageView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.bottombar.BottomBarUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarView;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.IncognitoColors;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.util.ColorUtils;

import java.util.List;

/** Unit tests for {@link BottomBarHubColorMixerAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarHubColorMixerAdapterUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HubColorMixer mHubColorMixer;

    private Activity mActivity;
    private BottomBarView mBottomBarView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mBottomBarView =
                (BottomBarView)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        org.chromium.chrome.browser.ui.bottombar.R.layout
                                                .bottom_bar_layout,
                                        null,
                                        false);
        mBottomBarView.setNewTabBackgroundVisible(true);
        mBottomBarView.resetColors();
    }

    @Test
    public void testRegisteredBlendsCount() {
        BottomBarHubColorMixerAdapter adapter =
                new BottomBarHubColorMixerAdapter(mBottomBarView, mHubColorMixer);
        ArgumentCaptor<HubViewColorBlend> captor = ArgumentCaptor.forClass(HubViewColorBlend.class);
        verify(mHubColorMixer, times(5)).registerBlend(captor.capture());
        assertEquals(5, captor.getAllValues().size());
        adapter.destroy();
    }

    @Test
    public void testInjectedRegistrationHelper() {
        HubColorMixerRegistrationHelper helper = mock(HubColorMixerRegistrationHelper.class);
        BottomBarHubColorMixerAdapter adapter =
                new BottomBarHubColorMixerAdapter(mBottomBarView, mHubColorMixer, helper);
        verify(helper, times(5)).registerBlend(any());
        verify(helper).setColorMixer(mHubColorMixer);

        adapter.destroy();
        verify(helper).destroy();
    }

    @Test
    public void testColorMixer_registeredBlendsUpdateColors() {
        BottomBarHubColorMixerAdapter adapter =
                new BottomBarHubColorMixerAdapter(mBottomBarView, mHubColorMixer);
        ArgumentCaptor<HubViewColorBlend> captor = ArgumentCaptor.forClass(HubViewColorBlend.class);
        verify(mHubColorMixer, times(5)).registerBlend(captor.capture());
        List<HubViewColorBlend> blends = captor.getAllValues();
        assertEquals(5, blends.size());

        // Update progress midway between DEFAULT and INCOGNITO
        for (HubViewColorBlend blend : blends) {
            blend.updateProgress(HubColorScheme.DEFAULT, HubColorScheme.INCOGNITO, 0.5f);
        }

        // 1. Verify background color is blended
        int defaultBg =
                BottomBarUtils.getBottomBarBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoBg =
                BottomBarUtils.getBottomBarBackgroundColor(mActivity, BrandedColorScheme.INCOGNITO);
        int expectedBg = ColorUtils.blendColorsMultiply(defaultBg, incognitoBg, 0.5f);

        ColorDrawable background = (ColorDrawable) mBottomBarView.getBackground();
        assertEquals(expectedBg, background.getColor());

        // 2. Verify New Tab bright surface tint is blended
        int defaultSurface =
                BottomBarUtils.getColorSurfaceBright(mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoSurface =
                BottomBarUtils.getColorSurfaceBright(mActivity, BrandedColorScheme.INCOGNITO);
        int expectedSurface =
                ColorUtils.blendColorsMultiply(defaultSurface, incognitoSurface, 0.5f);
        assertEquals(expectedSurface, mBottomBarView.getNewTabBackgroundTintForTesting());

        // 3. Verify New Tab background ripple is blended
        int defaultRippleBg =
                BottomBarUtils.getRippleColorBackground(mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoRippleBg =
                BottomBarUtils.getRippleColorBackground(mActivity, BrandedColorScheme.INCOGNITO);
        int expectedRippleBg =
                ColorUtils.blendColorsMultiply(defaultRippleBg, incognitoRippleBg, 0.5f);
        assertEquals(expectedRippleBg, mBottomBarView.getNewTabRippleBackgroundColorForTesting());

        // 4. Verify other ripples (and New Tab no-background ripple) are blended
        int defaultRippleNoBg =
                BottomBarUtils.getRippleColorNoBackground(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoRippleNoBg =
                BottomBarUtils.getRippleColorNoBackground(mActivity, BrandedColorScheme.INCOGNITO);
        int expectedRippleNoBg =
                ColorUtils.blendColorsMultiply(defaultRippleNoBg, incognitoRippleNoBg, 0.5f);
        assertEquals(expectedRippleNoBg, mBottomBarView.getOtherRipplesColorForTesting());

        // 5. Verify icon tint is blended
        int defaultOnSurface =
                IncognitoColors.getColorOnSurface(mActivity, /* isIncognito= */ false);
        int incognitoOnSurface =
                IncognitoColors.getColorOnSurface(mActivity, /* isIncognito= */ true);
        int expectedOnSurface =
                ColorUtils.blendColorsMultiply(defaultOnSurface, incognitoOnSurface, 0.5f);
        ColorStateList expectedIconTint =
                BottomBarUtils.getIconColorStateListFromOnSurface(mActivity, expectedOnSurface);
        ImageView newTabButton =
                mBottomBarView.findViewById(
                        org.chromium.chrome.browser.ui.bottombar.R.id.new_tab_button);
        assertEquals(
                expectedIconTint.getDefaultColor(),
                newTabButton.getImageTintList().getDefaultColor());

        // Verify discrete transition animator creation
        for (HubViewColorBlend blend : blends) {
            Animator anim =
                    blend.createAnimationForTransition(
                            HubColorScheme.DEFAULT, HubColorScheme.INCOGNITO);
            assertNotNull(anim);
        }

        adapter.destroy();
    }

    @Test
    public void testDestroy_resetsToBaselineColorScheme() {
        BottomBarHubColorMixerAdapter adapter =
                new BottomBarHubColorMixerAdapter(mBottomBarView, mHubColorMixer);
        ArgumentCaptor<HubViewColorBlend> captor = ArgumentCaptor.forClass(HubViewColorBlend.class);
        verify(mHubColorMixer, times(5)).registerBlend(captor.capture());
        List<HubViewColorBlend> blends = captor.getAllValues();
        for (HubViewColorBlend blend : blends) {
            blend.updateProgress(HubColorScheme.DEFAULT, HubColorScheme.INCOGNITO, 1.0f);
        }

        // Color was changed to incognito by progress
        int incognitoBg =
                BottomBarUtils.getBottomBarBackgroundColor(mActivity, BrandedColorScheme.INCOGNITO);
        assertEquals(incognitoBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // Calling destroy should unregister blends and reset to baseline APP_DEFAULT
        adapter.destroy();

        verify(mHubColorMixer, times(5)).unregisterBlend(any());
        int defaultBg =
                BottomBarUtils.getBottomBarBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        assertEquals(defaultBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());
    }
}
