// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertTrue;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;
import android.widget.ListView;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.RegularWebPageAppMenuFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.ViewUtils;

/** Public transit tests for the Bottom Bar. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.PHONE)
@EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
@Batch(Batch.PER_CLASS)
public class BottomBarPtTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Test
    @MediumTest
    public void testBottomBarContainerInflatedAndVisible() {
        mActivityTestRule.startOnBlankPage();

        // Will fail or timeout if the bottom bar container is not inflated and visible.
        ViewUtils.waitForVisibleView(withId(R.id.bottom_app_bar_container));
    }

    @Test
    @MediumTest
    public void testBottomBarAppMenuKeptBelowTopControls() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        View toolbar = assumeNonNull(page.toolbarElement).value();
        RegularWebPageAppMenuFacility menu = page.openRegularTabAppMenu();
        ListView listView = menu.menuListElement.value();

        int[] toolbarLocation = new int[2];
        int[] menuLocation = new int[2];
        int toolbarHeight =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            toolbar.getLocationOnScreen(toolbarLocation);
                            listView.getLocationOnScreen(menuLocation);
                            return toolbar.getHeight();
                        });
        int toolbarBottomY = toolbarLocation[1] + toolbarHeight;
        int menuTopY = menuLocation[1];
        int minTopMarginPx =
                page.getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_app_menu_top_margin);

        // AppMenu#calculateMenuHeight() reserves topControlsHeight + bottom_bar_app_menu_top_margin
        // above the menu list when the menu is anchored to the bottom bar with top controls.
        assertTrue(
                "Menu list top should be at least the margin below the top toolbar",
                menuTopY >= toolbarBottomY + minTopMarginPx);

        menu.closeProgrammatically();
    }
}
