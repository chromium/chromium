// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.quick_delete.QuickDeleteMetricsDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherAppMenuFacility;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/** Tests the Tab Switcher app menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction({DeviceFormFactor.PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class OverviewAppMenuTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    public BatchedPublicTransitRule<RegularTabSwitcherStation> mBatchedRule =
            new BatchedPublicTransitRule<>(
                    RegularTabSwitcherStation.class, /* expectResetByTest= */ true);
    public ChromeTabbedActivityPublicTransitEntryPoints mEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(sActivityTestRule);
    public RegularTabSwitcherStation mTabSwitcher;

    @Before
    public void setUp() {
        mTabSwitcher =
                mEntryPoints.startBatched(
                        mBatchedRule,
                        () -> mEntryPoints.startOnBlankPageNonBatched().openRegularTabSwitcher());
    }

    @Test
    @LargeTest
    @Feature({"Browser", "Main"})
    public void testAllMenuItems() {
        TabSwitcherAppMenuFacility menu = mTabSwitcher.openAppMenu();

        try {
            menu.verifyModelItems(
                    List.of(
                            R.id.new_tab_menu_id,
                            R.id.new_incognito_tab_menu_id,
                            R.id.close_all_tabs_menu_id,
                            R.id.menu_select_tabs,
                            R.id.quick_delete_menu_id,
                            R.id.preferences_id));

            menu.verifyPresentItems();
        } finally {
            menu.closeProgrammatically();
        }
    }

    @Test
    @LargeTest
    @Feature({"Browser", "Main"})
    public void testIncognitoAllMenuItems() {
        IncognitoTabSwitcherStation incognitoTabSwitcher =
                mTabSwitcher.openAppMenu().openNewIncognitoTab().openIncognitoTabSwitcher();
        TabSwitcherAppMenuFacility menu = incognitoTabSwitcher.openAppMenu();

        try {
            menu.verifyModelItems(
                    List.of(
                            R.id.new_tab_menu_id,
                            R.id.new_incognito_tab_menu_id,
                            R.id.close_all_incognito_tabs_menu_id,
                            R.id.menu_select_tabs,
                            R.id.preferences_id));

            menu.verifyPresentItems();
        } finally {
            menu.closeProgrammatically();
            incognitoTabSwitcher.selectRegularTabList();
        }
    }

    @Test
    @LargeTest
    @Feature({"Browser", "Main"})
    public void testQuickDeleteTabSwitcherMenu_entryFromTabSwitcherMenuItemHistogram() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                                QuickDeleteMetricsDelegate.QuickDeleteAction
                                        .TAB_SWITCHER_MENU_ITEM_CLICKED,
                                QuickDeleteMetricsDelegate.QuickDeleteAction
                                        .LAST_15_MINUTES_SELECTED)
                        .build();

        QuickDeleteDialogFacility dialog = mTabSwitcher.openAppMenu().clearBrowsingData();

        try {
            histogramWatcher.assertExpected();
        } finally {
            dialog.clickCancel();
        }
    }
}
