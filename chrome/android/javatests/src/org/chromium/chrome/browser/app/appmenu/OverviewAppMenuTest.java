// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests overview mode app menu popup.
 *
 * TODO(crbug.com/1031958): Add more required tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OverviewAppMenuTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, true);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    public void testAllMenuItemsWithoutStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        verifyTabSwitcherMenu(false);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    public void testIncognitoAllMenuItemsWithoutStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        verifyTabSwitcherMenuIncognito(false);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.
    EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void testAllMenuItemsWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        verifyTabSwitcherMenu(false);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID,
            ChromeFeatureList.TAB_GROUPS_ANDROID, ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void
    testAllMenuItemsWithStartSurfaceAndSelectTabs() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        verifyTabSwitcherMenu(true);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.DisableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @Features.
    EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testIncognitoAllMenuItemsWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        verifyTabSwitcherMenuIncognito(false);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID,
            ChromeFeatureList.TAB_GROUPS_ANDROID, ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void
    testIncognitoAllMenuItemsWithStartSurfaceAndSelectTabs() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        verifyTabSwitcherMenuIncognito(true);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:open_ntp_instead_of_start/true"})
    public void testNewTabIsEnabledWithStartSurfaceV2() throws Exception {
        // clang-format on
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.new_tab_menu_id));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.
    DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testGroupTabsIsDisabled() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.menu_group_tabs));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    public void testGroupTabsIsEnabled() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.menu_group_tabs));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testGroupTabsIsDisabledWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.menu_group_tabs));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.DisableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @Features.
    EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testGroupTabsIsEnabledWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.menu_group_tabs));
        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.menu_select_tabs));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID,
            ChromeFeatureList.TAB_GROUPS_ANDROID, ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void
    testGroupTabsIsEnabledWithStartSurfaceAndSelectTabs() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.menu_group_tabs));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.menu_select_tabs));
    }

    private void verifyTabSwitcherMenu(boolean selectTabs) {
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.new_tab_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.new_incognito_tab_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.close_all_tabs_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(),
                selectTabs ? R.id.menu_select_tabs : R.id.menu_group_tabs));
        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(),
                selectTabs ? R.id.menu_group_tabs : R.id.menu_select_tabs));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.preferences_id));

        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.close_all_incognito_tabs_menu_id));

        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(mActivityTestRule.getAppMenuCoordinator());
        assertEquals(5, menuItemsModelList.size());
    }

    private void verifyTabSwitcherMenuIncognito(boolean selectTabs) {
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.new_tab_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.new_incognito_tab_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.close_all_incognito_tabs_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(),
                selectTabs ? R.id.menu_select_tabs : R.id.menu_group_tabs));
        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(),
                selectTabs ? R.id.menu_group_tabs : R.id.menu_select_tabs));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.preferences_id));

        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.close_all_tabs_menu_id));

        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(mActivityTestRule.getAppMenuCoordinator());
        assertEquals(5, menuItemsModelList.size());
    }
}
