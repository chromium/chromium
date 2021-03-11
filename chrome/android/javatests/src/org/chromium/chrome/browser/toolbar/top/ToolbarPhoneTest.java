// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
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

    @Mock
    private MenuButtonCoordinator.SetFocusFunction mFocusFunction;
    @Mock
    private Runnable mRequestRenderRunnable;
    @Mock
    ThemeColorProvider mThemeColorProvider;

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

    @Test
    @MediumTest
    public void testOptionalButtonPadding_paddingUpdatesWithMenuVisibility() {
        mToolbar.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);
        Drawable drawable = AppCompatResources.getDrawable(
                mActivityTestRule.getActivity(), R.drawable.ic_toolbar_share_offset_24dp);

        // When menu is hidden, optional button should have no padding.
        doReturn(false).when(mMenuButtonCoordinator).isVisible();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.updateOptionalButton(
                    new ButtonDataImpl(false, drawable, null, R.string.share, false, null, false));
            mToolbar.updateButtonVisibility();
        });

        int padding = mToolbar.findViewById(R.id.optional_toolbar_button).getPaddingStart();
        assertEquals("Optional button's padding should be 0 when menu button is not visible", 0,
                padding);

        // However when menu is visible, optional button should have
        // toolbar_phone_optional_button_padding padding.
        doReturn(true).when(mMenuButtonCoordinator).isVisible();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mToolbar.updateButtonVisibility(); });
        padding = mToolbar.findViewById(R.id.optional_toolbar_button).getPaddingStart();
        int expectedPadding = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.toolbar_phone_optional_button_padding);
        assertEquals(
                "Optional button should have a 12dp start padding set when menu button is visible",
                expectedPadding, padding);
    }

    @Test
    @MediumTest
    public void testLocationBarLengthWithOptionalButton() {
        // The purpose of this test is to document the expected behavior for setting
        // paddings and sizes of toolbar elements based on the visibility of the menu button.
        // This test fails if View#isShown() is used to determine visibility.
        // See https://crbug.com/1176992 for an example when it caused an issue.
        Drawable drawable = AppCompatResources.getDrawable(
                mActivityTestRule.getActivity(), R.drawable.ic_toolbar_share_offset_24dp);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Has to be created on the main thread.
            MenuButtonCoordinator realMenuButtonCoordinator = new MenuButtonCoordinator(
                    new OneshotSupplierImpl<AppMenuCoordinator>(),
                    new TestControlsVisibilityDelegate(),
                    mActivityTestRule.getActivity().getWindowAndroid(), mFocusFunction,
                    mRequestRenderRunnable, true,
                    () -> false, mThemeColorProvider, org.chromium.chrome.R.id.menu_button_wrapper);
            mToolbar.setMenuButtonCoordinatorForTesting(realMenuButtonCoordinator);
            mToolbar.updateOptionalButton(
                    new ButtonDataImpl(false, drawable, null, R.string.share, false, null, false));
            // Make sure the button is visible in the beginning of the test.
            assertEquals(realMenuButtonCoordinator.isVisible(), true);

            // Make the ancestors of the menu button invisible.
            mToolbarButtonsContainer.setVisibility(View.INVISIBLE);

            // Ancestor's invisibility doesn't affect menu button's visibility.
            assertEquals("Menu button should be visible even if its parents are not",
                    realMenuButtonCoordinator.isVisible(), true);
            float offsetWhenParentInvisible = mToolbar.getLocationBarWidthOffsetForOptionalButton();

            // Make menu's ancestors visible.
            mToolbarButtonsContainer.setVisibility(View.VISIBLE);
            assertEquals(realMenuButtonCoordinator.isVisible(), true);
            float offsetWhenParentVisible = mToolbar.getLocationBarWidthOffsetForOptionalButton();

            assertEquals("Offset should be the same even if menu button's parents are invisible "
                            + "if it is visible",
                    offsetWhenParentInvisible, offsetWhenParentVisible, 0);

            // Sanity check that the offset is different when menu button is invisible
            realMenuButtonCoordinator.getMenuButton().setVisibility(View.INVISIBLE);
            assertEquals(realMenuButtonCoordinator.isVisible(), false);
            float offsetWhenButtonInvisible = mToolbar.getLocationBarWidthOffsetForOptionalButton();
            Assert.assertNotEquals("Offset should be different when menu button is invisible",
                    offsetWhenButtonInvisible, offsetWhenParentVisible);
        });
    }

    private static class TestControlsVisibilityDelegate
            extends BrowserStateBrowserControlsVisibilityDelegate {
        public TestControlsVisibilityDelegate() {
            super(new ObservableSupplier<Boolean>() {
                @Override
                public Boolean addObserver(Callback<Boolean> obs) {
                    return false;
                }

                @Override
                public void removeObserver(Callback<Boolean> obs) {}

                @Override
                public Boolean get() {
                    return false;
                }
            });
        }
    }
}
