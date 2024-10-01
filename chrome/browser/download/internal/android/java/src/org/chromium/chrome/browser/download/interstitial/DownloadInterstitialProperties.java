// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Extends the properties defined in {@link ListProperties} to facilitate the logic for an entire UI
 * containing a download ListItem.
 */
interface DownloadInterstitialProperties extends ListProperties {
    /**
     * Keeps track of the state of the DownloadInterstitial. This may be different to the state of
     * the offline item displayed within the UI.
     */
    @IntDef({
        State.UNKNOWN,
        State.IN_PROGRESS,
        State.SUCCESSFUL,
        State.CANCELLED,
        State.PAUSED,
        State.PENDING
    })
    @interface State {
        int UNKNOWN = 0;
        int IN_PROGRESS = 1;
        int SUCCESSFUL = 2;
        int CANCELLED = 3;
        int PAUSED = 4;
        int PENDING = 5;
    }

    WritableObjectPropertyKey<OfflineItem> DOWNLOAD_ITEM = new WritableObjectPropertyKey<>();

    WritableIntPropertyKey STATE = new WritableIntPropertyKey();

    WritableObjectPropertyKey<String> TITLE_TEXT = new WritableObjectPropertyKey<>();

    WritableBooleanPropertyKey PRIMARY_BUTTON_IS_VISIBLE = new WritableBooleanPropertyKey();

    WritableObjectPropertyKey<String> PRIMARY_BUTTON_TEXT = new WritableObjectPropertyKey<>();

    WritableObjectPropertyKey<Callback<OfflineItem>> PRIMARY_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    WritableBooleanPropertyKey SECONDARY_BUTTON_IS_VISIBLE = new WritableBooleanPropertyKey();

    WritableObjectPropertyKey<String> SECONDARY_BUTTON_TEXT = new WritableObjectPropertyKey<>();

    WritableObjectPropertyKey<Callback<OfflineItem>> SECONDARY_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    WritableObjectPropertyKey<Runnable> RELOAD_TAB = new WritableObjectPropertyKey<>();

    WritableBooleanPropertyKey PENDING_MESSAGE_IS_VISIBLE = new WritableBooleanPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ENABLE_ITEM_ANIMATIONS,
                CALLBACK_OPEN,
                CALLBACK_PAUSE,
                CALLBACK_RESUME,
                CALLBACK_CANCEL,
                CALLBACK_SHARE,
                CALLBACK_REMOVE,
                CALLBACK_RENAME,
                PROVIDER_VISUALS,
                PROVIDER_FAVICON,
                CALLBACK_SELECTION,
                SELECTION_MODE_ACTIVE,
                CALLBACK_PAGINATION_CLICK,
                CALLBACK_GROUP_PAGINATION_CLICK,
                DOWNLOAD_ITEM,
                STATE,
                TITLE_TEXT,
                PRIMARY_BUTTON_IS_VISIBLE,
                PRIMARY_BUTTON_TEXT,
                PRIMARY_BUTTON_CALLBACK,
                SECONDARY_BUTTON_IS_VISIBLE,
                SECONDARY_BUTTON_TEXT,
                SECONDARY_BUTTON_CALLBACK,
                RELOAD_TAB,
                PENDING_MESSAGE_IS_VISIBLE
            };
}
