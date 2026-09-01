// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** Unit tests for {@link ComposeplateUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ComposeplateUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ComposeplateUtils.Natives mMockComposeplateUtilsJni;
    @Mock private Profile mProfile;
    @Mock private View mView;
    @Captor private ArgumentCaptor<Drawable> mDrawableCaptor;

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        ComposeplateUtilsJni.setInstanceForTesting(mMockComposeplateUtilsJni);
        when(mView.getContext()).thenReturn(mContext);
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(true);
    }

    @Test
    public void testIsComposeplateEnabled() {
        testIsComposeplateEnabledImpl();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testIsComposeplateEnabled_LFF() {
        testIsComposeplateEnabledImpl();
    }

    private void testIsComposeplateEnabledImpl() {
        assertTrue(ComposeplateUtils.isComposeplateEnabled(mProfile));

        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(false);
        // Verifies that the composeplate is disabled by policy on all devices.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(mProfile));

        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(true);

        // Verifies that the composeplate is disabled by policy on all devices.
        assertTrue(ComposeplateUtils.isComposeplateEnabled(mProfile));
    }

    @Test
    public void testCanShowComposeplateButtonOnNtp() {
        // Case 1: mobile, feature Composeplate is enabled, and the composeplate button should be
        // shown.
        DeviceInfo.setIsDesktopForTesting(false);
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(true);
        assertFalse(DeviceInfo.isDesktop());
        assertTrue(ComposeplateUtils.canShowComposeplateButtonOnNtp(mProfile));

        // Case 2: mobile, feature Composeplate is disabled, and the composeplate button should not
        // be shown.
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(false);
        assertFalse(ComposeplateUtils.canShowComposeplateButtonOnNtp(mProfile));

        // Case 3: Desktop, feature Composeplate is enabled, but the composeplate button should not
        // be shown.
        DeviceInfo.setIsDesktopForTesting(true);
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(true);
        assertTrue(DeviceInfo.isDesktop());
        assertFalse(ComposeplateUtils.canShowComposeplateButtonOnNtp(mProfile));

        // Case 4: Desktop, feature Composeplate is disabled, and the composeplate button should not
        // be shown.
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(false);
        assertFalse(ComposeplateUtils.canShowComposeplateButtonOnNtp(mProfile));
    }

    @Test
    public void testApplyWhiteBackgroundAndShadow() {
        // Verifies the apply case.
        ComposeplateUtils.applyWhiteBackground(mContext, mView, /* apply= */ true);
        verifyPureWhiteBackgroundApplied();

        clearInvocations(mView);

        // Verifies the reset case.
        ComposeplateUtils.applyWhiteBackground(mContext, mView, /* apply= */ false);
        verifyDrawableApplied(R.drawable.home_surface_search_box_background);
    }

    @Test
    public void testGetSearchBoxTextStyleResId() {
        // Verifies the text style for customized background images.
        assertEquals(
                R.style.TextAppearance_ComposeplateTextMediumDark,
                ComposeplateUtils.getSearchBoxTextStyleResId(
                        /* shouldApplyWhiteBackgroundOnSearchBox= */ true));

        // Verifies the text style for the default theme.
        assertEquals(
                R.style.TextAppearance_ComposeplateTextMedium,
                ComposeplateUtils.getSearchBoxTextStyleResId(
                        /* shouldApplyWhiteBackgroundOnSearchBox= */ false));
    }

    @Test
    public void testGetSearchBoxIconColorTint() {
        // Verifies the color tint for customized background images.
        assertEquals(
                mContext.getColorStateList(R.color.default_icon_color_dark),
                ComposeplateUtils.getSearchBoxIconColorTint(
                        mContext, /* shouldApplyWhiteBackgroundOnSearchBox= */ true));

        // Verifies the color tint for the default theme.
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(mContext, BrandedColorScheme.APP_DEFAULT),
                ComposeplateUtils.getSearchBoxIconColorTint(
                        mContext, /* shouldApplyWhiteBackgroundOnSearchBox= */ false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NTP_AURORA)
    public void testApplySearchBoxBackground_auroraDisabled() {
        // When Aurora is disabled on customized image theme, apply pure white.
        ComposeplateUtils.applySearchBoxBackground(
                mContext, mView, /* applyWhiteBackground= */ true);
        verifyPureWhiteBackgroundApplied();

        clearInvocations(mView);

        // When Aurora is disabled on default theme, reset to default background.
        ComposeplateUtils.applySearchBoxBackground(
                mContext, mView, /* applyWhiteBackground= */ false);
        verifyDrawableApplied(R.drawable.home_surface_search_box_background);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NTP_AURORA)
    public void testApplySearchBoxBackground_auroraEnabled() {
        // When Aurora is enabled on customized image theme, apply white mixed with 2% primary.
        ComposeplateUtils.applySearchBoxBackground(
                mContext, mView, /* applyWhiteBackground= */ true);
        verifyDrawableApplied(R.drawable.fake_search_box_white_with_primary_color_alpha_2);

        clearInvocations(mView);

        // When Aurora is enabled on default theme, apply theme base mixed with 2% primary.
        ComposeplateUtils.applySearchBoxBackground(
                mContext, mView, /* applyWhiteBackground= */ false);
        verifyDrawableApplied(R.drawable.fake_search_box_background);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NTP_AURORA)
    public void testApplyComposeplateBackground_buttonColorDisabled_auroraDisabled() {
        // When Aurora is disabled on customized image theme, apply pure white.
        ComposeplateUtils.applyComposeplateBackground(
                mContext, mView, /* applyWhiteBackground= */ true);
        verifyPureWhiteBackgroundApplied();

        clearInvocations(mView);

        // When Aurora is disabled on default theme, reset to default background.
        ComposeplateUtils.applyComposeplateBackground(
                mContext, mView, /* applyWhiteBackground= */ false);
        verifyDrawableApplied(R.drawable.home_surface_search_box_background);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NTP_AURORA)
    public void testApplyComposeplateBackground_buttonColorDisabled_auroraEnabled() {
        // When Aurora is enabled but button color is disabled on customized image theme, apply pure
        // white.
        assertFalse(NewTabPageUtils.isNtpAuroraButtonColorEnabled());
        ComposeplateUtils.applyComposeplateBackground(
                mContext, mView, /* applyWhiteBackground= */ true);
        verifyPureWhiteBackgroundApplied();

        clearInvocations(mView);

        // When Aurora is enabled but button color is disabled on default theme, reset to default
        // background.
        ComposeplateUtils.applyComposeplateBackground(
                mContext, mView, /* applyWhiteBackground= */ false);
        verifyDrawableApplied(R.drawable.home_surface_search_box_background);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA + ":change_button_color/true"
    })
    public void testApplyComposeplateBackground_buttonColorEnabled() {
        // When Aurora button color is enabled on customized image theme, apply white mixed with 2%
        // primary.
        ComposeplateUtils.applyComposeplateBackground(
                mContext, mView, /* applyWhiteBackground= */ true);
        verifyDrawableApplied(R.drawable.fake_search_box_white_with_primary_color_alpha_2);

        clearInvocations(mView);

        // When Aurora button color is enabled on default theme, apply 8% primary background.
        ComposeplateUtils.applyComposeplateBackground(
                mContext, mView, /* applyWhiteBackground= */ false);
        verifyDrawableApplied(R.drawable.composeplate_button_background);
    }

    private void verifyDrawableApplied(@DrawableRes int expectedDrawableRes) {
        verify(mView).setBackground(mDrawableCaptor.capture());
        Drawable captured = mDrawableCaptor.getValue();
        Drawable expected = mContext.getDrawable(expectedDrawableRes);

        if (expected instanceof LayerDrawable) {
            assertTrue(captured instanceof LayerDrawable);
            LayerDrawable expectedLayer = (LayerDrawable) expected;
            LayerDrawable capturedLayer = (LayerDrawable) captured;
            assertEquals(expectedLayer.getNumberOfLayers(), capturedLayer.getNumberOfLayers());
            for (int i = 0; i < expectedLayer.getNumberOfLayers(); i++) {
                assertTrue(capturedLayer.getDrawable(i) instanceof GradientDrawable);
                assertEquals(
                        ((GradientDrawable) expectedLayer.getDrawable(i))
                                .getColor()
                                .getDefaultColor(),
                        ((GradientDrawable) capturedLayer.getDrawable(i))
                                .getColor()
                                .getDefaultColor());
            }
        } else if (expected instanceof GradientDrawable) {
            assertTrue(captured instanceof GradientDrawable);
            assertEquals(
                    ((GradientDrawable) expected).getColor().getDefaultColor(),
                    ((GradientDrawable) captured).getColor().getDefaultColor());
        }
    }

    private void verifyPureWhiteBackgroundApplied() {
        verify(mView).setBackground(mDrawableCaptor.capture());
        assertTrue(mDrawableCaptor.getValue() instanceof GradientDrawable);
        assertEquals(
                Color.WHITE,
                ((GradientDrawable) mDrawableCaptor.getValue()).getColor().getDefaultColor());
    }
}
