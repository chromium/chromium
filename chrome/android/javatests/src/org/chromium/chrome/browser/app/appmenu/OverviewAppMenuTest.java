// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.Menu;
import android.view.MenuItem;

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
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().showOverview(true); });
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

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            int itemGroupId = item.getGroupId();
            if (itemGroupId == R.id.OVERVIEW_MODE_MENU) {
                int itemId = item.getItemId();
                assertTrue(itemId == R.id.new_tab_menu_id
                        || itemId == R.id.new_incognito_tab_menu_id
                        || itemId == R.id.close_all_tabs_menu_id
                        || itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.menu_group_tabs || itemId == R.id.track_prices_row_menu_id
                        || itemId == R.id.preferences_id);
                if (itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.track_prices_row_menu_id) {
                    assertFalse(item.isVisible());
                } else {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(7));
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

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            int itemGroupId = item.getGroupId();
            if (itemGroupId == R.id.OVERVIEW_MODE_MENU) {
                int itemId = item.getItemId();
                assertTrue(itemId == R.id.new_tab_menu_id
                        || itemId == R.id.new_incognito_tab_menu_id
                        || itemId == R.id.close_all_tabs_menu_id
                        || itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.menu_group_tabs || itemId == R.id.track_prices_row_menu_id
                        || itemId == R.id.preferences_id);
                if (itemId == R.id.close_all_tabs_menu_id
                        || itemId == R.id.track_prices_row_menu_id) {
                    assertFalse(item.isVisible());
                } else {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(7));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.
    EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testAllMenuItemsWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            int itemGroupId = item.getGroupId();
            if (itemGroupId == R.id.START_SURFACE_MODE_MENU) {
                int itemId = item.getItemId();
                assertTrue(itemId == R.id.new_tab_menu_id
                        || itemId == R.id.new_incognito_tab_menu_id
                        || itemId == R.id.divider_line_id || itemId == R.id.open_history_menu_id
                        || itemId == R.id.downloads_menu_id || itemId == R.id.all_bookmarks_menu_id
                        || itemId == R.id.recent_tabs_menu_id
                        || itemId == R.id.close_all_tabs_menu_id
                        || itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.menu_group_tabs || itemId == R.id.track_prices_row_menu_id
                        || itemId == R.id.preferences_id);
                if (itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.track_prices_row_menu_id) {
                    assertFalse(item.isVisible());
                } else {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(13));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.
    EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testIncognitoAllMenuItemsWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            int itemGroupId = item.getGroupId();
            if (itemGroupId == R.id.START_SURFACE_MODE_MENU) {
                int itemId = item.getItemId();
                assertTrue(itemId == R.id.new_tab_menu_id
                        || itemId == R.id.new_incognito_tab_menu_id
                        || itemId == R.id.divider_line_id || itemId == R.id.open_history_menu_id
                        || itemId == R.id.downloads_menu_id || itemId == R.id.all_bookmarks_menu_id
                        || itemId == R.id.recent_tabs_menu_id
                        || itemId == R.id.close_all_tabs_menu_id
                        || itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.menu_group_tabs || itemId == R.id.track_prices_row_menu_id
                        || itemId == R.id.preferences_id);
                if (itemId == R.id.close_all_tabs_menu_id || itemId == R.id.recent_tabs_menu_id
                        || itemId == R.id.track_prices_row_menu_id) {
                    assertFalse(item.isVisible());
                } else {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(13));
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

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.menu_group_tabs) {
                assertFalse(item.isVisible());
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
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

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.menu_group_tabs) {
                int itemGroupId = item.getGroupId();
                if (itemGroupId == R.id.OVERVIEW_MODE_MENU) {
                    assertTrue(item.isVisible());
                }
                if (itemGroupId == R.id.START_SURFACE_MODE_MENU) {
                    assertFalse(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
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

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.menu_group_tabs) {
                assertFalse(item.isVisible());
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.
    EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testGroupTabsIsEnabledWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.menu_group_tabs) {
                int itemGroupId = item.getGroupId();
                if (itemGroupId == R.id.OVERVIEW_MODE_MENU) {
                    assertFalse(item.isVisible());
                }
                if (itemGroupId == R.id.START_SURFACE_MODE_MENU) {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_price_tracking/false"})
    public void
    testTrackPriceOnTabsIsDisabled() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.track_prices_row_menu_id) {
                assertFalse(item.isVisible());
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_price_tracking/true"})
    public void
    testTrackPriceOnTabsIsEnabled() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.track_prices_row_menu_id) {
                int itemGroupId = item.getGroupId();
                if (itemGroupId == R.id.OVERVIEW_MODE_MENU) {
                    assertTrue(item.isVisible());
                }
                if (itemGroupId == R.id.START_SURFACE_MODE_MENU) {
                    assertFalse(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_price_tracking/true"})
    public void
    testTrackPriceOnTabsIsDisabledInIncognitoMode() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.track_prices_row_menu_id) {
                assertFalse(item.isVisible());
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_price_tracking/true"})
    public void
    testTrackPriceOnTabsIsDisabledIfSyncDisabledOrNotSignedIn() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(false);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.track_prices_row_menu_id) {
                assertFalse(item.isVisible());
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID,
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_price_tracking/false"})
    public void
    testTrackPriceOnTabsIsDisabledWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.track_prices_row_menu_id) {
                assertFalse(item.isVisible());
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID,
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_price_tracking/true"})
    // TODO(crbug.com/1152925): Re-add the test when the bug is fixed.
    public void
    testTrackPriceOnTabsIsEnabledWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.track_prices_row_menu_id) {
                assertFalse(item.isVisible());
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }
}
