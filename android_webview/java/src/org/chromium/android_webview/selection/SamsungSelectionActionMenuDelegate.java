// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.selection;

import android.os.Build;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;

import org.chromium.android_webview.R;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.content_public.common.ContentFeatures;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Interface for customizing text selection menu items in {@link SelectionPopupController}. */
public class SamsungSelectionActionMenuDelegate implements SelectionActionMenuDelegate {
    private static final String TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME =
            "Android.WebView.TextSelectionMenuOrdering";

    // This should be kept in sync with the definition
    // |AndroidWebViewTextSelectionMenuOrdering| in
    // tools/metrics/histograms/metadata/android/enums.xml
    @IntDef({
        TextSelectionMenuOrdering.DEFAULT_MENU_ORDER,
        TextSelectionMenuOrdering.SAMSUNG_MENU_ORDER,
        TextSelectionMenuOrdering.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface TextSelectionMenuOrdering {
        int DEFAULT_MENU_ORDER = 0;
        int SAMSUNG_MENU_ORDER = 1;
        int COUNT = 2;
    }

    /**
     * On Samsung devices, OS mandates a different ordering than stock Android, and we want to be
     * consistent. This ordering is only used on WebView.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        SamsungDefaultItemOrder.CUT,
        SamsungDefaultItemOrder.COPY,
        SamsungDefaultItemOrder.PASTE,
        SamsungDefaultItemOrder.PASTE_AS_PLAIN_TEXT,
        SamsungDefaultItemOrder.SELECT_ALL,
        SamsungDefaultItemOrder.SHARE,
        SamsungDefaultItemOrder.WEB_SEARCH
    })
    public @interface SamsungDefaultItemOrder {
        int CUT = 1;
        int COPY = 2;
        int PASTE = 3;
        int PASTE_AS_PLAIN_TEXT = 4;
        int SELECT_ALL = 5;
        int SHARE = 6;
        int WEB_SEARCH = 7;
    }

    @Override
    public void getModifiedMenuItems(List<SelectionMenuItem.Builder> menuItemBuilders) {
        for (SelectionMenuItem.Builder builder : menuItemBuilders) {
            int menuItemOrder = getMenuItemOrder(builder.mId);
            if (menuItemOrder == -1) continue;
            builder.setOrderInCategory(menuItemOrder);
        }
    }

    public static void maybeAttachActionMenuDelegate(SelectionPopupController controller) {
        if (shouldUseSamsungMenuItemOrdering()) {
            controller.setSelectionActionMenuDelegate(new SamsungSelectionActionMenuDelegate());
            RecordHistogram.recordEnumeratedHistogram(
                    TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME,
                    TextSelectionMenuOrdering.SAMSUNG_MENU_ORDER,
                    TextSelectionMenuOrdering.COUNT);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME,
                    TextSelectionMenuOrdering.DEFAULT_MENU_ORDER,
                    TextSelectionMenuOrdering.COUNT);
        }
    }

    private static boolean shouldUseSamsungMenuItemOrdering() {
        return Build.VERSION.SDK_INT <= Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                && "SAMSUNG".equalsIgnoreCase(Build.MANUFACTURER)
                && ContentFeatureMap.isEnabled(ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION);
    }

    private int getMenuItemOrder(@IdRes int id) {
        if (id == R.id.select_action_menu_cut) {
            return SamsungDefaultItemOrder.CUT;
        } else if (id == R.id.select_action_menu_copy) {
            return SamsungDefaultItemOrder.COPY;
        } else if (id == R.id.select_action_menu_paste) {
            return SamsungDefaultItemOrder.PASTE;
        } else if (id == R.id.select_action_menu_select_all) {
            return SamsungDefaultItemOrder.SELECT_ALL;
        } else if (id == R.id.select_action_menu_share) {
            return SamsungDefaultItemOrder.SHARE;
        } else if (id == R.id.select_action_menu_paste_as_plain_text) {
            return SamsungDefaultItemOrder.PASTE_AS_PLAIN_TEXT;
        } else if (id == R.id.select_action_menu_web_search) {
            return SamsungDefaultItemOrder.WEB_SEARCH;
        }
        return -1;
    }
}
