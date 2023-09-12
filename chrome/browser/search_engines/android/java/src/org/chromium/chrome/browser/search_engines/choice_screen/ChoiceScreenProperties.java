// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Data model for the state of the {@link ChoiceScreenView}.
 * The state is updated by the {@link ChoiceScreenMediator} and changes will be applied by the
 * {@link ChoiceScreenViewBinder}.
 */
class ChoiceScreenProperties {
    /** Data model for individual choice items from the {@link ChoiceScreenView}. */
    static class Item {
        /** Display name for a search engine choice item. */
        static final ReadableObjectPropertyKey<String> SHORT_NAME =
                new ReadableObjectPropertyKey<>("short_name");

        /** Whether this item is the one currently selected. */
        static final WritableBooleanPropertyKey IS_SELECTED =
                new WritableBooleanPropertyKey("is_selected");

        /** Callback to be executed when the user interacts with this item to select it. */
        static final ReadableObjectPropertyKey<Runnable> ON_CLICKED =
                new ReadableObjectPropertyKey<>("on_clicked");

        static final PropertyKey[] ALL_KEYS = {SHORT_NAME, IS_SELECTED, ON_CLICKED};

        static PropertyModel createPropertyModel(String shortName, Runnable onClicked) {
            return new PropertyModel.Builder(ALL_KEYS)
                    .with(SHORT_NAME, shortName)
                    .with(IS_SELECTED, false)
                    .with(ON_CLICKED, onClicked)
                    .build();
        }
    }

    /**
     * Value used as type identifier for choice items in the RecyclerView displayed in
     * {@link ChoiceScreenView}. This the only type of items supported.
     */
    static final int DEFAULT_TYPE = 0;

    /** The models representing the items displayed as options on the choice screen. */
    public static final WritableObjectPropertyKey<MVCListAdapter.ModelList> ITEM_MODELS =
            new WritableObjectPropertyKey<>();

    /**
     * Callback to be executed when the user wants to confirm their choice. Can be {@code null},
     * indicating that the action is not available and the button should be disabled.
     */
    static final WritableObjectPropertyKey<Runnable> ON_PRIMARY_CLICKED =
            new WritableObjectPropertyKey<>("on_primary_clicked");

    static final PropertyKey[] ALL_KEYS = {ITEM_MODELS, ON_PRIMARY_CLICKED};

    static PropertyModel createPropertyModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(ITEM_MODELS, new MVCListAdapter.ModelList())
                .with(ON_PRIMARY_CLICKED, null)
                .build();
    }
}
