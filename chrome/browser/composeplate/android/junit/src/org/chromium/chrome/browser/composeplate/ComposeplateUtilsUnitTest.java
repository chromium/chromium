// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** Unit tests for {@link ComposeplateUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ComposeplateUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ComposeplateUtils.Natives mMockComposeplateUtilsJni;
    @Mock private Profile mProfile;
    @Mock private View mView;
    @Captor private ArgumentCaptor<GradientDrawable> mBackgroundDrawableCaptor;

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
    public void testApplyWhiteBackgroundAndShadow() {
        // Verifies the apply case.
        ComposeplateUtils.applyWhiteBackground(mContext, mView, /* apply= */ true);
        verify(mView).setBackground(mBackgroundDrawableCaptor.capture());
        assertEquals(
                Color.WHITE, mBackgroundDrawableCaptor.getValue().getColor().getDefaultColor());

        clearInvocations(mView);

        // Verifies the reset case.
        ComposeplateUtils.applyWhiteBackground(mContext, mView, /* apply= */ false);
        verify(mView).setBackground(any(Drawable.class));
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
}
