// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.listmenu.ListItemType.DIVIDER;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.SUBMENU_HEADER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE_ID;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.PluralsRes;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;

/** Utility class containing common test logic for tab strip context menus. */
public class StripLayoutContextMenuCoordinatorTestUtils {
    public static void testAnchorWidth(
            WeakReference<Activity> weakReferenceActivity,
            Function<Integer, Integer> getMenuWidthFunc) {
        testAnchorWidth_smallAnchorWidth(weakReferenceActivity, getMenuWidthFunc);
        testAnchorWidth_largeAnchorWidth(weakReferenceActivity, getMenuWidthFunc);
        testAnchorWidth_moderateAnchorWidth(weakReferenceActivity, getMenuWidthFunc);
    }

    private static void testAnchorWidth_smallAnchorWidth(
            WeakReference<Activity> weakReferenceActivity,
            Function<Integer, Integer> getMenuWidthFunc) {
        assertEquals(
                weakReferenceActivity
                        .get()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width),
                (int) getMenuWidthFunc.apply(1));
    }

    private static void testAnchorWidth_largeAnchorWidth(
            WeakReference<Activity> weakReferenceActivity,
            Function<Integer, Integer> getMenuWidthFunc) {
        assertEquals(
                weakReferenceActivity
                        .get()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width),
                (int) getMenuWidthFunc.apply(10000));
    }

    private static void testAnchorWidth_moderateAnchorWidth(
            WeakReference<Activity> weakReferenceActivity,
            Function<Integer, Integer> getMenuWidthFunc) {
        int minWidth =
                weakReferenceActivity
                        .get()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width);
        int expectedWidth = minWidth + 1;
        assertEquals(expectedWidth, (int) getMenuWidthFunc.apply(expectedWidth));
    }

    public static void testAnchor_offset(
            Consumer<RectProvider> showMenuAction, Runnable destroyMenuAction) {
        RectProvider rectProvider = new RectProvider();
        rectProvider.setRect(new Rect(0, 10, 50, 40));
        showMenuAction.accept(rectProvider);
        assertEquals(
                "Expected anchor rect to have a top offset of popup_menu_shadow_length, "
                        + "and a width which accounts for the popup_menu_shadow_length",
                new Rect(0, 4, 74, 34),
                rectProvider.getRect());
        // Clean up to avoid "object not destroyed after test".
        destroyMenuAction.run();
    }

    public static void testAnchor_offset_incognito(
            Consumer<RectProvider> showMenuAction, Runnable destroyMenuAction) {
        RectProvider rectProvider = new RectProvider();
        rectProvider.setRect(new Rect(0, 10, 50, 40));
        showMenuAction.accept(rectProvider);
        assertEquals(
                "Expected anchor rect to not have any offset in incognito",
                new Rect(0, 10, 50, 40),
                rectProvider.getRect());
        // Clean up to avoid "object not destroyed after test".
        destroyMenuAction.run();
    }

    public static void verifyAddToWindowSubmenu(
            ModelList modelList,
            int indexOfAddToWindow,
            @PluralsRes int label,
            List<String> otherWindowTitles,
            Activity activity) {
        int modelListSizeBeforeNav = modelList.size();
        var moveToOtherWindowItem = modelList.get(indexOfAddToWindow);
        var subMenu = moveToOtherWindowItem.model.get(SUBMENU_ITEMS);
        int expectedNumberOfItems =
                1 + (otherWindowTitles.isEmpty() ? 0 : 1 + otherWindowTitles.size());
        assertEquals(
                "Submenu should have "
                        + expectedNumberOfItems
                        + " item(s), but was "
                        + getDebugString(subMenu),
                expectedNumberOfItems,
                subMenu.size());
        moveToOtherWindowItem.model.get(CLICK_LISTENER).onClick(new View(activity));
        assertNotNull("Submenu should be present", subMenu);
        assertEquals(
                "Expected to display "
                        + expectedNumberOfItems
                        + 1 // Back header added
                        + " item(s) after entering submenu, but was "
                        + getDebugString(modelList),
                expectedNumberOfItems + 1,
                modelList.size());
        MVCListAdapter.ListItem headerItem = modelList.get(0);
        assertEquals(
                "Expected first item to have SUBMENU_HEADER type", SUBMENU_HEADER, headerItem.type);
        assertEquals(
                "Expected submenu back header to have the same text as submenu parent item",
                activity.getResources().getQuantityString(label, 2),
                headerItem.model.get(TITLE));
        assertTrue("Expected submenu header to be enabled", headerItem.model.get(ENABLED));
        assertEquals("Expected 2nd item to have MENU_ITEM type", MENU_ITEM, modelList.get(1).type);
        assertEquals(
                "Expected 2nd item to be 'New window' row",
                R.string.menu_new_window,
                modelList.get(1).model.get(TITLE_ID));
        if (!otherWindowTitles.isEmpty()) {
            assertEquals(
                    "Expected 3rd item to be divider, but was " + getDebugString(modelList),
                    DIVIDER,
                    modelList.get(2).type);
            for (int i = 0; i < otherWindowTitles.size(); i++) {
                assertEquals(
                        "Expected window row at position " + (i + 3) + " to have MENU_ITEM type",
                        MENU_ITEM,
                        modelList.get(1).type);
                assertEquals(
                        "Expected window row at position "
                                + (i + 2)
                                + " to have text "
                                + otherWindowTitles.get(i),
                        otherWindowTitles.get(i),
                        modelList.get(i + 3).model.get(TITLE));
                assertTrue(
                        "Expected window row at position " + (i + 3) + " to be enabled",
                        modelList.get(i + 3).model.get(ENABLED));
            }
        }
        headerItem.model.get(CLICK_LISTENER).onClick(new View(activity));
        assertEquals(
                "Expected to navigate back to parent menu",
                modelListSizeBeforeNav,
                modelList.size());
    }

    public static void clickMoveToNewWindow(
            ModelList modelList, int moveToOtherWindowIdx, View view) {
        var moveToOtherWindowItem = modelList.get(moveToOtherWindowIdx);
        moveToOtherWindowItem.model.get(CLICK_LISTENER).onClick(view);
        assertTrue(
                "Expected model list to have at least 2 items, but contents were "
                        + getDebugString(modelList),
                modelList.size() >= 2);
        MVCListAdapter.ListItem newWindowItem = modelList.get(1);
        assertEquals("Expected 2nd item to have MENU_ITEM type", MENU_ITEM, newWindowItem.type);
        assertEquals(
                "Expected 2nd item to be 'New window' row",
                R.string.menu_new_window,
                newWindowItem.model.get(TITLE_ID));

        newWindowItem.model.get(CLICK_LISTENER).onClick(view);
    }

    public static void clickMoveToWindowRow(
            ModelList modelList, int moveToOtherWindowIdx, String expectedWindowTitle, View view) {
        var moveToOtherWindowItem = modelList.get(moveToOtherWindowIdx);
        moveToOtherWindowItem.model.get(CLICK_LISTENER).onClick(view);
        assertEquals("Expected model list to have 4 items", 4, modelList.size());
        MVCListAdapter.ListItem windowRowItem = modelList.get(3);
        assertEquals("Expected 4th item to be a menu item", MENU_ITEM, windowRowItem.type);
        assertEquals(
                "Expected 4th item to display the name of the other window",
                expectedWindowTitle,
                windowRowItem.model.get(TITLE));

        windowRowItem.model.get(CLICK_LISTENER).onClick(view);
    }

    private static String getDebugString(ModelList modelList) {
        StringBuilder modelListContents = new StringBuilder();
        for (int i = 0; i < modelList.size(); i++) {
            modelListContents.append(modelList.get(i).type);
            modelListContents.append(" ");
            modelListContents.append(
                    PropertyModel.getFromModelOrDefault(modelList.get(i).model, TITLE, null));
            if (i < modelList.size() - 1) modelListContents.append(", ");
        }
        return modelListContents.toString();
    }

    private static String getDebugString(List<MVCListAdapter.ListItem> items) {
        StringBuilder modelListContents = new StringBuilder();
        for (int i = 0; i < items.size(); i++) {
            modelListContents.append(items.get(i).type);
            modelListContents.append(" ");
            modelListContents.append(
                    PropertyModel.getFromModelOrDefault(items.get(i).model, TITLE, null));
            if (i < items.size() - 1) modelListContents.append(", ");
        }
        return modelListContents.toString();
    }
}
