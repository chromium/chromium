// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.quick_delete.QuickDeleteMetricsDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTabbedActivityEntryPoints;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherAppMenuFacility;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/** Tests the Tab Switcher app menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction({DeviceFormFactor.PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
@Batch(Batch.PER_CLASS)
public class OverviewAppMenuTest {
    @Rule
    public ReusedCtaTransitTestRule<RegularTabSwitcherStation> mCtaTestRule =
            ChromeTransitTestRules.customStartReusedActivityRule(
                    RegularTabSwitcherStation.class,
                    rule ->
                            ChromeTabbedActivityEntryPoints.startOnBlankPage(rule)
                                    .openRegularTabSwitcher());

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock Tracker mTracker;

    public RegularTabSwitcherStation mTabSwitcher;

    @Before
    public void setUp() {
        // Disable IPHs from interfering with tests.
        when(mTracker.shouldTriggerHelpUi(anyString())).thenReturn(false);
        TrackerFactory.setTrackerForTests(mTracker);

        mTabSwitcher = mCtaTestRule.start();
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
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testAllMenuItems_tabGroupEntryPointsFeatureEnabled() {
        TabSwitcherAppMenuFacility menu = mTabSwitcher.openAppMenu();

        try {
            menu.verifyModelItems(
                    List.of(
                            R.id.new_tab_menu_id,
                            R.id.new_incognito_tab_menu_id,
                            R.id.new_tab_group_menu_id,
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
            incognitoTabSwitcher.selectRegularTabsPane();
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
