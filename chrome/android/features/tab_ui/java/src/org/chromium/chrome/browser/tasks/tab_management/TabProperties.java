// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.content.res.ColorStateList;
import android.util.Size;
import android.view.View.AccessibilityDelegate;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** List of properties to designate information about a single tab. */
public class TabProperties {
    /** IDs for possible types of UI in the tab list. */
    @IntDef({
        UiType.TAB,
        UiType.STRIP,
        UiType.MESSAGE,
        UiType.DIVIDER,
        UiType.NEW_TAB_TILE_DEPRECATED,
        UiType.LARGE_MESSAGE,
        UiType.CUSTOM_MESSAGE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface UiType {
        int TAB = 0;
        int STRIP = 1;
        int MESSAGE = 2;
        int DIVIDER = 3;
        int NEW_TAB_TILE_DEPRECATED = 4;
        int LARGE_MESSAGE = 5;
        int CUSTOM_MESSAGE = 6;
    }

    /** IDs for possible tab action states. */
    @IntDef({TabActionState.UNSET, TabActionState.SELECTABLE, TabActionState.CLOSABLE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabActionState {
        int UNSET = 0;
        int SELECTABLE = 1;
        int CLOSABLE = 2;
    }

    /** The {@link TabActionState} for the view, either CLOSABLE or SELECTABLE. */
    public static final WritableIntPropertyKey TAB_ACTION_STATE = new WritableIntPropertyKey();

    public static final WritableIntPropertyKey TAB_ID = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<TabListMediator.TabActionListener>
            TAB_SELECTED_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabListMediator.TabActionListener>
            TAB_ACTION_BUTTON_LISTENER = new WritableObjectPropertyKey<>();

    /**
     * Indicator that a {@link TabProperties.FAVICON_FETCHER} has completed fetching a favicon. Only
     * used by TabStrip for the {@link TabStripSnapshotter}.
     */
    public static final WritableBooleanPropertyKey FAVICON_FETCHED =
            new WritableBooleanPropertyKey();

    /** Property for lazily fetching favicons when required by an item in a UI. */
    public static final WritableObjectPropertyKey<TabListFaviconProvider.TabFaviconFetcher>
            FAVICON_FETCHER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabListMediator.ThumbnailFetcher>
            THUMBNAIL_FETCHER = new WritableObjectPropertyKey<>(true);

    public static final WritableObjectPropertyKey<Size> GRID_CARD_SIZE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabListMediator.IphProvider> IPH_PROVIDER =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<ColorStateList> CHECKED_DRAWABLE_STATE_LIST =
            new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey CARD_ANIMATION_STATUS = new WritableIntPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<TabListMediator.TabActionListener>
            SELECTABLE_TAB_CLICKED_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<SelectionDelegate<Integer>>
            TAB_SELECTION_DELEGATE = new WritableObjectPropertyKey<>();

    public static final PropertyModel.ReadableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.ReadableBooleanPropertyKey();

    public static final PropertyModel.ReadableIntPropertyKey SELECTED_TAB_BACKGROUND_DRAWABLE_ID =
            new PropertyModel.ReadableIntPropertyKey();

    public static final PropertyModel.ReadableIntPropertyKey TABSTRIP_FAVICON_BACKGROUND_COLOR_ID =
            new PropertyModel.ReadableIntPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList>
            SELECTABLE_TAB_ACTION_BUTTON_BACKGROUND =
                    new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList>
            SELECTABLE_TAB_ACTION_BUTTON_SELECTED_BACKGROUND =
                    new PropertyModel.WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> URL_DOMAIN =
            new WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<AccessibilityDelegate>
            ACCESSIBILITY_DELEGATE = new PropertyModel.WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabListMediator.ShoppingPersistedTabDataFetcher>
            SHOPPING_PERSISTED_TAB_DATA_FETCHER = new WritableObjectPropertyKey<>(true);

    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION_STRING =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> ACTION_BUTTON_DESCRIPTION_STRING =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey SHOULD_SHOW_PRICE_DROP_TOOLTIP =
            new WritableBooleanPropertyKey();

    public static final WritableIntPropertyKey QUICK_DELETE_ANIMATION_STATUS =
            new WritableIntPropertyKey();

    /** The {@link TabGroupColorId} for a tab group representation's color in TabListMode only. */
    public static final WritableIntPropertyKey TAB_GROUP_COLOR_ID = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<TabListMediator.TabGroupInfo> TAB_GROUP_INFO =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<TabListGroupMenuCoordinator.OnItemClickedCallback>
            ON_MENU_ITEM_CLICKED_CALLBACK = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS_TAB_GRID =
            new PropertyKey[] {
                TAB_ACTION_STATE,
                TAB_ID,
                TAB_SELECTED_LISTENER,
                TAB_ACTION_BUTTON_LISTENER,
                FAVICON_FETCHED,
                FAVICON_FETCHER,
                IS_SELECTED,
                IS_INCOGNITO,
                GRID_CARD_SIZE,
                THUMBNAIL_FETCHER,
                IPH_PROVIDER,
                TITLE,
                CHECKED_DRAWABLE_STATE_LIST,
                CARD_ALPHA,
                CARD_ANIMATION_STATUS,
                SELECTABLE_TAB_CLICKED_LISTENER,
                TAB_SELECTION_DELEGATE,
                SELECTED_TAB_BACKGROUND_DRAWABLE_ID,
                TABSTRIP_FAVICON_BACKGROUND_COLOR_ID,
                SELECTABLE_TAB_ACTION_BUTTON_BACKGROUND,
                SELECTABLE_TAB_ACTION_BUTTON_SELECTED_BACKGROUND,
                URL_DOMAIN,
                ACCESSIBILITY_DELEGATE,
                CARD_TYPE,
                CONTENT_DESCRIPTION_STRING,
                ACTION_BUTTON_DESCRIPTION_STRING,
                SHOPPING_PERSISTED_TAB_DATA_FETCHER,
                SHOULD_SHOW_PRICE_DROP_TOOLTIP,
                QUICK_DELETE_ANIMATION_STATUS,
                TAB_GROUP_COLOR_ID,
                TAB_GROUP_INFO,
                ON_MENU_ITEM_CLICKED_CALLBACK
            };

    public static final PropertyKey[] ALL_KEYS_TAB_STRIP =
            new PropertyKey[] {
                TAB_ID,
                TAB_SELECTED_LISTENER,
                TAB_ACTION_BUTTON_LISTENER,
                FAVICON_FETCHED,
                FAVICON_FETCHER,
                IS_SELECTED,
                TITLE,
                TABSTRIP_FAVICON_BACKGROUND_COLOR_ID,
                IS_INCOGNITO
            };
}
