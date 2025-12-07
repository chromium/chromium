// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** List of all predefined Context Menu Items available in Chrome. */
@NullMarked
class ChromeContextMenuItem {
    @IntDef({
        Item.OPEN_IN_NEW_CHROME_TAB,
        Item.OPEN_IN_CHROME_INCOGNITO_TAB,
        Item.OPEN_IN_BROWSER_ID,
        Item.OPEN_IN_NEW_TAB,
        Item.OPEN_IN_INCOGNITO_TAB,
        Item.OPEN_IN_INCOGNITO_WINDOW,
        Item.OPEN_IN_OTHER_WINDOW,
        Item.OPEN_IN_NEW_WINDOW,
        Item.SHOW_INTEREST_IN_ELEMENT,
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
        Item.PICTURE_IN_PICTURE,
        Item.OPEN_IN_CHROME,
        Item.OPEN_IN_NEW_TAB_IN_GROUP,
        Item.SHARE_HIGHLIGHT,
        Item.REMOVE_HIGHLIGHT,
        Item.LEARN_MORE,
        Item.SAVE_PAGE,
        Item.SHARE_PAGE,
        Item.PRINT_PAGE,
        Item.VIEW_PAGE_SOURCE,
        Item.INSPECT_ELEMENT,
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
        int OPEN_IN_INCOGNITO_WINDOW = 6;
        int OPEN_IN_OTHER_WINDOW = 7;
        int OPEN_IN_NEW_WINDOW = 8;
        int SHOW_INTEREST_IN_ELEMENT = 9;
        int OPEN_IN_EPHEMERAL_TAB = 10;
        int COPY_LINK_ADDRESS = 11;
        int COPY_LINK_TEXT = 12;
        int SAVE_LINK_AS = 13;
        int SHARE_LINK = 14;
        int DIRECT_SHARE_LINK = 15;
        int READ_LATER = 16;
        // Image Group
        int LOAD_ORIGINAL_IMAGE = 17;
        int SAVE_IMAGE = 18;
        int OPEN_IMAGE = 19;
        int OPEN_IMAGE_IN_NEW_TAB = 20;
        int OPEN_IMAGE_IN_EPHEMERAL_TAB = 21;
        int COPY_IMAGE = 22;
        int SEARCH_BY_IMAGE = 23;
        int SEARCH_WITH_GOOGLE_LENS = 24;
        int SHOP_IMAGE_WITH_GOOGLE_LENS = 25;
        int SHARE_IMAGE = 26;
        int DIRECT_SHARE_IMAGE = 27;
        // Message Group
        int CALL = 28;
        int SEND_MESSAGE = 29;
        int ADD_TO_CONTACTS = 30;
        int COPY = 31;
        // Video Group
        int SAVE_VIDEO = 32;
        int PICTURE_IN_PICTURE = 33;
        // Other
        int OPEN_IN_CHROME = 34;
        // Shared Highlighting options
        int SHARE_HIGHLIGHT = 35;
        int REMOVE_HIGHLIGHT = 36;
        int LEARN_MORE = 37;
        // Page Group
        int SAVE_PAGE = 38;
        int SHARE_PAGE = 39;
        int PRINT_PAGE = 40;
        // Developer Group
        int VIEW_PAGE_SOURCE = 41;
        int INSPECT_ELEMENT = 42;
        // ALWAYS UPDATE!
        int NUM_ENTRIES = 43;
    }

    /** Mapping from {@link Item} to the ID found in the ids.xml. */
    private static final int[] MENU_IDS = {
        R.id.contextmenu_open_in_new_chrome_tab, // Item.OPEN_IN_NEW_CHROME_TAB
        R.id.contextmenu_open_in_chrome_incognito_tab, // Item.OPEN_IN_CHROME_INCOGNITO_TAB
        R.id.contextmenu_open_in_browser_id, // Item.OPEN_IN_BROWSER_ID
        R.id.contextmenu_open_in_new_tab, // Item.OPEN_IN_NEW_TAB
        R.id.contextmenu_open_in_new_tab_in_group, // Item.OPEN_IN_NEW_TAB_IN_GROUP
        R.id.contextmenu_open_in_incognito_tab, // Item.OPEN_IN_INCOGNITO_TAB
        R.id.contextmenu_open_in_incognito_window, // Item.OPEN_IN_INCOGNITO_WINDOW
        R.id.contextmenu_open_in_other_window, // Item.OPEN_IN_OTHER_WINDOW
        R.id.contextmenu_open_in_new_window, // Item.OPEN_IN_NEW_WINDOW
        R.id.contextmenu_show_interest_in_element, // Item.SHOW_INTEREST_IN_ELEMENT
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
        R.id.contextmenu_picture_in_picture, // Item.PICTURE_IN_PICTURE
        R.id.contextmenu_open_in_chrome, // Item.OPEN_IN_CHROME
        R.id.contextmenu_share_highlight, // Item.SHARE_HIGHLIGHT
        R.id.contextmenu_remove_highlight, // Item.REMOVE_HIGHLIGHT
        R.id.contextmenu_learn_more, // Item.LEARN_MORE
        R.id.contextmenu_save_page, // Item.SAVE_PAGE
        R.id.contextmenu_share_page, // Item.SHARE_PAGE
        R.id.contextmenu_print_page, // Item.PRINT_PAGE
        R.id.contextmenu_view_page_source, // Item.VIEW_PAGE_SOURCE
        R.id.contextmenu_inspect_element, // Item.INSPECT_ELEMENT
    };

    /** Mapping from {@link Item} to the ID of the string that describes the action of the item. */
    private static final int[] STRING_IDS = {
        R.string.contextmenu_open_in_new_chrome_tab, // Item.OPEN_IN_NEW_CHROME_TAB:
        R.string.contextmenu_open_in_chrome_incognito_tab, // Item.OPEN_IN_CHROME_INCOGNITO_TAB:
        0, // Item.OPEN_IN_BROWSER_ID is not handled by this mapping.
        R.string.contextmenu_open_in_new_tab, // Item.OPEN_IN_NEW_TAB:
        R.string.contextmenu_open_in_new_tab_group, // Item.OPEN_IN_NEW_TAB_IN_GROUP
        R.string.contextmenu_open_in_incognito_tab, // Item.OPEN_IN_INCOGNITO_TAB:
        R.string.contextmenu_open_in_incognito_window, // Item.OPEN_IN_INCOGNITO_WINDOW:
        R.string.contextmenu_open_in_other_window, // Item.OPEN_IN_OTHER_WINDOW:
        R.string.contextmenu_open_in_new_window, // Item.OPEN_IN_NEW_WINDOW:
        R.string.contextmenu_show_interest_in_element, // Item.SHOW_INTEREST_IN_ELEMENT
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
        0, // Item.PICTURE_IN_PICTURE is not handled by this mapping.
        R.string.menu_open_in_chrome, // Item.OPEN_IN_CHROME:
        R.string.contextmenu_share_highlight, // Item.SHARE_HIGHLIGHT
        R.string.contextmenu_remove_highlight, // Item.REMOVE_HIGHLIGHT
        R.string.contextmenu_learn_more, // Item.LEARN_MORE
        R.string.contextmenu_save_page, // Item.SAVE_PAGE
        R.string.contextmenu_share_page, // Item.SHARE_PAGE
        R.string.contextmenu_print_page, // Item.PRINT_PAGE
        R.string.contextmenu_view_page_source, // Item.VIEW_PAGE_SOURCE
        R.string.contextmenu_inspect_element, // Item.INSPECT_ELEMENT
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
     *
     * @param item #Item Item ID.
     * @return Returns the string that describes the action of the item.
     */
    private static @StringRes int getStringId(@Item int item) {
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
                TemplateUrl templateUrl =
                        TemplateUrlServiceFactory.getForProfile(profile)
                                .getDefaultSearchEngineTemplateUrl();
                assumeNonNull(templateUrl);
                return context.getString(getStringId(item), templateUrl.getShortName());
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
            case Item.OPEN_IN_CHROME_INCOGNITO_TAB:
                if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                    return context.getString(R.string.contextmenu_open_in_incognito_window);
                }
                break;
            case Item.OPEN_IN_NEW_CHROME_TAB:
                if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                    return context.getString(R.string.contextmenu_open_in_chrome_window);
                }
                break;
            default:
                return context.getString(getStringId(item));
        }
        return context.getString(getStringId(item));
    }

    /**
     * Modify the menu title by applying span attributes or removing the 'New' label if the menu has
     * already been selected before.
     */
    private static CharSequence addOrRemoveNewLabel(
            Context context, @Item int item, @Nullable String prefKey, boolean showNewLabel) {
        String menuTitle = context.getString(getStringId(item));
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
