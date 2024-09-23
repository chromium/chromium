// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebauthnCredential;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties defined here reflect the visible state of the TouchToFill-components. */
class TouchToFillProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final PropertyModel.ReadableObjectPropertyKey<ListModel<MVCListAdapter.ListItem>>
            SHEET_ITEMS = new PropertyModel.ReadableObjectPropertyKey<>("sheet_items");
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

        FaviconOrFallback(
                String originUrl,
                @Nullable Bitmap icon,
                int fallbackColor,
                boolean isFallbackColorDefault,
                int iconType,
                int iconSize) {
            mUrl = originUrl;
            mIcon = icon;
            mFallbackColor = fallbackColor;
            mIsFallbackColorDefault = isFallbackColorDefault;
            mIconType = iconType;
            mIconSize = iconSize;
        }
    }

    /** Properties for a credential entry in TouchToFill sheet. */
    static class CredentialProperties {
        static final PropertyModel.WritableObjectPropertyKey<FaviconOrFallback>
                FAVICON_OR_FALLBACK = new PropertyModel.WritableObjectPropertyKey<>("favicon");
        static final PropertyModel.ReadableObjectPropertyKey<Credential> CREDENTIAL =
                new PropertyModel.ReadableObjectPropertyKey<>("credential");
        static final PropertyModel.ReadableObjectPropertyKey<String> FORMATTED_ORIGIN =
                new PropertyModel.ReadableObjectPropertyKey<>("formatted_url");
        static final PropertyModel.ReadableObjectPropertyKey<Boolean> SHOW_SUBMIT_BUTTON =
                new PropertyModel.ReadableObjectPropertyKey<>("submit_credential");
        static final PropertyModel.ReadableObjectPropertyKey<Callback<Credential>>
                ON_CLICK_LISTENER =
                        new PropertyModel.ReadableObjectPropertyKey<>("on_click_listener");
        static final PropertyModel.ReadableObjectPropertyKey<FillableItemCollectionInfo>
                ITEM_COLLECTION_INFO =
                        new PropertyModel.ReadableObjectPropertyKey<>("item_collection_info");

        static final PropertyKey[] ALL_KEYS = {
            FAVICON_OR_FALLBACK,
            CREDENTIAL,
            FORMATTED_ORIGIN,
            ON_CLICK_LISTENER,
            SHOW_SUBMIT_BUTTON,
            ITEM_COLLECTION_INFO
        };

        private CredentialProperties() {}
    }

    /** Properties for a Web Authentication credential entry in TouchToFill sheet. */
    static class WebAuthnCredentialProperties {
        static final PropertyModel.WritableObjectPropertyKey<FaviconOrFallback>
                WEBAUTHN_FAVICON_OR_FALLBACK =
                        new PropertyModel.WritableObjectPropertyKey<>("favicon");
        static final PropertyModel.ReadableObjectPropertyKey<WebauthnCredential>
                WEBAUTHN_CREDENTIAL =
                        new PropertyModel.ReadableObjectPropertyKey<>("webauthn_credential");
        static final PropertyModel.ReadableObjectPropertyKey<Boolean> SHOW_WEBAUTHN_SUBMIT_BUTTON =
                new PropertyModel.ReadableObjectPropertyKey<>("submit_credential");
        static final PropertyModel.ReadableObjectPropertyKey<Callback<WebauthnCredential>>
                ON_WEBAUTHN_CLICK_LISTENER =
                        new PropertyModel.ReadableObjectPropertyKey<>("on_webauthn_click_listener");
        static final PropertyModel.ReadableObjectPropertyKey<FillableItemCollectionInfo>
                WEBAUTHN_ITEM_COLLECTION_INFO =
                        new PropertyModel.ReadableObjectPropertyKey<>("item_collection_info");
        static final PropertyKey[] ALL_KEYS = {
            WEBAUTHN_CREDENTIAL,
            WEBAUTHN_FAVICON_OR_FALLBACK,
            ON_WEBAUTHN_CLICK_LISTENER,
            SHOW_WEBAUTHN_SUBMIT_BUTTON,
            WEBAUTHN_ITEM_COLLECTION_INFO
        };

        private WebAuthnCredentialProperties() {}
    }

    static class MorePasskeysProperties {
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_CLICK =
                new PropertyModel.ReadableObjectPropertyKey<>("more_passkeys_on_click");
        static final PropertyModel.ReadableObjectPropertyKey<String> TITLE =
                new PropertyModel.ReadableObjectPropertyKey<>("more_passkeys_title");

        static final PropertyKey[] ALL_KEYS = {ON_CLICK, TITLE};

        private MorePasskeysProperties() {}
    }

    /** Properties defined here reflect the visible state of the header in the TouchToFill sheet. */
    static class HeaderProperties {
        static final PropertyModel.ReadableIntPropertyKey IMAGE_DRAWABLE_ID =
                new PropertyModel.ReadableIntPropertyKey("image_drawable_id");
        static final PropertyModel.ReadableObjectPropertyKey<String> TITLE =
                new PropertyModel.ReadableObjectPropertyKey<>("title");
        static final PropertyModel.ReadableObjectPropertyKey<String> SUBTITLE =
                new PropertyModel.ReadableObjectPropertyKey<>("subtitle");
        static final PropertyModel.WritableObjectPropertyKey<Drawable> AVATAR =
                new PropertyModel.WritableObjectPropertyKey<>("avatar");

        static final PropertyKey[] ALL_KEYS = {IMAGE_DRAWABLE_ID, TITLE, SUBTITLE, AVATAR};

        private HeaderProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the footer in the TouchToFill sheet for
     * payments.
     */
    static class FooterProperties {
        // TODO(crbug.com/40196949): Use ReadableBooleanPropertyKey.
        static final PropertyModel.WritableObjectPropertyKey<Runnable> ON_CLICK_MANAGE =
                new PropertyModel.WritableObjectPropertyKey<>("on_click_manage");
        static final PropertyModel.WritableObjectPropertyKey<String> MANAGE_BUTTON_TEXT =
                new PropertyModel.WritableObjectPropertyKey<>("manage_button_text");
        static final PropertyModel.WritableObjectPropertyKey<Runnable> ON_CLICK_HYBRID =
                new PropertyModel.WritableObjectPropertyKey<>("on_click_hybrid");
        static final PropertyModel.WritableBooleanPropertyKey SHOW_HYBRID =
                new PropertyModel.WritableBooleanPropertyKey("show_hybrid");

        static final PropertyKey[] ALL_KEYS = {
            ON_CLICK_MANAGE, MANAGE_BUTTON_TEXT, ON_CLICK_HYBRID, SHOW_HYBRID
        };

        private FooterProperties() {}
    }

    @IntDef({
        ItemType.HEADER,
        ItemType.CREDENTIAL,
        ItemType.WEBAUTHN_CREDENTIAL,
        ItemType.MORE_PASSKEYS,
        ItemType.FILL_BUTTON,
        ItemType.FOOTER
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        /** The header at the top of the touch to fill sheet. */
        int HEADER = 1;

        /** A section containing a user's name and password. */
        int CREDENTIAL = 2;

        /** A section containing information about a WebAuthn credential. */
        int WEBAUTHN_CREDENTIAL = 3;

        /** A section that opens Android Credential Manager API. */
        int MORE_PASSKEYS = 4;

        /** The fill button at the end of the sheet that filling more obvious for one suggestion. */
        int FILL_BUTTON = 5;

        /** A footer section containing additional actions. */
        int FOOTER = 6;
    }

    /**
     * Returns the sheet item type for a given item.
     *
     * @param item An {@link MVCListAdapter.ListItem}.
     * @return The {@link ItemType} of the given list item.
     */
    static @ItemType int getItemType(MVCListAdapter.ListItem item) {
        return item.type;
    }

    private TouchToFillProperties() {}
}
