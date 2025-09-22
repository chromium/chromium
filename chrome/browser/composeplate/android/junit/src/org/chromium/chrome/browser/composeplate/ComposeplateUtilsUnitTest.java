// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
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

    @Before
    public void setUp() {
        ComposeplateUtilsJni.setInstanceForTesting(mMockComposeplateUtilsJni);
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(true);
    }

    @Test
    public void testIsComposeplateEnabled() {
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));

        // Verifies that the function returns false on tablets.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true, mProfile));
    }

    @Test
    public void testIsComposeplateEnabled_DisabledByServerEligibility() {
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));

        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(false);
        // Verifies that the composeplate is disabled by policy.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));
    }
}
