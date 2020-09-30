// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Canvas;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Instrumentation tests for {@link ToolbarPhone}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class ToolbarPhoneTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private MenuButtonCoordinator mMenuButtonCoordinator;

    private Canvas mCanvas = new Canvas();
    private ToolbarPhone mToolbar;
    private MenuButton mMenuButton;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivityTestRule.startMainActivityOnBlankPage();
        mToolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        mMenuButton = Mockito.spy(mToolbar.findViewById(R.id.menu_button_wrapper));
        mToolbar.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);
        doReturn(mMenuButton).when(mMenuButtonCoordinator).getMenuButton();
    }

    @Test
    @MediumTest
    public void testDrawTabSwitcherAnimation_menuButtonDrawn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.drawTabSwitcherAnimationOverlay(mCanvas, 0);
            verify(mMenuButton).drawTabSwitcherAnimationOverlay(mCanvas, 255);

            mToolbar.setTextureCaptureMode(true);
            mToolbar.draw(mCanvas);
            verify(mMenuButton, times(2)).drawTabSwitcherAnimationOverlay(mCanvas, 255);
            mToolbar.setTextureCaptureMode(false);
        });
    }
}
