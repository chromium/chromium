// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** List of all predefined Context Menu Items available in Chrome. */
class ChromeContextMenuItem {
    @IntDef({
        Item.OPEN_IN_NEW_CHROME_TAB,
        Item.OPEN_IN_CHROME_INCOGNITO_TAB,
        Item.OPEN_IN_BROWSER_ID,
        Item.OPEN_IN_NEW_TAB,
        Item.OPEN_IN_INCOGNITO_TAB,
        Item.OPEN_IN_OTHER_WINDOW,
        Item.OPEN_IN_NEW_WINDOW,
        Item.OPEN_IN_EPHEMERAL_TAB,
        Item.COPY_LINK_ADDRESS,
        Item.COPY_LINK_TEXT,
        Item.SAVE_LINK_AS,
        Item.SHARE_LINK,
        Item.DIRECT_SHARE_LINK,
        Item.READ_LATER,
        Item.LOAD_ORIGINAL_IMAGE,
        Item.SAVE_IMAGE,
        Item.OPEN_IMAGE,
        Item.OPEN_IMAGE_IN_NEW_TAB,
        Item.OPEN_IMAGE_IN_EPHEMERAL_TAB,
        Item.COPY_IMAGE,
        Item.SEARCH_BY_IMAGE,
        Item.SEARCH_WITH_GOOGLE_LENS,
        Item.SHOP_IMAGE_WITH_GOOGLE_LENS,
        Item.SHARE_IMAGE,
        Item.DIRECT_SHARE_IMAGE,
        Item.CALL,
        Item.SEND_MESSAGE,
        Item.ADD_TO_CONTACTS,
        Item.COPY,
        Item.SAVE_VIDEO,
        Item.OPEN_IN_CHROME,
        Item.OPEN_IN_NEW_TAB_IN_GROUP,
        Item.SHARE_HIGHLIGHT,
        Item.REMOVE_HIGHLIGHT,
        Item.LEARN_MORE
    })
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
        int OPEN_IN_NEW_TAB_IN_GROUP = 4;
        int OPEN_IN_INCOGNITO_TAB = 5;
        int OPEN_IN_OTHER_WINDOW = 6;
        int OPEN_IN_NEW_WINDOW = 7;
        int OPEN_IN_EPHEMERAL_TAB = 8;
        int COPY_LINK_ADDRESS = 9;
        int COPY_LINK_TEXT = 10;
        int SAVE_LINK_AS = 11;
        int SHARE_LINK = 12;
        int DIRECT_SHARE_LINK = 13;
        int READ_LATER = 14;
        // Image Group
        int LOAD_ORIGINAL_IMAGE = 15;
        int SAVE_IMAGE = 16;
        int OPEN_IMAGE = 17;
        int OPEN_IMAGE_IN_NEW_TAB = 18;
        int OPEN_IMAGE_IN_EPHEMERAL_TAB = 19;
        int COPY_IMAGE = 20;
        int SEARCH_BY_IMAGE = 21;
        int SEARCH_WITH_GOOGLE_LENS = 22;
        int SHOP_IMAGE_WITH_GOOGLE_LENS = 23;
        int SHARE_IMAGE = 24;
        int DIRECT_SHARE_IMAGE = 25;
        // Message Group
        int CALL = 26;
        int SEND_MESSAGE = 27;
        int ADD_TO_CONTACTS = 28;
        int COPY = 29;
        // Video Group
        int SAVE_VIDEO = 30;
        // Other
        int OPEN_IN_CHROME = 31;
        // Shared Highlighting options
        int SHARE_HIGHLIGHT = 32;
        int REMOVE_HIGHLIGHT = 33;
        int LEARN_MORE = 34;
        // ALWAYS UPDATE!
        int NUM_ENTRIES = 35;
    }

    /** Mapping from {@link Item} to the ID found in the ids.xml. */
    private static final int[] MENU_IDS = {
        R.id.contextmenu_open_in_new_chrome_tab, // Item.OPEN_IN_NEW_CHROME_TAB
        R.id.contextmenu_open_in_chrome_incognito_tab, // Item.OPEN_IN_CHROME_INCOGNITO_TAB
        R.id.contextmenu_open_in_browser_id, // Item.OPEN_IN_BROWSER_ID
        R.id.contextmenu_open_in_new_tab, // Item.OPEN_IN_NEW_TAB
        R.id.contextmenu_open_in_new_tab_in_group, // Item.OPEN_IN_NEW_TAB_IN_GROUP
        R.id.contextmenu_open_in_incognito_tab, // Item.OPEN_IN_INCOGNITO_TAB
        R.id.contextmenu_open_in_other_window, // Item.OPEN_IN_OTHER_WINDOW
        R.id.contextmenu_open_in_new_window, // Item.OPEN_IN_NEW_WINDOW
        R.id.contextmenu_open_in_ephemeral_tab, // Item.OPEN_IN_EPHEMERAL_TAB
        R.id.contextmenu_copy_link_address, // Item.COPY_LINK_ADDRESS
        R.id.contextmenu_copy_link_text, // Item.COPY_LINK_TEXT
        R.id.contextmenu_save_link_as, // Item.SAVE_LINK_AS
        R.id.contextmenu_share_link, // Item.SHARE_LINK
        R.id.contextmenu_direct_share_link, // Item.DIRECT_SHARE_LINK
        R.id.contextmenu_read_later, // Item.READ_LATER
        R.id.contextmenu_load_original_image, // Item.LOAD_ORIGINAL_IMAGE
        R.id.contextmenu_save_image, // Item.SAVE_IMAGE
        R.id.contextmenu_open_image, // Item.OPEN_IMAGE
        R.id.contextmenu_open_image_in_new_tab, // Item.OPEN_IMAGE_IN_NEW_TAB
        R.id.contextmenu_open_image_in_ephemeral_tab, // Item.OPEN_IMAGE_IN_EPHEMERAL_TAB
        R.id.contextmenu_copy_image, // Item.COPY_IMAGE
        R.id.contextmenu_search_by_image, // Item.SEARCH_BY_IMAGE
        R.id.contextmenu_search_with_google_lens, // Item.SEARCH_WITH_GOOGLE_LENS
        R.id.contextmenu_shop_image_with_google_lens, // Item.SHOP_IMAGE_WITH_GOOGLE_LENS
        R.id.contextmenu_share_image, // Item.SHARE_IMAGE
        R.id.contextmenu_direct_share_image, // Item.DIRECT_SHARE_IMAGE
        R.id.contextmenu_call, // Item.CALL
        R.id.contextmenu_send_message, // Item.SEND_MESSAGE
        R.id.contextmenu_add_to_contacts, // Item.ADD_TO_CONTACTS
        R.id.contextmenu_copy, // Item.COPY
        R.id.contextmenu_save_video, // Item.SAVE_VIDEO
        R.id.contextmenu_open_in_chrome, // Item.OPEN_IN_CHROME
        R.id.contextmenu_share_highlight, // Item.SHARE_HIGHLIGHT
        R.id.contextmenu_remove_highlight, // Item.REMOVE_HIGHLIGHT
        R.id.contextmenu_learn_more, // Item.LEARN_MORE
    };

    /** Mapping from {@link Item} to the ID of the string that describes the action of the item. */
    private static final int[] STRING_IDS = {
        R.string.contextmenu_open_in_new_chrome_tab, // Item.OPEN_IN_NEW_CHROME_TAB:
        R.string.contextmenu_open_in_chrome_incognito_tab, // Item.OPEN_IN_CHROME_INCOGNITO_TAB:
        0, // Item.OPEN_IN_BROWSER_ID is not handled by this mapping.
        R.string.contextmenu_open_in_new_tab, // Item.OPEN_IN_NEW_TAB:
        R.string.contextmenu_open_in_new_tab_group, // Item.OPEN_IN_NEW_TAB_IN_GROUP
        R.string.contextmenu_open_in_incognito_tab, // Item.OPEN_IN_INCOGNITO_TAB:
        R.string.contextmenu_open_in_other_window, // Item.OPEN_IN_OTHER_WINDOW:
        R.string.contextmenu_open_in_new_window, // Item.OPEN_IN_NEW_WINDOW:
        R.string.contextmenu_open_in_ephemeral_tab, // Item.OPEN_IN_EPHEMERAL_TAB:
        R.string.contextmenu_copy_link_address, // Item.COPY_LINK_ADDRESS:
        R.string.contextmenu_copy_link_text, // Item.COPY_LINK_TEXT:
        R.string.contextmenu_save_link, // Item.SAVE_LINK_AS:
        R.string.contextmenu_share_link, // Item.SHARE_LINK
        0, // Item.DIRECT_SHARE_LINK is not handled by this mapping.
        R.string.contextmenu_read_later, // Item.READ_LATER
        R.string.contextmenu_load_original_image, // Item.LOAD_ORIGINAL_IMAGE:
        R.string.contextmenu_save_image, // Item.SAVE_IMAGE:
        R.string.contextmenu_open_image, // Item.OPEN_IMAGE:
        R.string.contextmenu_open_image_in_new_tab, // Item.OPEN_IMAGE_IN_NEW_TAB:
        R.string.contextmenu_open_image_in_ephemeral_tab, // Item.OPEN_IMAGE_IN_EPHEMERAL_TAB:
        R.string.contextmenu_copy_image, // Item.COPY_IMAGE:
        R.string.contextmenu_search_web_for_image, // Item.SEARCH_BY_IMAGE:
        R.string.contextmenu_search_image_with_google_lens, // Item.SEARCH_WITH_GOOGLE_LENS:
        R.string.contextmenu_shop_image_with_google_lens, // Item.SHOP_IMAGE_WITH_GOOGLE_LENS:
        R.string.contextmenu_share_image, // Item.SHARE_IMAGE
        0, // Item.DIRECT_SHARE_IMAGE is not handled by this mapping.
        R.string.contextmenu_call, // Item.CALL:
        R.string.contextmenu_send_message, // Item.SEND_MESSAGE:
        R.string.contextmenu_add_to_contacts, // Item.ADD_TO_CONTACTS:
        R.string.contextmenu_copy, // Item.COPY:
        R.string.contextmenu_save_video, // Item.SAVE_VIDEO:
        R.string.menu_open_in_chrome, // Item.OPEN_IN_CHROME:
        R.string.contextmenu_share_highlight, // Item.SHARE_HIGHLIGHT
        R.string.contextmenu_remove_highlight, // Item.REMOVE_HIGHLIGHT
        R.string.contextmenu_learn_more, // Item.LEARN_MORE
    };

    /**
     * Returns the menu id for a given {@link @Item}.
     * @param item The {@link @Item}.
     * @return Menu id associated with the {@code item}.
     */
    public static int getMenuId(@Item int item) {
        assert MENU_IDS.length == Item.NUM_ENTRIES;
        return MENU_IDS[item];
    }

    /**
     * Get string ID from the ID of the item.
     * @param context The activity context.
     * @param item #Item Item ID.
     * @return Returns the string that describes the action of the item.
     */
    private static @StringRes int getStringId(Context context, @Item int item) {
        assert STRING_IDS.length == Item.NUM_ENTRIES;

        return STRING_IDS[item];
    }

    /**
     * Transforms the id of the item into a string. It manages special cases that need minor changes
     * due to templating.
     *
     * @param context Requires to get the string resource related to the item.
     * @param profile The {@link Profile} associated with the current page.
     * @param item Context menu item id.
     * @param showInProductHelp Whether the menu item should show the new superscript label.
     * @return Returns a string for the menu item.
     */
    public static CharSequence getTitle(
            Context context, Profile profile, @Item int item, boolean showInProductHelp) {
        switch (item) {
            case Item.OPEN_IN_BROWSER_ID:
                return DefaultBrowserInfo.getTitleOpenInDefaultBrowser(false);
            case Item.SEARCH_BY_IMAGE:
                return context.getString(
                        getStringId(context, item),
                        TemplateUrlServiceFactory.getForProfile(profile)
                                .getDefaultSearchEngineTemplateUrl()
                                .getShortName());
            case Item.READ_LATER:
                return addOrRemoveNewLabel(context, item, null, showInProductHelp);
            case Item.OPEN_IN_EPHEMERAL_TAB:
                return addOrRemoveNewLabel(
                        context,
                        item,
                        ChromePreferenceKeys.CONTEXT_MENU_OPEN_IN_EPHEMERAL_TAB_CLICKED,
                        showInProductHelp);
            case Item.OPEN_IMAGE_IN_EPHEMERAL_TAB:
                return addOrRemoveNewLabel(
                        context,
                        item,
                        ChromePreferenceKeys.CONTEXT_MENU_OPEN_IMAGE_IN_EPHEMERAL_TAB_CLICKED,
                        showInProductHelp);
            case Item.SEARCH_WITH_GOOGLE_LENS:
                return addOrRemoveNewLabel(
                        context,
                        item,
                        ChromePreferenceKeys.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS_CLICKED,
                        showInProductHelp);
            case Item.SHOP_IMAGE_WITH_GOOGLE_LENS:
                return addOrRemoveNewLabel(
                        context,
                        item,
                        ChromePreferenceKeys.CONTEXT_MENU_SHOP_IMAGE_WITH_GOOGLE_LENS_CLICKED,
                        showInProductHelp);
            default:
                return context.getString(getStringId(context, item));
        }
    }

    /**
     * Modify the menu title by applying span attributes or removing the 'New' label if the menu
     * has already been selected before.
     */
    private static CharSequence addOrRemoveNewLabel(
            Context context, @Item int item, @Nullable String prefKey, boolean showNewLabel) {
        String menuTitle = context.getString(getStringId(context, item));
        if (!showNewLabel
                || (prefKey != null
                        && ChromeSharedPreferences.getInstance().readBoolean(prefKey, false))) {
            return SpanApplier.removeSpanText(menuTitle, new SpanInfo("<new>", "</new>"));
        }
        return SpanApplier.applySpans(
                menuTitle,
                new SpanInfo(
                        "<new>",
                        "</new>",
                        new SuperscriptSpan(),
                        new RelativeSizeSpan(0.75f),
                        new ForegroundColorSpan(
                                SemanticColorUtils.getDefaultTextColorAccent1(context))));
    }
}
