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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link ComposeplateUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({
    ChromeFeatureList.ANDROID_COMPOSEPLATE,
})
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
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(true);
        when(mMockComposeplateUtilsJni.isAimEntrypointLFFEligible(eq(mProfile))).thenReturn(true);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_COMPOSEPLATE_LFF})
    public void testIsComposeplateEnabled_LFFFlagDisabled() {
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));

        // Verifies that the function returns false on tablets.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true, mProfile));
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_COMPOSEPLATE_LFF})
    public void testIsComposeplateEnabled_DisabledByServerEligibility() {
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));

        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(false);
        // Verifies that the composeplate is disabled by policy.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_COMPOSEPLATE_LFF})
    public void testIsComposeplateEnabled_LFF() {
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true, mProfile));

        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(false);
        // Verifies that the composeplate is disabled by policy on all devices.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true, mProfile));

        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(true);
        when(mMockComposeplateUtilsJni.isAimEntrypointLFFEligible(eq(mProfile))).thenReturn(false);

        // Verifies that the composeplate is disabled by policy on tablets.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true, mProfile));
        // Verifies that the composeplate is still enabled by policy on phones.
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));
    }

    @Test
    public void testApplyWhiteBackgroundAndShadow() {
        float elevation =
                mContext.getResources().getDimensionPixelSize(R.dimen.ntp_search_box_elevation);

        // Verifies the apply case.
        ComposeplateUtils.applyWhiteBackgroundAndShadow(mContext, mView, /* apply= */ true);
        verify(mView).setClipToOutline(eq(true));
        verify(mView).setBackground(mBackgroundDrawableCaptor.capture());
        assertEquals(
                Color.WHITE, mBackgroundDrawableCaptor.getValue().getColor().getDefaultColor());
        verify(mView).setElevation(eq(elevation));

        clearInvocations(mView);

        // Verifies the reset case.
        ComposeplateUtils.applyWhiteBackgroundAndShadow(mContext, mView, /* apply= */ false);
        verify(mView).setClipToOutline(eq(false));
        verify(mView).setBackground(any(Drawable.class));
        verify(mView).setElevation(eq(0f));
    }
}
