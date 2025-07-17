// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.Rect;

import org.chromium.chrome.R;
import org.chromium.ui.widget.RectProvider;

import java.lang.ref.WeakReference;
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
}
