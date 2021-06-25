// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link MultiWindowUtilsUnit}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiWindowUtilsUnitTest {
    private MultiWindowUtils mUtils;
    private boolean mIsInMultiWindowMode;
    private boolean mIsInMultiDisplayMode;
    private boolean mIsMultipleInstanceRunning;
    private boolean mIsAutosplitSupported;
    private boolean mCustomMultiWindowSupported;

    @Before
    public void setUp() {
        mUtils = new MultiWindowUtils() {
            @Override
            public boolean isInMultiWindowMode(Activity activity) {
                return mIsInMultiWindowMode;
            }

            @Override
            public boolean isInMultiDisplayMode(Activity activity) {
                return mIsInMultiDisplayMode;
            }

            @Override
            public boolean areMultipleChromeInstancesRunning(Context context) {
                return mIsMultipleInstanceRunning;
            }

            @Override
            public boolean aospMultiWindowModeSupported() {
                return mIsAutosplitSupported;
            }

            @Override
            public boolean customMultiWindowModeSupported() {
                return mCustomMultiWindowSupported;
            }

            @Override
            public Class<? extends Activity> getOpenInOtherWindowActivity(Activity current) {
                return Activity.class;
            }
        };
    }

    @Test
    public void testCanEnterMultiWindowMode() {
        // Chrome can enter multi-window mode through menu on the platform that supports it
        // (Android S or certain vendor-customized platform).
        for (int i = 0; i < 32; ++i) {
            mIsInMultiWindowMode = ((i >> 0) & 1) == 1;
            mIsInMultiDisplayMode = ((i >> 1) & 1) == 1;
            mIsMultipleInstanceRunning = ((i >> 2) & 1) == 1;
            mIsAutosplitSupported = ((i >> 3) & 1) == 1;
            mCustomMultiWindowSupported = ((i >> 4) & 1) == 1;

            boolean canEnter = mIsAutosplitSupported || mCustomMultiWindowSupported;
            assertEquals(
                    " api-s: " + mIsAutosplitSupported + " vendor: " + mCustomMultiWindowSupported,
                    canEnter, mUtils.canEnterMultiWindowMode(null));
        }
    }

    @Test
    public void testIsOpenInOtherWindowEnabled() {
        for (int i = 0; i < 32; ++i) {
            mIsInMultiWindowMode = ((i >> 0) & 1) == 1;
            mIsInMultiDisplayMode = ((i >> 1) & 1) == 1;
            mIsMultipleInstanceRunning = ((i >> 2) & 1) == 1;
            mIsAutosplitSupported = ((i >> 3) & 1) == 1;
            mCustomMultiWindowSupported = ((i >> 4) & 1) == 1;

            // 'openInOtherWindow' is supported if we are already in multi-window/display mode.
            boolean openInOtherWindow = (mIsInMultiWindowMode || mIsInMultiDisplayMode);
            assertEquals("multi-window: " + mIsInMultiWindowMode
                            + " multi-display: " + mIsInMultiDisplayMode
                            + " multi-instance: " + mIsMultipleInstanceRunning,
                    openInOtherWindow, mUtils.isOpenInOtherWindowSupported(null));
        }
    }
}
