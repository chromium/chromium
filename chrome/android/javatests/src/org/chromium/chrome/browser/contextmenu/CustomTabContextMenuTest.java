// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import android.app.PendingIntent;
import android.content.Intent;

import androidx.browser.customtabs.CustomContentAction;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for context menus in a Custom Tab. Replaces the need to add CCT-specific tests to
 * ContextMenuTest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class CustomTabContextMenuTest {

    private static final String TEST_PATH =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";
    private String mTestUrl;

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        EmbeddedTestServer testServer =
                mCustomTabActivityTestRule.getEmbeddedTestServerRule().getServer();
        mTestUrl = testServer.getURL(TEST_PATH);

        PendingIntent pendingIntent =
                PendingIntent.getBroadcast(
                        ContextUtils.getApplicationContext(),
                        0,
                        new Intent(),
                        PendingIntent.FLAG_IMMUTABLE);
        CustomContentAction customAction =
                new CustomContentAction.Builder(
                                1,
                                "Test Action",
                                pendingIntent,
                                CustomTabsIntent.CONTENT_TARGET_TYPE_LINK)
                        .build();

        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ContextUtils.getApplicationContext(), mTestUrl);

        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.addCustomContentAction(customAction);
        CustomTabsIntent customTabsIntent = builder.build();

        intent.putExtras(customTabsIntent.intent.getExtras());

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testCustomItemPresent_WhenFeatureEnabled() throws TimeoutException {
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();

        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testLink");

        Integer[] expectedItems = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link,
            ChromeContextMenuPopulator.getCustomMenuItemIdStartForTesting()
        };

        assertMenuItemsAreEqual(menuCoordinator, expectedItems);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testCustomItemNotPresent_WhenFeatureDisabled() throws TimeoutException {
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();

        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testLink");

        Integer[] expectedItems = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link,
        };

        assertMenuItemsAreEqual(menuCoordinator, expectedItems);
    }

    /**
     * Takes all the visible items on the menu and compares them to the list of expected items.
     *
     * @param menu A context menu that is displaying visible items.
     * @param expectedItems A list of items that are expected to appear within a context menu. The
     *     list does not need to be ordered.
     */
    private void assertMenuItemsAreEqual(ContextMenuCoordinator menu, Integer... expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < menu.getCount(); i++) {
            if (menu.getItem(i).model.containsKey(ListMenuItemProperties.MENU_ITEM_ID)) {
                actualItems.add(menu.getItem(i).model.get(ListMenuItemProperties.MENU_ITEM_ID));
            }
        }

        assertThat(
                "Populated menu items were:" + getMenuTitles(menu),
                actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private String getMenuTitles(ContextMenuCoordinator menu) {
        StringBuilder items = new StringBuilder();
        for (int i = 0; i < menu.getCount(); i++) {
            if (menu.getItem(i).model.containsKey(ListMenuItemProperties.TITLE)) {
                items.append("\n").append(menu.getItem(i).model.get(ListMenuItemProperties.TITLE));
            }
        }
        return items.toString();
    }
}
