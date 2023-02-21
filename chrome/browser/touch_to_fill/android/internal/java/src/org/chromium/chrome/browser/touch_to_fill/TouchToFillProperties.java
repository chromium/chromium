// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebAuthnCredential;
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
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final PropertyModel
            .ReadableObjectPropertyKey<ListModel<MVCListAdapter.ListItem>> SHEET_ITEMS =
            new PropertyModel.ReadableObjectPropertyKey<>("sheet_items");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");
    static PropertyModel createDefaultModel(Callback<Integer> handler) {
        return new PropertyModel.Builder(VISIBLE, SHEET_ITEMS, DISMISS_HANDLER)
                .with(VISIBLE, false)
                .with(SHEET_ITEMS, new ListModel<>())
                .with(DISMISS_HANDLER, handler)
                .build();
    }

    static class FaviconOrFallback {
        final String mUrl;
        final @Nullable Bitmap mIcon;
        final int mFallbackColor;
        final boolean mIsFallbackColorDefault;
        final int mIconType;
        final int mIconSize;

        FaviconOrFallback(String originUrl, @Nullable Bitmap icon, int fallbackColor,
                boolean isFallbackColorDefault, int iconType, int iconSize) {
            mUrl = originUrl;
            mIcon = icon;
            mFallbackColor = fallbackColor;
            mIsFallbackColorDefault = isFallbackColorDefault;
            mIconType = iconType;
            mIconSize = iconSize;
        }
    }

    /**
     * Properties for a credential entry in TouchToFill sheet.
     */
    static class CredentialProperties {
        static final PropertyModel
                .WritableObjectPropertyKey<FaviconOrFallback> FAVICON_OR_FALLBACK =
                new PropertyModel.WritableObjectPropertyKey<>("favicon");
        static final PropertyModel.ReadableObjectPropertyKey<Credential> CREDENTIAL =
                new PropertyModel.ReadableObjectPropertyKey<>("credential");
        static final PropertyModel.ReadableObjectPropertyKey<String> FORMATTED_ORIGIN =
                new PropertyModel.ReadableObjectPropertyKey<>("formatted_url");
        static final PropertyModel.ReadableObjectPropertyKey<Boolean> SHOW_SUBMIT_BUTTON =
                new PropertyModel.ReadableObjectPropertyKey<>("submit_credential");
        static final PropertyModel
                .ReadableObjectPropertyKey<Callback<Credential>> ON_CLICK_LISTENER =
                new PropertyModel.ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {FAVICON_OR_FALLBACK, CREDENTIAL, FORMATTED_ORIGIN,
                ON_CLICK_LISTENER, SHOW_SUBMIT_BUTTON};

        private CredentialProperties() {}
    }

    /**
     * Properties for a Web Authentication credential entry in TouchToFill sheet.
     */
    static class WebAuthnCredentialProperties {
        static final PropertyModel
                .WritableObjectPropertyKey<FaviconOrFallback> WEBAUTHN_FAVICON_OR_FALLBACK =
                new PropertyModel.WritableObjectPropertyKey<>("favicon");
        static final PropertyModel
                .ReadableObjectPropertyKey<WebAuthnCredential> WEBAUTHN_CREDENTIAL =
                new PropertyModel.ReadableObjectPropertyKey<>("webauthn_credential");
        static final PropertyModel.ReadableObjectPropertyKey<Boolean> SHOW_WEBAUTHN_SUBMIT_BUTTON =
                new PropertyModel.ReadableObjectPropertyKey<>("submit_credential");
        static final PropertyModel.ReadableObjectPropertyKey<Callback<WebAuthnCredential>>
                ON_WEBAUTHN_CLICK_LISTENER =
                new PropertyModel.ReadableObjectPropertyKey<>("on_webauthn_click_listener");

        static final PropertyKey[] ALL_KEYS = {WEBAUTHN_CREDENTIAL, WEBAUTHN_FAVICON_OR_FALLBACK,
                ON_WEBAUTHN_CLICK_LISTENER, SHOW_WEBAUTHN_SUBMIT_BUTTON};

        private WebAuthnCredentialProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the header in the TouchToFill sheet.
     */
    static class HeaderProperties {
        static final PropertyModel.ReadableBooleanPropertyKey SHOW_SUBMIT_SUBTITLE =
                new PropertyModel.ReadableBooleanPropertyKey("submit_credential");
        static final PropertyModel.ReadableObjectPropertyKey<String> FORMATTED_URL =
                new PropertyModel.ReadableObjectPropertyKey<>("formatted_url");
        static final PropertyModel.ReadableBooleanPropertyKey ORIGIN_SECURE =
                new PropertyModel.ReadableBooleanPropertyKey("origin_secure");
        static final PropertyModel.ReadableIntPropertyKey IMAGE_DRAWABLE_ID =
                new PropertyModel.ReadableIntPropertyKey("image_drawable_id");
        static final PropertyModel.ReadableObjectPropertyKey<String> TITLE =
                new PropertyModel.ReadableObjectPropertyKey<>("title");

        static final PropertyKey[] ALL_KEYS = {
                SHOW_SUBMIT_SUBTITLE, FORMATTED_URL, ORIGIN_SECURE, IMAGE_DRAWABLE_ID, TITLE};

        private HeaderProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the footer in the TouchToFill sheet for
     * payments.
     */
    static class FooterProperties {
        // TODO(crbug.com/1247698): Use ReadableBooleanPropertyKey.
        static final PropertyModel.WritableObjectPropertyKey<Runnable> ON_CLICK_MANAGE =
                new PropertyModel.WritableObjectPropertyKey<>("on_click_manage");
        static final PropertyModel.WritableObjectPropertyKey<String> MANAGE_BUTTON_TEXT =
                new PropertyModel.WritableObjectPropertyKey<>("manage_button_text");

        static final PropertyKey[] ALL_KEYS = {ON_CLICK_MANAGE, MANAGE_BUTTON_TEXT};

        private FooterProperties() {}
    }

    @IntDef({ItemType.HEADER, ItemType.CREDENTIAL, ItemType.WEBAUTHN_CREDENTIAL,
            ItemType.FILL_BUTTON, ItemType.FOOTER})
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
         * A section containing information about a WebAuthn credential.
         */
        int WEBAUTHN_CREDENTIAL = 3;

        /**
         * The fill button at the end of the sheet that filling more obvious for one suggestion.
         */
        int FILL_BUTTON = 4;

        /**
         * A footer section containing additional actions.
         */
        int FOOTER = 5;
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
