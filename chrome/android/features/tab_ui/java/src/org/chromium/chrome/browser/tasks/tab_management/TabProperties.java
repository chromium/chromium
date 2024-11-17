// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.util.Size;
import android.view.View.AccessibilityDelegate;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.ShoppingPersistedTabDataFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionListener;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** List of properties to designate information about a single tab. */
public class TabProperties {
    /** IDs for possible types of UI in the tab list. */
    @IntDef({UiType.TAB, UiType.STRIP, UiType.MESSAGE, UiType.LARGE_MESSAGE, UiType.CUSTOM_MESSAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UiType {
        int TAB = 0;
        int STRIP = 1;
        int MESSAGE = 2;
        int LARGE_MESSAGE = 3;
        int CUSTOM_MESSAGE = 4;
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

    public static final ReadableBooleanPropertyKey IS_INCOGNITO = new ReadableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<TabActionListener> TAB_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabActionListener> TAB_LONG_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabActionButtonData> TAB_ACTION_BUTTON_DATA =
            new WritableObjectPropertyKey<>();

    // TODO(crbug.com/365973166): Move this to `TabStripProperties` when it is created.
    /**
     * Indicator that a {@link TabProperties.FAVICON_FETCHER} has completed fetching a favicon. Only
     * used by TabStrip for the {@link TabStripSnapshotter}.
     */
    public static final WritableBooleanPropertyKey FAVICON_FETCHED =
            new WritableBooleanPropertyKey();

    /** Property for lazily fetching favicons when required by an item in a UI. */
    public static final WritableObjectPropertyKey<TabListFaviconProvider.TabFaviconFetcher>
            FAVICON_FETCHER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<ThumbnailFetcher> THUMBNAIL_FETCHER =
            new WritableObjectPropertyKey<>(true);

    public static final WritableObjectPropertyKey<Size> GRID_CARD_SIZE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    public static final WritableIntPropertyKey CARD_ANIMATION_STATUS = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<SelectionDelegate<Integer>>
            TAB_SELECTION_DELEGATE = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> URL_DOMAIN =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AccessibilityDelegate> ACCESSIBILITY_DELEGATE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION_STRING =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> ACTION_BUTTON_DESCRIPTION_STRING =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<ShoppingPersistedTabDataFetcher>
            SHOPPING_PERSISTED_TAB_DATA_FETCHER = new WritableObjectPropertyKey<>(true);

    public static final WritableBooleanPropertyKey SHOULD_SHOW_PRICE_DROP_TOOLTIP =
            new WritableBooleanPropertyKey();

    public static final WritableIntPropertyKey QUICK_DELETE_ANIMATION_STATUS =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey VISIBILITY = new WritableIntPropertyKey();

    public static final WritableBooleanPropertyKey USE_SHRINK_CLOSE_ANIMATION =
            new WritableBooleanPropertyKey();

    // TODO(crbug.com/365972761): Move this to `TabGroupProperties` when it is created.
    /**
     * Provides a view for the tab group color. In list mode this is shown alongside the row. In
     * grid mode this replaces the favicon image view. {@code #destroy()} must be invoked on this
     * object before it is nulled out or the property model as a whole is removed.
     */
    public static final WritableObjectPropertyKey<TabGroupColorViewProvider>
            TAB_GROUP_COLOR_VIEW_PROVIDER = new WritableObjectPropertyKey<>();

    // TODO(crbug.com/365973166): Move this to `TabStripProperties` when it is created.
    public static final WritableBooleanPropertyKey HAS_NOTIFICATION_BUBBLE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<TabCardLabelData> TAB_CARD_LABEL_DATA =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);

    public static final PropertyKey[] ALL_KEYS_TAB_GRID =
            new PropertyKey[] {
                TAB_ACTION_STATE,
                TAB_ID,
                IS_INCOGNITO,
                TAB_CLICK_LISTENER,
                TAB_LONG_CLICK_LISTENER,
                TAB_ACTION_BUTTON_DATA,
                FAVICON_FETCHED,
                FAVICON_FETCHER,
                IS_SELECTED,
                GRID_CARD_SIZE,
                THUMBNAIL_FETCHER,
                TITLE,
                CARD_ALPHA,
                CARD_ANIMATION_STATUS,
                TAB_SELECTION_DELEGATE,
                URL_DOMAIN,
                ACCESSIBILITY_DELEGATE,
                CARD_TYPE,
                CONTENT_DESCRIPTION_STRING,
                ACTION_BUTTON_DESCRIPTION_STRING,
                SHOPPING_PERSISTED_TAB_DATA_FETCHER,
                SHOULD_SHOW_PRICE_DROP_TOOLTIP,
                QUICK_DELETE_ANIMATION_STATUS,
                TAB_GROUP_COLOR_VIEW_PROVIDER,
                VISIBILITY,
                USE_SHRINK_CLOSE_ANIMATION,
                HAS_NOTIFICATION_BUBBLE,
                TAB_CARD_LABEL_DATA,
            };

    public static final PropertyKey[] ALL_KEYS_TAB_STRIP =
            new PropertyKey[] {
                TAB_ID,
                IS_INCOGNITO,
                TAB_CLICK_LISTENER,
                TAB_ACTION_BUTTON_DATA,
                FAVICON_FETCHED,
                FAVICON_FETCHER,
                IS_SELECTED,
                TITLE,
                HAS_NOTIFICATION_BUBBLE
            };

    public static final WritableObjectPropertyKey[] TAB_ACTION_STATE_OBJECT_KEYS =
            new WritableObjectPropertyKey[] {
                TAB_ACTION_BUTTON_DATA,
                TAB_CLICK_LISTENER,
                TAB_LONG_CLICK_LISTENER,
                ACTION_BUTTON_DESCRIPTION_STRING,
                CONTENT_DESCRIPTION_STRING,
            };
}
