// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.password_check.helper.PasswordCheckIconHelper.FaviconOrFallback;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties defined here reflect the visible state of the PasswordCheck subcomponents. */
class PasswordCheckProperties {
    static final PropertyModel.ReadableObjectPropertyKey<ListModel<MVCListAdapter.ListItem>> ITEMS =
            new PropertyModel.ReadableObjectPropertyKey<>("items");
    static final PropertyModel.WritableObjectPropertyKey<
                    PasswordCheckDeletionDialogFragment.Handler>
            DELETION_CONFIRMATION_HANDLER =
                    new PropertyModel.WritableObjectPropertyKey<>("deletion_confirmation_handler");
    static final PropertyModel.WritableObjectPropertyKey<String> DELETION_ORIGIN =
            new PropertyModel.WritableObjectPropertyKey<>("deletion_origin");
    static final PropertyModel.WritableObjectPropertyKey<CompromisedCredential> VIEW_CREDENTIAL =
            new PropertyModel.WritableObjectPropertyKey<>("view_credential");
    static final PropertyModel.WritableObjectPropertyKey<PasswordCheckViewDialogFragment.Handler>
            VIEW_DIALOG_HANDLER =
                    new PropertyModel.WritableObjectPropertyKey<>("view_dialog_handler");

    static final PropertyKey[] ALL_KEYS = {
        ITEMS, DELETION_CONFIRMATION_HANDLER, DELETION_ORIGIN, VIEW_CREDENTIAL, VIEW_DIALOG_HANDLER
    };

    static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS).with(ITEMS, new ListModel<>()).build();
    }

    /** Properties for a compromised credential entry. */
    static class CompromisedCredentialProperties {
        static final PropertyModel.ReadableObjectPropertyKey<CompromisedCredential>
                COMPROMISED_CREDENTIAL =
                        new PropertyModel.ReadableObjectPropertyKey<>("compromised_credential");
        static final PropertyModel.ReadableObjectPropertyKey<
                        PasswordCheckCoordinator.CredentialEventHandler>
                CREDENTIAL_HANDLER =
                        new PropertyModel.ReadableObjectPropertyKey<>("credential_handler");
        static final PropertyModel.ReadableBooleanPropertyKey HAS_MANUAL_CHANGE_BUTTON =
                new PropertyModel.ReadableBooleanPropertyKey("has_change_button");
        static final PropertyModel.WritableObjectPropertyKey<FaviconOrFallback>
                FAVICON_OR_FALLBACK = new PropertyModel.WritableObjectPropertyKey<>("favicon");

        static final PropertyKey[] ALL_KEYS = {
            COMPROMISED_CREDENTIAL,
            CREDENTIAL_HANDLER,
            HAS_MANUAL_CHANGE_BUTTON,
            FAVICON_OR_FALLBACK
        };

        private CompromisedCredentialProperties() {}
    }

    /** Properties defining the header (banner logo and status line). */
    static class HeaderProperties {
        static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, Integer>>
                CHECK_PROGRESS = new PropertyModel.WritableObjectPropertyKey<>("check_progress");
        static final PropertyModel.WritableIntPropertyKey CHECK_STATUS =
                new PropertyModel.WritableIntPropertyKey("check_status");
        static final PropertyModel.WritableObjectPropertyKey<Long> CHECK_TIMESTAMP =
                new PropertyModel.WritableObjectPropertyKey<>("check_timestamp");
        static final PropertyModel.WritableObjectPropertyKey<Integer>
                COMPROMISED_CREDENTIALS_COUNT =
                        new PropertyModel.WritableObjectPropertyKey<>(
                                "compromised_credentials_count");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable>
                LAUNCH_ACCOUNT_CHECKUP_ACTION =
                        new PropertyModel.ReadableObjectPropertyKey<>(
                                "launch_account_checkup_action");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> RESTART_BUTTON_ACTION =
                new PropertyModel.ReadableObjectPropertyKey<>("restart_button_action");
        static final PropertyModel.WritableBooleanPropertyKey SHOW_CHECK_SUBTITLE =
                new PropertyModel.WritableBooleanPropertyKey("show_check_subtitle");

        static final PropertyKey[] ALL_KEYS = {
            CHECK_PROGRESS,
            CHECK_STATUS,
            CHECK_TIMESTAMP,
            COMPROMISED_CREDENTIALS_COUNT,
            LAUNCH_ACCOUNT_CHECKUP_ACTION,
            RESTART_BUTTON_ACTION,
            SHOW_CHECK_SUBTITLE
        };

        static final Pair<Integer, Integer> UNKNOWN_PROGRESS = new Pair<>(-1, -1);

        private HeaderProperties() {}
    }

    @IntDef({ItemType.HEADER, ItemType.COMPROMISED_CREDENTIAL})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        /** The header at the top of the password check settings screen. */
        int HEADER = 1;

        /** A section containing a user's name and password. */
        int COMPROMISED_CREDENTIAL = 2;
    }

    /**
     * Returns the sheet item type for a given item.
     * @param item An {@link MVCListAdapter.ListItem}.
     * @return The {@link ItemType} of the given list item.
     */
    static @ItemType int getItemType(MVCListAdapter.ListItem item) {
        return item.type;
    }

    private PasswordCheckProperties() {}
}
