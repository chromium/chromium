// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.firstrun.FirstRunUtils.SafetyFrePromoArm;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link FirstRunUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FirstRunUtilsUnitTest {
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
    @EnableFeatures(ChromeFeatureList.SAFETY_FRE_PROMO)
    public void testShouldShowSafetyFrePromoCarousel() {
        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.PASSWORD_MANAGER);
        assertTrue(FirstRunUtils.shouldShowSafetyFrePromoCarousel());

        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.ANIMATED_ILLUSTRATION);
        assertFalse(FirstRunUtils.shouldShowSafetyFrePromoCarousel());

        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.UNDEFINED);
        assertFalse(FirstRunUtils.shouldShowSafetyFrePromoCarousel());
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
    public void testGetItemsForSafetyFrePromoArm() {
        assertEquals(
                FirstRunUtils.ARM_1_ITEMS,
                FirstRunUtils.getItemsForSafetyFrePromoArm(SafetyFrePromoArm.PASSWORD_MANAGER));
        assertEquals(
                FirstRunUtils.ARM_2_ITEMS,
                FirstRunUtils.getItemsForSafetyFrePromoArm(SafetyFrePromoArm.HISTORY_QUICK_DELETE));
        assertEquals(
                FirstRunUtils.ARM_3_ITEMS,
                FirstRunUtils.getItemsForSafetyFrePromoArm(
                        SafetyFrePromoArm.PASSWORD_MANAGER_AND_HISTORY_QUICK_DELETE));
        assertTrue(
                FirstRunUtils.getItemsForSafetyFrePromoArm(SafetyFrePromoArm.UNDEFINED).isEmpty());
        assertTrue(
                FirstRunUtils.getItemsForSafetyFrePromoArm(SafetyFrePromoArm.ANIMATED_ILLUSTRATION)
                        .isEmpty());
    }
}
