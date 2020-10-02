// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Canvas;
import android.view.View;

import androidx.test.espresso.matcher.ViewMatchers.Visibility;
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
    private View mToolbarButtonsContainer;
    private MenuButton mMenuButton;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivityTestRule.startMainActivityOnBlankPage();
        mToolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        mToolbarButtonsContainer = mToolbar.findViewById(R.id.toolbar_buttons);
    }

    @Test
    @MediumTest
    public void testDrawTabSwitcherAnimation_menuButtonDrawn() {
        mMenuButton = Mockito.spy(mToolbar.findViewById(R.id.menu_button_wrapper));
        mToolbar.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);
        doReturn(mMenuButton).when(mMenuButtonCoordinator).getMenuButton();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.drawTabSwitcherAnimationOverlay(mCanvas, 0);
            verify(mMenuButtonCoordinator)
                    .drawTabSwitcherAnimationOverlay(mToolbarButtonsContainer, mCanvas, 255);

            mToolbar.setTextureCaptureMode(true);
            mToolbar.draw(mCanvas);
            verify(mMenuButtonCoordinator, times(2))
                    .drawTabSwitcherAnimationOverlay(mToolbarButtonsContainer, mCanvas, 255);
            mToolbar.setTextureCaptureMode(false);
        });
    }

    @Test
    @MediumTest
    public void testFocusAnimation_menuButtonHidesAndShows() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mToolbar.onUrlFocusChange(true); });
        onView(allOf(withId(R.id.menu_button_wrapper), withEffectiveVisibility(Visibility.GONE)));

        TestThreadUtils.runOnUiThreadBlocking(() -> { mToolbar.onUrlFocusChange(false); });
        onView(allOf(
                withId(R.id.menu_button_wrapper), withEffectiveVisibility(Visibility.VISIBLE)));
    }
}
