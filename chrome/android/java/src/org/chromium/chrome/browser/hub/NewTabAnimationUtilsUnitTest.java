// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;

import android.graphics.Rect;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link NewTabAnimationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NewTabAnimationUtilsUnitTest {
    @Test
    public void testUpdateRects_Rtl() {
        Rect initialRect = new Rect();
        Rect finalRect = new Rect(-200, -100, 150, 200);

        NewTabAnimationUtils.updateRects(initialRect, finalRect, true);

        Rect expectedInitialRect = new Rect(80, -100, 150, -40);
        Rect expectedFinalRect = new Rect(-235, -100, 150, 230);

        assertEquals(expectedInitialRect, initialRect);
        assertEquals(expectedFinalRect, finalRect);
    }

    @Test
    public void testUpdateRects_Ltr() {
        Rect initialRect = new Rect();
        Rect finalRect = new Rect(-200, -100, 150, 200);

        NewTabAnimationUtils.updateRects(initialRect, finalRect, false);

        Rect expectedInitialRect = new Rect(-200, -100, -130, -40);
        Rect expectedFinalRect = new Rect(-200, -100, 185, 230);

        assertEquals(expectedInitialRect, initialRect);
        assertEquals(expectedFinalRect, finalRect);
    }
}
