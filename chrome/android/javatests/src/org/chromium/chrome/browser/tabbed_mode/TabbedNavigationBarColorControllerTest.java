// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.graphics.Color;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.Window;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Tests for the TabbedNavigationBarColorController.  */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.O_MR1)
@TargetApi(Build.VERSION_CODES.O_MR1)
@SuppressLint("NewApi")
public class TabbedNavigationBarColorControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Window mWindow;
    private int mLightNavigationColor;
    private int mDarkNavigationColor;
    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mWindow = mActivityTestRule.getActivity().getWindow();
        mLightNavigationColor = ApiCompatibilityUtils.getColor(
                mActivityTestRule.getActivity().getResources(), R.color.bottom_system_nav_color);
        mDarkNavigationColor = Color.BLACK;
    }

    @Test
    @SmallTest
    public void testToggleOverview() {
        assertEquals("Navigation bar should be white before entering overview mode.",
                mLightNavigationColor, mWindow.getNavigationBarColor());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));

        assertEquals("Navigation bar should be white in overview mode.", mLightNavigationColor,
                mWindow.getNavigationBarColor());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));

        assertEquals("Navigation bar should be white after exiting overview mode.",
                mLightNavigationColor, mWindow.getNavigationBarColor());
    }

    @Test
    @SmallTest
    public void testToggleIncognito() {
        assertEquals("Navigation bar should be white on normal tabs.", mLightNavigationColor,
                mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), true, true);

        assertEquals("Navigation bar should be black on incognito tabs.", mDarkNavigationColor,
                mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), false, true);

        assertEquals("Navigation bar should be white after switching back to normal tab.",
                mLightNavigationColor, mWindow.getNavigationBarColor());
    }
}
