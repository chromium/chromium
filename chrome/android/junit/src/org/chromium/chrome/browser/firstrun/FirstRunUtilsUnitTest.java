// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.firstrun.FirstRunUtils.SafetyFrePromoArm;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link FirstRunUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class FirstRunUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @DisableFeatures(ChromeFeatureList.SAFETY_FRE_PROMO)
    public void testShouldShowSafetyFrePromo_FeatureDisabled() {
        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.PASSWORD_MANAGER);
        assertFalse(FirstRunUtils.shouldShowSafetyFrePromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SAFETY_FRE_PROMO)
    public void testShouldShowSafetyFrePromo_FeatureEnabled_ArmUndefined() {
        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.UNDEFINED);
        assertFalse(FirstRunUtils.shouldShowSafetyFrePromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SAFETY_FRE_PROMO)
    public void testShouldShowSafetyFrePromo_FeatureEnabled_ValidArm() {
        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.PASSWORD_MANAGER);
        assertTrue(FirstRunUtils.shouldShowSafetyFrePromo());

        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.HISTORY_QUICK_DELETE);
        assertTrue(FirstRunUtils.shouldShowSafetyFrePromo());

        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(
                SafetyFrePromoArm.PASSWORD_MANAGER_AND_HISTORY_QUICK_DELETE);
        assertTrue(FirstRunUtils.shouldShowSafetyFrePromo());

        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.ANIMATED_ILLUSTRATION);
        assertTrue(FirstRunUtils.shouldShowSafetyFrePromo());
    }

    @Test
    public void testIsCardBasedPromoArm() {
        assertFalse(FirstRunUtils.isCardBasedPromoArm(SafetyFrePromoArm.UNDEFINED));
        assertFalse(FirstRunUtils.isCardBasedPromoArm(SafetyFrePromoArm.ANIMATED_ILLUSTRATION));

        assertTrue(FirstRunUtils.isCardBasedPromoArm(SafetyFrePromoArm.PASSWORD_MANAGER));
        assertTrue(FirstRunUtils.isCardBasedPromoArm(SafetyFrePromoArm.HISTORY_QUICK_DELETE));
        assertTrue(
                FirstRunUtils.isCardBasedPromoArm(
                        SafetyFrePromoArm.PASSWORD_MANAGER_AND_HISTORY_QUICK_DELETE));
    }

    @Test
    public void testGetCardsForSafetyFrePromoArm() {
        assertEquals(
                FirstRunUtils.ARM_1_CARDS,
                FirstRunUtils.getCardsForSafetyFrePromoArm(SafetyFrePromoArm.PASSWORD_MANAGER));
        assertEquals(
                FirstRunUtils.ARM_2_CARDS,
                FirstRunUtils.getCardsForSafetyFrePromoArm(SafetyFrePromoArm.HISTORY_QUICK_DELETE));
        assertEquals(
                FirstRunUtils.ARM_3_CARDS,
                FirstRunUtils.getCardsForSafetyFrePromoArm(
                        SafetyFrePromoArm.PASSWORD_MANAGER_AND_HISTORY_QUICK_DELETE));
        assertTrue(
                FirstRunUtils.getCardsForSafetyFrePromoArm(SafetyFrePromoArm.UNDEFINED).isEmpty());
        assertTrue(
                FirstRunUtils.getCardsForSafetyFrePromoArm(SafetyFrePromoArm.ANIMATED_ILLUSTRATION)
                        .isEmpty());
    }
}
