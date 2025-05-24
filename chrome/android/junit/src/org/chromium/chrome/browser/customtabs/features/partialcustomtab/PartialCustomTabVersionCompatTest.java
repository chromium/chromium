// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Build;
import android.view.Display;
import android.view.DisplayCutout;
import android.view.Surface;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link PartialCustomTabVersionCompat}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
public class PartialCustomTabVersionCompatTest {
    @Rule public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    @Config(sdk = Build.VERSION_CODES.Q)
    @Test
    public void getXOffset_cutoutWidthIsExcluded() {
        final int cutoutWidth = 200;
        final int navbarWidth = 150;

        Activity activity = mPCCTTestRule.mActivity;
        var vc = PartialCustomTabVersionCompat.create(activity, CallbackUtils.emptyRunnable());

        final int displayWidth = DEVICE_WIDTH - cutoutWidth - navbarWidth;
        mPCCTTestRule.mDisplaySize = new Point(displayWidth, DEVICE_HEIGHT);

        // Set a non-zero cutout right inset.
        Display display = mPCCTTestRule.mDisplay;
        Rect insets = new Rect(0, 0, 200, 0);
        var cutout = new DisplayCutout(insets, null);
        when(display.getCutout()).thenReturn(cutout);

        // Reverse lanscape mode. Navbar on the left, and the cutout on the right.
        // Cutout on the right doesn't count when calculating the x offset.
        when(display.getRotation()).thenReturn(Surface.ROTATION_270);
        assertEquals(navbarWidth, vc.getXOffset());

        // Landscape mode. Cutout on the left, and navbar on the right.
        // Verify that the cutout width is again excluded from the offset, since the content
        // area doesn't include it.
        when(display.getRotation()).thenReturn(Surface.ROTATION_90);
        assertEquals(0, vc.getXOffset());
    }
}
