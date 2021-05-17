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
    private boolean mIsApi31;
    private boolean mCustomMultiWindowSupported;
    private boolean mIsTabletScreen;

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
            public boolean isBuildAtLeastS() {
                return mIsApi31;
            }

            @Override
            public boolean customMultiWindowModeSupported() {
                return mCustomMultiWindowSupported;
            }

            @Override
            public boolean isTabletScreen(Context context) {
                return mIsTabletScreen;
            }

            @Override
            public Class<? extends Activity> getOpenInOtherWindowActivity(Activity current) {
                return Activity.class;
            }
        };
    }

    @Test
    public void testCanEnterMultiWindowMode() {
        // Chrome can enter multi-window mode through menu when it is not already in it
        // on the platform that supports it (Android S or certain vendor-customized platform).
        // Cannot enter it if there are already multiple instances running in multiple windows.
        boolean[][] flags = {
                // multi-instances, multi-window, Android S, Vendor, result
                {false, false, false, false, false},
                {false, false, false, true, true},
                {false, false, true, false, true},
                {false, false, true, true, true},
                {false, true, false, false, false},
                {false, true, false, true, true},
                {false, true, true, false, true},
                {false, true, true, true, true},
                {true, false, false, false, false},
                {true, false, false, true, false},
                {true, false, true, false, false},
                {true, false, true, true, false},
                {true, true, false, false, false},
                {true, true, false, true, false},
                {true, true, true, false, false},
                {true, true, true, true, false},
        };

        mIsTabletScreen = true;
        for (int i = 0; i < flags.length; ++i) {
            mIsMultipleInstanceRunning = flags[i][0];
            mIsInMultiWindowMode = flags[i][1];
            mIsApi31 = flags[i][2];
            mCustomMultiWindowSupported = flags[i][3];

            boolean canEnter = flags[i][4];
            assertEquals("multi-instance: " + mIsMultipleInstanceRunning
                            + " multi-window: " + mIsInMultiWindowMode + " api-s: " + mIsApi31
                            + " vendor: " + mCustomMultiWindowSupported,
                    canEnter, mUtils.canEnterMultiWindowMode(null));
        }
    }

    @Test
    public void testIsOpenInOtherWindowSupported() {
        for (int i = 0; i < 16; ++i) {
            // Check all the combinations.
            mIsInMultiWindowMode = ((i >> 0) & 1) == 1;
            mIsInMultiDisplayMode = ((i >> 1) & 1) == 1;
            mIsMultipleInstanceRunning = ((i >> 2) & 1) == 1;
            mIsTabletScreen = ((i >> 3) & 1) == 1;

            // 'OpenInOtherWindow' is supported as long as the platform supports it
            // and we have another instance to move the window to.
            // Multi-display device always can open in other window as well.
            boolean openInOtherWindow = (mIsInMultiWindowMode || mIsInMultiDisplayMode)
                    && (!mIsTabletScreen || mIsMultipleInstanceRunning);
            assertEquals("multi-window: " + mIsInMultiWindowMode
                            + " multi-display: " + mIsInMultiDisplayMode + " multi-instance: "
                            + mIsMultipleInstanceRunning + " tablet-screen: " + mIsTabletScreen,
                    openInOtherWindow, mUtils.isOpenInOtherWindowSupported(null));
        }
    }
}
