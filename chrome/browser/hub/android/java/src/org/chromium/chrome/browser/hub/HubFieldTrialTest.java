// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link HubFieldTrial}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubFieldTrialTest {
    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON})
    public void testUsesFloatActionButtonEnabled() {
        assertTrue(HubFieldTrial.usesFloatActionButton());
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON})
    public void testUsesFloatActionButtonDisabled() {
        assertFalse(HubFieldTrial.usesFloatActionButton());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON})
    public void testUseAlternativeFabColor() {
        assertFalse(HubFieldTrial.useAlternativeFabColor());
        HubFieldTrial.ALTERNATIVE_FAB_COLOR.setForTesting(true);
        assertTrue(HubFieldTrial.useAlternativeFabColor());
    }
}
