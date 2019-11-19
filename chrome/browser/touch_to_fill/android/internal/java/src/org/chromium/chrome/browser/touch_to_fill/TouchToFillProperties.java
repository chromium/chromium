// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Properties defined here reflect the visible state of the TouchToFill-components.
 */
class TouchToFillProperties {
    static final String FIELD_TRIAL_PARAM_SHOW_CONFIRMATION_BUTTON = "show_confirmation_button";

    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final PropertyModel
            .ReadableObjectPropertyKey<ListModel<MVCListAdapter.ListItem>> SHEET_ITEMS =
            new PropertyModel.ReadableObjectPropertyKey<>("sheet_items");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");
    static final PropertyModel.WritableObjectPropertyKey<Runnable> ON_CLICK_MANAGE =
            new PropertyModel.WritableObjectPropertyKey<>("on_click_manage");
    static PropertyModel createDefaultModel(Callback<Integer> handler) {
        return new PropertyModel.Builder(VISIBLE, SHEET_ITEMS, DISMISS_HANDLER, ON_CLICK_MANAGE)
                .with(VISIBLE, false)
                .with(SHEET_ITEMS, new ListModel<>())
                .with(DISMISS_HANDLER, handler)
                .build();
    }

    /**
     * Properties for a credential entry in TouchToFill sheet.
     */
    static class CredentialProperties {
        static final PropertyModel.WritableObjectPropertyKey<Bitmap> FAVICON =
                new PropertyModel.WritableObjectPropertyKey<>("favicon");
        static final PropertyModel.ReadableObjectPropertyKey<Credential> CREDENTIAL =
                new PropertyModel.ReadableObjectPropertyKey<>("credential");
        static final PropertyModel.ReadableObjectPropertyKey<String> FORMATTED_ORIGIN =
                new PropertyModel.ReadableObjectPropertyKey<>("formatted_url");
        static final PropertyModel
                .ReadableObjectPropertyKey<Callback<Credential>> ON_CLICK_LISTENER =
                new PropertyModel.ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {
                FAVICON, CREDENTIAL, FORMATTED_ORIGIN, ON_CLICK_LISTENER};

        private CredentialProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the header in the TouchToFill sheet.
     */
    static class HeaderProperties {
        static final PropertyModel.ReadableObjectPropertyKey<String> FORMATTED_URL =
                new PropertyModel.ReadableObjectPropertyKey<>("formatted_url");
        static final PropertyModel.ReadableBooleanPropertyKey ORIGIN_SECURE =
                new PropertyModel.ReadableBooleanPropertyKey("origin_secure");

        static final PropertyKey[] ALL_KEYS = {FORMATTED_URL, ORIGIN_SECURE};

        private HeaderProperties() {}
    }

    @IntDef({ItemType.HEADER, ItemType.CREDENTIAL, ItemType.FILL_BUTTON})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        /**
         * The header at the top of the touch to fill sheet.
         */
        int HEADER = 1;

        /**
         * A section containing a user's name and password.
         */
        int CREDENTIAL = 2;

        /**
         * The fill button at the end of the sheet that filling more obvious for one suggestion.
         */
        int FILL_BUTTON = 3;
    }

    /**
     * Returns the sheet item type for a given item.
     * @param item An {@link MVCListAdapter.ListItem}.
     * @return The {@link ItemType} of the given list item.
     */
    static @ItemType int getItemType(MVCListAdapter.ListItem item) {
        return item.type;
    }

    private TouchToFillProperties() {}
}
