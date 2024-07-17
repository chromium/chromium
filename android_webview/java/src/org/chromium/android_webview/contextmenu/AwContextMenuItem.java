// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.android_webview.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** List of all predefined Context Menu Items available in WebView. */
public class AwContextMenuItem {
    private AwContextMenuItem() {}

    @IntDef({
        Item.COPY_LINK_ADDRESS,
        Item.COPY_LINK_TEXT,
        Item.OPEN_IN_BROWSER,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Item {
        // Values are numerated from 0 and can't have gaps.
        // The menu and string IDs below must be kept in sync with this list.
        int COPY_LINK_ADDRESS = 0;
        int COPY_LINK_TEXT = 1;
        int OPEN_IN_BROWSER = 2;
        int NUM_ENTRIES = 3;
    }

    private static final int[] MENU_IDS = {
        R.id.contextmenu_copy_link_address, // Item.COPY_LINK_ADDRESS
        R.id.contextmenu_copy_link_text, // Item.COPY_LINK_TEXT
        R.id.contextmenu_open_in_browser_id, // Item.OPEN_IN_BROWSER
    };

    private static final int[] STRING_IDS = {
        R.string.context_menu_copy_link_address, // Item.COPY_LINK_ADDRESS
        R.string.context_menu_copy_link_text, // Item.COPY_LINK_TEXT
        R.string.context_menu_open_in_browser, // Item.OPEN_IN_BROWSER
    };

    /**
     * Returns the menu id for a given {@link @Item}.
     *
     * @param item The {@link @Item}.
     * @return Menu id associated with the {@code item}.
     */
    public static int getMenuId(@Item int item) {
        assert MENU_IDS.length == Item.NUM_ENTRIES;
        return MENU_IDS[item];
    }

    /**
     * Get string ID from the ID of the item.
     *
     * @param item #Item Item ID.
     * @return Returns the string that describes the action of the item.
     */
    private static @StringRes int getStringId(@Item int item) {
        assert STRING_IDS.length == Item.NUM_ENTRIES;
        return STRING_IDS[item];
    }

    /** Transforms the id of the item into a string. */
    public static CharSequence getTitle(Context context, @Item int item) {
        return context.getResources().getString(getStringId(item));
    }
}
