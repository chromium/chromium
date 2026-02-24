// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

/** Instrumentation tests for the tab strip empty space long press context menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@EnableFeatures(ChromeFeatureList.TAB_STRIP_EMPTY_SPACE_CONTEXT_MENU_ANDROID)
public class TabStripContextMenuTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private StripLayoutHelper mStripLayoutHelper;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
    }

    @Test
    @LargeTest
    public void testBookmarkAllTabs_ShownWithMultipleTabs() {
        // 1. Create 2 tabs to ensure we have multiple tabs (count > 1).
        mPage.openNewTabFast().loadAboutBlank();

        // 2. Perform a long press on the empty space of the tab strip.
        longPressOnEmptySpace();

        // 3. Verify the "Bookmark all tabs" menu item is displayed.
        onView(withText(R.string.menu_bookmark_all_tabs)).check(matches(isDisplayed()));

        // 4. Click the menu item to ensure it's actionable and closes the menu.
        onView(withText(R.string.menu_bookmark_all_tabs)).perform(click());
        onView(withText(R.string.menu_bookmark_all_tabs)).check(doesNotExist());

        // 5. Verify the "Bookmarked" snackbar shows
        // TODO(crbug.com/483119758): Add check to verify bookmark action occurred.
    }

    @Test
    @LargeTest
    public void testBookmarkAllTabs_NotShownWithSingleTab() {
        // 1. Start with 1 tab.
        // Perform a long press on the empty space of the tab strip.
        longPressOnEmptySpace();

        // 2. Verify the "Bookmark all tabs" menu item is NOT displayed.
        onView(withText(R.string.menu_bookmark_all_tabs)).check(doesNotExist());

        // 3. Close menu
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mStripLayoutHelper.dismissContextMenu();
                        });
    }

    private void longPressOnEmptySpace() {
        // Calculate a position on the empty space of the tab strip (right of the last tab).
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsToRender();
        StripLayoutTab lastTab = tabs[tabs.length - 1];
        // Click 50dp to the right of the last tab to ensure we hit empty space.
        StripLayoutHelperManager manager =
                mActivityTestRule.getActivity().getLayoutManager().getStripLayoutHelperManager();
        float x = lastTab.getDrawX() + lastTab.getWidth() + 50f;
        float y = manager.getHeight() / 2;

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            manager.simulateLongPress(x, y);
                        });
    }
}
