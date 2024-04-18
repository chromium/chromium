// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties defined here reflect the visible state of the AllPasswordsBottomSheet. */
class AllPasswordsBottomSheetProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");
    static final PropertyModel.ReadableObjectPropertyKey<String> ORIGIN =
            new PropertyModel.ReadableObjectPropertyKey<>("origin");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<String>> ON_QUERY_TEXT_CHANGE =
            new PropertyModel.ReadableObjectPropertyKey<>("on_query_text_change");

    static final PropertyKey[] ALL_KEYS = {VISIBLE, DISMISS_HANDLER, ORIGIN, ON_QUERY_TEXT_CHANGE};

    static PropertyModel createDefaultModel(
            String origin,
            Callback<Integer> dismissHandler,
            Callback<String> onSearchQueryChangeHandler) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(VISIBLE, false)
                .with(DISMISS_HANDLER, dismissHandler)
                .with(ORIGIN, origin)
                .with(ON_QUERY_TEXT_CHANGE, onSearchQueryChangeHandler)
                .build();
    }

    static class CredentialProperties {
        static final PropertyModel.ReadableObjectPropertyKey<Credential> CREDENTIAL =
                new PropertyModel.ReadableObjectPropertyKey<>("credential");
        static final PropertyModel.ReadableObjectPropertyKey<Callback<CredentialFillRequest>>
                ON_CLICK_LISTENER =
                        new PropertyModel.ReadableObjectPropertyKey<>("on_click_listener");
        static final PropertyModel.ReadableBooleanPropertyKey IS_PASSWORD_FIELD =
                new PropertyModel.ReadableBooleanPropertyKey("is_password_field");
        static final PropertyKey[] ALL_KEYS = {CREDENTIAL, ON_CLICK_LISTENER, IS_PASSWORD_FIELD};

        private CredentialProperties() {}

        static PropertyModel createCredentialModel(
                Credential credential,
                Callback<CredentialFillRequest> clickListener,
                boolean isPasswordField) {
            return new PropertyModel.Builder(
                            AllPasswordsBottomSheetProperties.CredentialProperties.ALL_KEYS)
                    .with(CREDENTIAL, credential)
                    .with(ON_CLICK_LISTENER, clickListener)
                    .with(IS_PASSWORD_FIELD, isPasswordField)
                    .build();
        }
    }

    @IntDef({ItemType.CREDENTIAL})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        /** A section containing username, password and origin. */
        int CREDENTIAL = 0;
    }

    /**
     * Returns the sheet item type for a given item.
     * @param item An {@link MVCListAdapter.ListItem}.
     * @return The {@link ItemType} of the given list item.
     */
    static @ItemType int getItemType(MVCListAdapter.ListItem item) {
        return item.type;
    }
}
