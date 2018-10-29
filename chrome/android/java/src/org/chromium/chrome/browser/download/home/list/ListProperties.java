// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.components.offline_items_collection.VisualsCallback;

import java.util.List;

/**
 * The properties required to build a {@link ListItem} which contain two types of properties for the
 * download manager: (1) A set of properties that act directly on the list view itself. (2) A set of
 * properties that are effectively shared across all list items like callbacks.
 */
public interface ListProperties {
    /** A helper interface to support retrieving {@link OfflineItemVisuals} asynchronously. */
    @FunctionalInterface
    interface VisualsProvider {
        /**
         * @param item         The {@link OfflineItem} to get the {@link OfflineItemVisuals} for.
         * @param iconWidthPx  The desired width of the icon in pixels (not guaranteed).
         * @param iconHeightPx The desired height of the icon in pixels (not guaranteed).
         * @param callback     A {@link Callback} that will be notified on completion.
         * @return             A {@link Runnable} that can be used to cancel the request.
         */
        Runnable getVisuals(
                OfflineItem item, int iconWidthPx, int iconHeightPx, VisualsCallback callback);
    }

    /** Whether or not item animations should be enabled. */
    WritableBooleanPropertyKey ENABLE_ITEM_ANIMATIONS = new WritableBooleanPropertyKey();

    /** The callback for when a UI action should open a {@link OfflineItem}. */
    WritableObjectPropertyKey<Callback<OfflineItem>> CALLBACK_OPEN =
            new WritableObjectPropertyKey<>();

    /** The callback for when a UI action should pause a {@link OfflineItem}. */
    WritableObjectPropertyKey<Callback<OfflineItem>> CALLBACK_PAUSE =
            new WritableObjectPropertyKey<>();

    /** The callback for when a UI action should resume a {@link OfflineItem}. */
    WritableObjectPropertyKey<Callback<OfflineItem>> CALLBACK_RESUME =
            new WritableObjectPropertyKey<>();

    /** The callback for when a UI action should cancel a {@link OfflineItem}. */
    WritableObjectPropertyKey<Callback<OfflineItem>> CALLBACK_CANCEL =
            new WritableObjectPropertyKey<>();

    /** The callback for when a UI action should share a {@link OfflineItem}. */
    WritableObjectPropertyKey<Callback<OfflineItem>> CALLBACK_SHARE =
            new WritableObjectPropertyKey<>();

    /** The callback for when a UI action should share all selected {@link OfflineItem}s. */
    WritableObjectPropertyKey < Callback < List<OfflineItem>>> CALLBACK_SHARE_ALL =
            new WritableObjectPropertyKey<>();

    /** The callback for when a UI action should remove a {@link OfflineItem}. */
    WritableObjectPropertyKey<Callback<OfflineItem>> CALLBACK_REMOVE =
            new WritableObjectPropertyKey<>();

    /** The callback for when a UI action should remove all selected {@link OfflineItem}s. */
    WritableObjectPropertyKey < Callback < List<OfflineItem>>> CALLBACK_REMOVE_ALL =
            new WritableObjectPropertyKey<>();

    /** The provider to retrieve expensive assets for a {@link OfflineItem}. */
    WritableObjectPropertyKey<VisualsProvider> PROVIDER_VISUALS = new WritableObjectPropertyKey<>();

    /** The callback to trigger when a UI action selects or deselects a {@link ListItem}. */
    WritableObjectPropertyKey<Callback<ListItem>> CALLBACK_SELECTION =
            new WritableObjectPropertyKey<>();

    /** Whether or not selection mode is currently active. */
    WritableBooleanPropertyKey SELECTION_MODE_ACTIVE = new WritableBooleanPropertyKey();

    /**
     * The callback to trigger when a UI action starts general selection mode.  This is different
     * from {@link #CALLBACK_SELECTION} in that it should be triggered when the UI enters selection
     * mode without any particularly attached {@link ListItem}.
     */
    WritableObjectPropertyKey<Runnable> CALLBACK_START_SELECTION =
            new WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS = new PropertyKey[] {ENABLE_ITEM_ANIMATIONS, CALLBACK_OPEN,
            CALLBACK_PAUSE, CALLBACK_RESUME, CALLBACK_CANCEL, CALLBACK_SHARE, CALLBACK_SHARE_ALL,
            CALLBACK_REMOVE, CALLBACK_REMOVE_ALL, PROVIDER_VISUALS, CALLBACK_SELECTION,
            SELECTION_MODE_ACTIVE, CALLBACK_START_SELECTION};
}
