// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertEquals;

import android.view.View;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/** Junit tests for class {@link NavbarVisibilityDelegate} */
@RunWith(BlockJUnit4ClassRunner.class)
public class NavbarVisibilityDelegateTest {
    // For totalSteps = 3 the currentStepIdx represents the following:
    // 0. Welcome card
    // 1. Middle card, neither Welcome nor Done
    // 2. Done card
    @Test
    public void testNavbarVisibility_Idx0_Count3() {
        int totalSteps = 3;
        int currentStepIdx = 0;

        NavbarVisibilityDelegate delegate = new NavbarVisibilityDelegate(totalSteps);
        assertEquals(View.VISIBLE, delegate.getStartButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getBackButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getNextButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getFinishButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getDoneButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getProgressIndicatorVisibility(currentStepIdx));
    }

    @Test
    public void testNavbarVisibility_Idx1_Count3() {
        int totalSteps = 3;
        int currentStepIdx = 1;

        NavbarVisibilityDelegate delegate = new NavbarVisibilityDelegate(totalSteps);
        assertEquals(View.GONE, delegate.getStartButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getBackButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getNextButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getFinishButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getDoneButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getProgressIndicatorVisibility(currentStepIdx));
    }

    @Test
    public void testNavbarVisibility_Idx2_Count3() {
        int totalSteps = 3;
        int currentStepIdx = 2;

        NavbarVisibilityDelegate delegate = new NavbarVisibilityDelegate(totalSteps);
        assertEquals(View.GONE, delegate.getStartButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getBackButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getNextButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getFinishButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getDoneButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getProgressIndicatorVisibility(currentStepIdx));
    }

    // For totalSteps = 5 the currentStepIdx represents the following:
    // 0. Welcome card
    // 1. Card after Welcome card
    // 2. Neither of the cards above or below
    // 3. Card before Done card
    // 4. Done card
    @Test
    public void testNavbarVisibility_Idx0_Count5() {
        int totalSteps = 5;
        int currentStepIdx = 0;

        NavbarVisibilityDelegate delegate = new NavbarVisibilityDelegate(totalSteps);
        assertEquals(View.VISIBLE, delegate.getStartButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getBackButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getNextButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getFinishButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getDoneButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getProgressIndicatorVisibility(currentStepIdx));
    }

    @Test
    public void testNavbarVisibility_Idx1_Count5() {
        int totalSteps = 5;
        int currentStepIdx = 1;

        NavbarVisibilityDelegate delegate = new NavbarVisibilityDelegate(totalSteps);
        assertEquals(View.GONE, delegate.getStartButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getBackButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getNextButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getFinishButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getDoneButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getProgressIndicatorVisibility(currentStepIdx));
    }

    @Test
    public void testNavbarVisibility_Idx2_Count5() {
        int totalSteps = 5;
        int currentStepIdx = 2;

        NavbarVisibilityDelegate delegate = new NavbarVisibilityDelegate(totalSteps);
        assertEquals(View.GONE, delegate.getStartButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getBackButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getNextButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getFinishButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getDoneButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getProgressIndicatorVisibility(currentStepIdx));
    }

    @Test
    public void testNavbarVisibility_Idx3_Count5() {
        int totalSteps = 5;
        int currentStepIdx = 3;

        NavbarVisibilityDelegate delegate = new NavbarVisibilityDelegate(totalSteps);
        assertEquals(View.GONE, delegate.getStartButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getBackButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getNextButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getFinishButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getDoneButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getProgressIndicatorVisibility(currentStepIdx));
    }

    @Test
    public void testNavbarVisibility_Idx4_Count5() {
        int totalSteps = 5;
        int currentStepIdx = 4;

        NavbarVisibilityDelegate delegate = new NavbarVisibilityDelegate(totalSteps);
        assertEquals(View.GONE, delegate.getStartButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getBackButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getNextButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getFinishButtonVisibility(currentStepIdx));
        assertEquals(View.VISIBLE, delegate.getDoneButtonVisibility(currentStepIdx));
        assertEquals(View.GONE, delegate.getProgressIndicatorVisibility(currentStepIdx));
    }
}
