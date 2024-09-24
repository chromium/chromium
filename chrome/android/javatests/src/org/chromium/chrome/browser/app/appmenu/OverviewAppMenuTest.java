// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.quick_delete.QuickDeleteMetricsDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * Tests overview mode app menu popup.
 *
 * <p>TODO(crbug.com/40662624): Add more required tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction({DeviceFormFactor.PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OverviewAppMenuTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void openTabSwitcher() {
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, true);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @DisableFeatures({ChromeFeatureList.QUICK_DELETE_FOR_ANDROID})
    public void testAllMenuItems() throws Exception {
        openTabSwitcher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });

        verifyTabSwitcherMenu();
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testIncognitoAllMenuItems() throws Exception {
        openTabSwitcher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });

        verifyTabSwitcherMenuIncognito();
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @EnableFeatures({ChromeFeatureList.QUICK_DELETE_FOR_ANDROID})
    public void testQuickDeleteMenuItem_Shown() throws Exception {
        openTabSwitcher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });

        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.quick_delete_menu_id));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @EnableFeatures({ChromeFeatureList.QUICK_DELETE_FOR_ANDROID})
    public void testQuickDeleteTabSwitcherMenu_entryFromTabSwitcherMenuItemHistogram() {
        openTabSwitcher();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                        QuickDeleteMetricsDelegate.QuickDeleteAction
                                .TAB_SWITCHER_MENU_ITEM_CLICKED);

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.quick_delete_menu_id);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testIncognitoAllMenuItemsWithStartSurface() throws Exception {
        openTabSwitcher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });

        verifyTabSwitcherMenuIncognito();
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testSelectTabsIsEnabledWithStartSurface() throws Exception {
        openTabSwitcher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });

        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.menu_select_tabs));
    }

    private void verifyTabSwitcherMenu() {
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.new_tab_menu_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.new_incognito_tab_menu_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.close_all_tabs_menu_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.menu_select_tabs));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.preferences_id));

        assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(),
                        R.id.close_all_incognito_tabs_menu_id));

        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(mActivityTestRule.getAppMenuCoordinator());
        assertEquals(5, menuItemsModelList.size());
    }

    private void verifyTabSwitcherMenuIncognito() {
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.new_tab_menu_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.new_incognito_tab_menu_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(),
                        R.id.close_all_incognito_tabs_menu_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.menu_select_tabs));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.preferences_id));

        assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.close_all_tabs_menu_id));

        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(mActivityTestRule.getAppMenuCoordinator());
        assertEquals(5, menuItemsModelList.size());
    }
}
