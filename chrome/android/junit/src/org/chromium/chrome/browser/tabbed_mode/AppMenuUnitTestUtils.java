// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemWithSubmenuProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BiConsumer;

/** Utilities for testing AppMenu builders. */
public class AppMenuUnitTestUtils {

    /** Represents an expected node in an app menu tree for assertions. */
    public static class MenuItem {
        public final Object property;
        public final MenuItem[] children;

        public MenuItem(Object property, MenuItem... children) {
            this.property = property;
            this.children = children;
        }
    }

    /**
     * Shorthand factory method for creating a {@link MenuItem}.
     *
     * @param property The expected property (e.g. ID or title) for the item.
     * @param children Expected child sub-menu items, if any.
     * @return A new {@link MenuItem} instance.
     */
    public static MenuItem item(Object property, MenuItem... children) {
        return new MenuItem(property, children);
    }

    /**
     * Asserts that a tree of menu items matches the expected structure of item IDs.
     *
     * @param items The actual top-level menu items.
     * @param expectedItems The expected top-level menu items with expected IDs.
     */
    public static void assertMenuItemsAreEqual(
            Iterable<ListItem> items, List<MenuItem> expectedItems) {
        assertMenuTreesAreEqual(
                items,
                expectedItems,
                (item, expectedProperty) -> {
                    assertEquals(
                            "Mismatched item id.",
                            expectedProperty,
                            item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
                });
    }

    /**
     * Asserts that a tree of menu items matches the expected structure of item titles.
     *
     * @param items The actual top-level menu items.
     * @param expectedItems The expected top-level menu items with expected string or resource IDs.
     */
    public static void assertMenuTitlesAreEqual(
            Iterable<ListItem> items, List<MenuItem> expectedItems) {
        assertMenuTreesAreEqual(
                items,
                expectedItems,
                (item, expectedProperty) -> {
                    if (expectedProperty instanceof Integer) {
                        if ((Integer) expectedProperty == 0) {
                            assertNull(item.model.get(AppMenuItemProperties.TITLE));
                            return;
                        }
                        assertEquals(
                                "Mismatched title.",
                                ContextUtils.getApplicationContext()
                                        .getString((Integer) expectedProperty),
                                item.model.get(AppMenuItemProperties.TITLE));
                    } else {
                        assertEquals(
                                "Mismatched title.",
                                expectedProperty,
                                item.model.get(AppMenuItemProperties.TITLE));
                    }
                });
    }

    private static void assertMenuTreesAreEqual(
            Iterable<ListItem> items,
            List<MenuItem> expectedNodes,
            BiConsumer<ListItem, Object> assertionLogic) {
        List<ListItem> itemList = new ArrayList<>();
        for (ListItem item : items) {
            itemList.add(item);
        }

        assertEquals("Mismatched item count.", expectedNodes.size(), itemList.size());
        for (int i = 0; i < itemList.size(); ++i) {
            assertMenuTreesAreEqualRecursively(
                    itemList.get(i), expectedNodes.get(i), assertionLogic);
        }
    }

    private static void assertMenuTreesAreEqualRecursively(
            ListItem item, MenuItem expectedNode, BiConsumer<ListItem, Object> assertionLogic) {
        assertionLogic.accept(item, expectedNode.property);

        boolean hasSubItems =
                item.model.containsKey(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER);
        assertEquals("Mismatched children.", expectedNode.children.length > 0, hasSubItems);

        if (!hasSubItems) {
            return;
        }

        assertTrue(
                "We got Item for empty submenu, which was unexpected.",
                item.model.containsKey(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER));
        List<ListItem> children =
                item.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertEquals(
                "Mismatched submenu item count.", expectedNode.children.length, children.size());

        for (int i = 0; i < children.size(); ++i) {
            assertMenuTreesAreEqualRecursively(
                    children.get(i), expectedNode.children[i], assertionLogic);
        }
    }

    /**
     * Finds a {@link ListItem} in a given iterable by its {@link
     * AppMenuItemProperties#MENU_ITEM_ID}. Note: This only searches the provided iterable and does
     * not recursively search submenus.
     *
     * @param menuItems The items to search through.
     * @param id The menu item ID to look for.
     * @return The matching {@link ListItem}, or null if not found.
     */
    public static ListItem findItemById(Iterable<ListItem> menuItems, int id) {
        for (ListItem item : menuItems) {
            if (item.model.get(AppMenuItemProperties.MENU_ITEM_ID) == id) {
                return item;
            }
        }
        return null;
    }
}
