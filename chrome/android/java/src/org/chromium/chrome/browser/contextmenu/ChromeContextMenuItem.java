// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * List of all predefined Context Menu Items available in Chrome.
 */
public class ChromeContextMenuItem implements ContextMenuItem {
    @IntDef({Item.OPEN_IN_NEW_CHROME_TAB, Item.OPEN_IN_CHROME_INCOGNITO_TAB,
            Item.OPEN_IN_BROWSER_ID, Item.OPEN_IN_NEW_TAB, Item.OPEN_IN_INCOGNITO_TAB,
            Item.OPEN_IN_OTHER_WINDOW, Item.OPEN_IN_EPHEMERAL_TAB, Item.COPY_LINK_ADDRESS,
            Item.COPY_LINK_TEXT, Item.SAVE_LINK_AS, Item.LOAD_ORIGINAL_IMAGE, Item.SAVE_IMAGE,
            Item.OPEN_IMAGE, Item.OPEN_IMAGE_IN_NEW_TAB, Item.OPEN_IMAGE_IN_EPHEMERAL_TAB,
            Item.SEARCH_BY_IMAGE, Item.SEARCH_WITH_GOOGLE_LENS, Item.CALL, Item.SEND_MESSAGE,
            Item.ADD_TO_CONTACTS, Item.COPY, Item.SAVE_VIDEO, Item.OPEN_IN_CHROME})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Item {
        // Values are numerated from 0 and can't have gaps.
        // The menu and string IDs below must be kept in sync with this list.
        // Custom Tab Group
        int OPEN_IN_NEW_CHROME_TAB = 0;
        int OPEN_IN_CHROME_INCOGNITO_TAB = 1;
        int OPEN_IN_BROWSER_ID = 2;
        // Link Group
        int OPEN_IN_NEW_TAB = 3;
        int OPEN_IN_INCOGNITO_TAB = 4;
        int OPEN_IN_OTHER_WINDOW = 5;
        int OPEN_IN_EPHEMERAL_TAB = 6;
        int COPY_LINK_ADDRESS = 7;
        int COPY_LINK_TEXT = 8;
        int SAVE_LINK_AS = 9;
        // Image Group
        int LOAD_ORIGINAL_IMAGE = 10;
        int SAVE_IMAGE = 11;
        int OPEN_IMAGE = 12;
        int OPEN_IMAGE_IN_NEW_TAB = 13;
        int OPEN_IMAGE_IN_EPHEMERAL_TAB = 14;
        int SEARCH_BY_IMAGE = 15;
        int SEARCH_WITH_GOOGLE_LENS = 16;
        // Message Group
        int CALL = 17;
        int SEND_MESSAGE = 18;
        int ADD_TO_CONTACTS = 19;
        int COPY = 20;
        // Video Group
        int SAVE_VIDEO = 21;
        // Other
        int OPEN_IN_CHROME = 22;
        // ALWAYS UPDATE!
        int NUM_ENTRIES = 23;
    }

    /**
     * Mapping from {@link Item} to the ID found in the ids.xml.
     */
    private static final int[] MENU_IDS = {
            R.id.contextmenu_open_in_new_chrome_tab, // Item.OPEN_IN_NEW_CHROME_TAB
            R.id.contextmenu_open_in_chrome_incognito_tab, // Item.OPEN_IN_CHROME_INCOGNITO_TAB
            R.id.contextmenu_open_in_browser_id, // Item.OPEN_IN_BROWSER_ID
            R.id.contextmenu_open_in_new_tab, // Item.OPEN_IN_NEW_TAB
            R.id.contextmenu_open_in_incognito_tab, // Item.OPEN_IN_INCOGNITO_TAB
            R.id.contextmenu_open_in_other_window, // Item.OPEN_IN_OTHER_WINDOW
            R.id.contextmenu_open_in_ephemeral_tab, // Item.OPEN_IN_EPHEMERAL_TAB
            R.id.contextmenu_copy_link_address, // Item.COPY_LINK_ADDRESS
            R.id.contextmenu_copy_link_text, // Item.COPY_LINK_TEXT
            R.id.contextmenu_save_link_as, // Item.SAVE_LINK_AS
            R.id.contextmenu_load_original_image, // Item.LOAD_ORIGINAL_IMAGE
            R.id.contextmenu_save_image, // Item.SAVE_IMAGE
            R.id.contextmenu_open_image, // Item.OPEN_IMAGE
            R.id.contextmenu_open_image_in_new_tab, // Item.OPEN_IMAGE_IN_NEW_TAB
            R.id.contextmenu_open_image_in_ephemeral_tab, // Item.OPEN_IMAGE_IN_EPHEMERAL_TAB
            R.id.contextmenu_search_by_image, // Item.SEARCH_BY_IMAGE
            R.id.contextmenu_search_with_google_lens, // Item.SEARCH_WITH_GOOGLE_LENS
            R.id.contextmenu_call, // Item.CALL
            R.id.contextmenu_send_message, // Item.SEND_MESSAGE
            R.id.contextmenu_add_to_contacts, // Item.ADD_TO_CONTACTS
            R.id.contextmenu_copy, // Item.COPY
            R.id.contextmenu_save_video, // Item.SAVE_VIDEO
            R.id.contextmenu_open_in_chrome, // Item.OPEN_IN_CHROME
    };

    /**
     * Mapping from {@link Item} to the ID of the string that describes the action of the item.
     */
    private static final int[] STRING_IDS = {
            R.string.contextmenu_open_in_new_chrome_tab, // Item.OPEN_IN_NEW_CHROME_TAB:
            R.string.contextmenu_open_in_chrome_incognito_tab, // Item.OPEN_IN_CHROME_INCOGNITO_TAB:
            0, // Item.OPEN_IN_BROWSER_ID is not handled by this mapping.
            R.string.contextmenu_open_in_new_tab, // Item.OPEN_IN_NEW_TAB:
            R.string.contextmenu_open_in_incognito_tab, // Item.OPEN_IN_INCOGNITO_TAB:
            R.string.contextmenu_open_in_other_window, // Item.OPEN_IN_OTHER_WINDOW:
            R.string.contextmenu_open_in_ephemeral_tab, // Item.OPEN_IN_EPHEMERAL_TAB:
            R.string.contextmenu_copy_link_address, // Item.COPY_LINK_ADDRESS:
            R.string.contextmenu_copy_link_text, // Item.COPY_LINK_TEXT:
            R.string.contextmenu_save_link, // Item.SAVE_LINK_AS:
            R.string.contextmenu_load_original_image, // Item.LOAD_ORIGINAL_IMAGE:
            R.string.contextmenu_save_image, // Item.SAVE_IMAGE:
            R.string.contextmenu_open_image, // Item.OPEN_IMAGE:
            R.string.contextmenu_open_image_in_new_tab, // Item.OPEN_IMAGE_IN_NEW_TAB:
            R.string.contextmenu_open_image_in_ephemeral_tab, // Item.OPEN_IMAGE_IN_EPHEMERAL_TAB:
            R.string.contextmenu_search_web_for_image, // Item.SEARCH_BY_IMAGE:
            R.string.contextmenu_search_with_google_lens, // Item.SEARCH_WITH_GOOGLE_LENS:
            R.string.contextmenu_call, // Item.CALL:
            R.string.contextmenu_send_message, // Item.SEND_MESSAGE:
            R.string.contextmenu_add_to_contacts, // Item.ADD_TO_CONTACTS:
            R.string.contextmenu_copy, // Item.COPY:
            R.string.contextmenu_save_video, // Item.SAVE_VIDEO:
            R.string.menu_open_in_chrome, // Item.OPEN_IN_CHROME:
    };

    private final @Item int mItem;

    public ChromeContextMenuItem(@Item int item) {
        mItem = item;
    }

    @Override
    public int getMenuId() {
        assert MENU_IDS.length == Item.NUM_ENTRIES;
        return MENU_IDS[mItem];
    }

    /**
     * Get string ID from the ID of the item.
     * @param item #Item Item ID.
     * @return Returns the string that describes the action of the item.
     */
    private static @StringRes int getStringId(@Item int item) {
        assert STRING_IDS.length == Item.NUM_ENTRIES;

        if (FeatureUtilities.isTabGroupsAndroidEnabled() && item == Item.OPEN_IN_NEW_TAB) {
            return R.string.contextmenu_open_in_new_tab_group;
        }

        return STRING_IDS[item];
    }

    /**
     * Transforms the id of the item into a string. It manages special cases that need minor
     * changes due to templating.
     * @param context Requires to get the string resource related to the item.
     * @return Returns a string for the menu item.
     */
    @Override
    public CharSequence getTitle(Context context) {
        switch (mItem) {
            case Item.OPEN_IN_BROWSER_ID:
                return DefaultBrowserInfo.getTitleOpenInDefaultBrowser(false);
            case Item.SEARCH_BY_IMAGE:
                return context.getString(getStringId(mItem),
                        TemplateUrlServiceFactory.get()
                                .getDefaultSearchEngineTemplateUrl()
                                .getShortName());
            case Item.OPEN_IN_EPHEMERAL_TAB:
            case Item.OPEN_IMAGE_IN_EPHEMERAL_TAB:
            case Item.SEARCH_WITH_GOOGLE_LENS:
                return SpanApplier.applySpans(context.getString(getStringId(mItem)),
                        new SpanInfo("<new>", "</new>", new SuperscriptSpan(),
                                new RelativeSizeSpan(0.75f),
                                new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                                        context.getResources(), R.color.default_text_color_blue))));
            default:
                return context.getString(getStringId(mItem));
        }
    }
}
