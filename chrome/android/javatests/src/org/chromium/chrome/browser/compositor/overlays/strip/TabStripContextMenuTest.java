// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tab_strip.TabStripContextMenuFacility;
import org.chromium.ui.base.DeviceFormFactor;

/** Instrumentation tests for the tab strip empty space long press context menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@Batch(Batch.PER_CLASS)
@DisableIf.Device(DeviceFormFactor.AUTOMOTIVE) // crbug.com/462592111
public class TabStripContextMenuTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private WebPageStation mPage;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
    }

    @Test
    @LargeTest
    public void testBookmarkAllTabs_EnabledWithMultipleTabs() {
        // 1. Create 2 tabs to ensure we have multiple tabs (count > 1).
        mPage = mPage.openNewTabFast().loadAboutBlank();

        // 2. Perform a long press on the empty space of the tab strip. The menu Facility checks
        // that the "Bookmark all tabs" menu item is displayed and enabled.
        TabStripContextMenuFacility<CtaPageStation> menu = mPage.expectTabStrip().openContextMenu();

        // 3. Click the menu item to ensure it's actionable and closes the menu.
        menu.bookmarkAllTabs();

        // 4. Verify the "Bookmarked" snackbar shows
        // TODO(crbug.com/483119758): Add check to verify bookmark action occurred.
    }

    @Test
    @LargeTest
    public void testBookmarkAllTabs_DisabledWithSingleTab() {
        // 1. Start with 1 tab.
        // Perform a long press on the empty space of the tab strip. The menu Facility checks
        // that the "Bookmark all tabs" menu item is displayed but disabled.
        TabStripContextMenuFacility<CtaPageStation> menu = mPage.expectTabStrip().openContextMenu();
        assertEquals(
                ScrollableFacility.Item.Presence.PRESENT_AND_DISABLED,
                menu.bookmarkAllTabsItem.getPresence());

        // 2. Close menu
        menu.dismiss();
    }
}
