// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.password_manager.PasswordManagerHelper.usesUnifiedPasswordManagerBranding;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.SHOW_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.SHOW_SUBMIT_SUBTITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.TITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.MANAGE_BUTTON_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ON_CLICK_MANAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.ON_WEBAUTHN_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.SHOW_WEBAUTHN_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.WEBAUTHN_CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.WEBAUTHN_FAVICON_OR_FALLBACK;
import static org.chromium.components.embedder_support.util.UrlUtilities.stripScheme;

import android.content.Context;
import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FaviconOrFallback;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebAuthnCredential;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * Provides functions that map {@link TouchToFillProperties} changes in a {@link PropertyModel} to
 * the suitable method in {@link TouchToFillView}.
 */
class TouchToFillViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link TouchToFillView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindTouchToFillView(
            PropertyModel model, TouchToFillView view, PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            boolean visibilityChangeSuccessful = view.setVisible(model.get(VISIBLE));
            if (!visibilityChangeSuccessful && model.get(VISIBLE)) {
                assert (model.get(DISMISS_HANDLER) != null);
                model.get(DISMISS_HANDLER).onResult(BottomSheetController.StateChangeReason.NONE);
            }
        } else if (propertyKey == ON_CLICK_MANAGE) {
            view.setOnManagePasswordClick(model.get(ON_CLICK_MANAGE));
        } else if (propertyKey == MANAGE_BUTTON_TEXT) {
            view.setManagePasswordText(model.get(MANAGE_BUTTON_TEXT));
        } else if (propertyKey == SHEET_ITEMS) {
            view.setSheetItemListAdapter(
                    new RecyclerViewAdapter<>(new SimpleRecyclerViewMcp<>(model.get(SHEET_ITEMS),
                                                      TouchToFillProperties::getItemType,
                                                      TouchToFillViewBinder::connectPropertyModel),
                            TouchToFillViewBinder::createViewHolder));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new View inside the ListView inside the TouchToFillView.
     * @param parent The parent {@link ViewGroup} of the new item.
     * @param itemType The type of View to create.
     */
    private static TouchToFillViewHolder createViewHolder(
            ViewGroup parent, @ItemType int itemType) {
        switch (itemType) {
            case ItemType.HEADER:
                return new TouchToFillViewHolder(parent,
                        usesUnifiedPasswordManagerBranding()
                                ? R.layout.touch_to_fill_header_item_modern
                                : R.layout.touch_to_fill_header_item,
                        TouchToFillViewBinder::bindHeaderView);
            case ItemType.CREDENTIAL:
                return new TouchToFillViewHolder(parent,
                        usesUnifiedPasswordManagerBranding()
                                ? R.layout.touch_to_fill_credential_item_modern
                                : R.layout.touch_to_fill_credential_item,
                        TouchToFillViewBinder::bindCredentialView);
            case ItemType.WEBAUTHN_CREDENTIAL:
                return new TouchToFillViewHolder(parent,
                        usesUnifiedPasswordManagerBranding()
                                ? R.layout.touch_to_fill_webauthn_credential_item_modern
                                : R.layout.touch_to_fill_webauthn_credential_item,
                        TouchToFillViewBinder::bindWebAuthnCredentialView);
            case ItemType.FILL_BUTTON:
                return new TouchToFillViewHolder(parent,
                        usesUnifiedPasswordManagerBranding()
                                ? R.layout.touch_to_fill_fill_button_modern
                                : R.layout.touch_to_fill_fill_button,
                        TouchToFillViewBinder::bindFillButtonView);
        }
        assert false : "Cannot create view for ItemType: " + itemType;
        return null;
    }

    /**
     * This method creates a model change processor for each recycler view item when it is created.
     * @param holder A {@link TouchToFillViewHolder} holding the view and view binder for the MCP.
     * @param item A {@link MVCListAdapter.ListItem} holding the {@link PropertyModel} for the MCP.
     */
    private static void connectPropertyModel(
            TouchToFillViewHolder holder, MVCListAdapter.ListItem item) {
        holder.setupModelChangeProcessor(item.model);
    }

    /**
     * Called whenever a credential is bound to this view holder. Please note that this method
     * might be called on the same list entry repeatedly, so make sure to always set a default
     * for unused fields.
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    private static void bindCredentialView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        Credential credential = model.get(CREDENTIAL);
        if (propertyKey == FAVICON_OR_FALLBACK) {
            ImageView imageView = view.findViewById(R.id.favicon);
            FaviconOrFallback data = model.get(FAVICON_OR_FALLBACK);
            imageView.setImageDrawable(FaviconUtils.getIconDrawableWithoutFilter(data.mIcon,
                    data.mUrl, data.mFallbackColor,
                    FaviconUtils.createCircularIconGenerator(view.getContext()),
                    view.getResources(), data.mIconSize));
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener(
                    clickedView -> { model.get(ON_CLICK_LISTENER).onResult(credential); });
        } else if (propertyKey == FORMATTED_ORIGIN) {
            TextView pslOriginText = view.findViewById(R.id.credential_origin);
            pslOriginText.setText(model.get(FORMATTED_ORIGIN));
            pslOriginText.setVisibility(credential.isExactMatch() ? View.GONE : View.VISIBLE);
        } else if (propertyKey == CREDENTIAL) {
            TextView pslOriginText = view.findViewById(R.id.credential_origin);
            String formattedOrigin = stripScheme(credential.getOriginUrl());
            formattedOrigin =
                    formattedOrigin.replaceFirst("/$", ""); // Strip possibly trailing slash.
            pslOriginText.setText(formattedOrigin);
            pslOriginText.setVisibility(credential.isExactMatch() ? View.GONE : View.VISIBLE);

            TextView usernameText = view.findViewById(R.id.username);
            usernameText.setText(credential.getFormattedUsername());

            TextView passwordText = view.findViewById(R.id.password);
            passwordText.setText(credential.getPassword());
            passwordText.setTransformationMethod(new PasswordTransformationMethod());

            String contentDescription = view.getContext().getString(
                    R.string.touch_to_fill_password_credential_accessibility_description,
                    credential.getFormattedUsername());
            view.setContentDescription(contentDescription);
        } else if (propertyKey == SHOW_SUBMIT_BUTTON) {
            // Whether Touch To Fill should auto-submit a form doesn't affect the credentials list.
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Called whenever a WebAuthn credential is bound to this view holder.
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    private static void bindWebAuthnCredentialView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        WebAuthnCredential credential = model.get(WEBAUTHN_CREDENTIAL);
        if (propertyKey == ON_WEBAUTHN_CLICK_LISTENER) {
            view.setOnClickListener(
                    clickedView -> model.get(ON_WEBAUTHN_CLICK_LISTENER).onResult(credential));
        } else if (propertyKey == WEBAUTHN_FAVICON_OR_FALLBACK) {
            ImageView imageView = view.findViewById(R.id.favicon);
            FaviconOrFallback data = model.get(WEBAUTHN_FAVICON_OR_FALLBACK);
            imageView.setImageDrawable(FaviconUtils.getIconDrawableWithoutFilter(data.mIcon,
                    data.mUrl, data.mFallbackColor,
                    FaviconUtils.createCircularIconGenerator(view.getContext()),
                    view.getResources(), data.mIconSize));
        } else if (propertyKey == WEBAUTHN_CREDENTIAL) {
            TextView usernameText = view.findViewById(R.id.username);
            usernameText.setText(credential.getUsername());
            TextView descriptionText = view.findViewById(R.id.webauthn_credential_context);
            descriptionText.setText(R.string.touch_to_fill_sheet_webauthn_credential_context);
            String contentDescription = view.getContext().getString(
                    R.string.touch_to_fill_passkey_credential_accessibility_description,
                    credential.getUsername());
            view.setContentDescription(contentDescription);
        } else if (propertyKey == SHOW_WEBAUTHN_SUBMIT_BUTTON) {
            // Ignore.
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Called whenever a fill button for a single credential is bound to this view holder.
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    private static void bindFillButtonView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ON_CLICK_LISTENER) {
            Credential credential = model.get(CREDENTIAL);
            view.setOnClickListener(
                    clickedView -> { model.get(ON_CLICK_LISTENER).onResult(credential); });
        } else if (propertyKey == ON_WEBAUTHN_CLICK_LISTENER) {
            WebAuthnCredential webauthn_credential = model.get(WEBAUTHN_CREDENTIAL);
            view.setOnClickListener(clickedView -> {
                model.get(ON_WEBAUTHN_CLICK_LISTENER).onResult(webauthn_credential);
            });
        } else if (propertyKey == SHOW_SUBMIT_BUTTON) {
            TextView buttonTitleText = view.findViewById(R.id.touch_to_fill_button_title);
            buttonTitleText.setText(view.getContext().getString(model.get(SHOW_SUBMIT_BUTTON)
                            ? R.string.touch_to_fill_signin
                            : R.string.touch_to_fill_continue));
        } else if (propertyKey == SHOW_WEBAUTHN_SUBMIT_BUTTON) {
            TextView buttonTitleText = view.findViewById(R.id.touch_to_fill_button_title);
            buttonTitleText.setText(view.getContext().getString(
                    model.get(SHOW_WEBAUTHN_SUBMIT_BUTTON) ? R.string.touch_to_fill_signin
                                                           : R.string.touch_to_fill_continue));
        } else if (propertyKey == FAVICON_OR_FALLBACK || propertyKey == FORMATTED_ORIGIN
                || propertyKey == CREDENTIAL || propertyKey == WEBAUTHN_CREDENTIAL
                || propertyKey == WEBAUTHN_FAVICON_OR_FALLBACK) {
            // Credential properties don't affect the button.
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Helper function to infer the subtitle of Touch To Fill sheet.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param context The {@link Context} of the header to update.
     * @return The title of Touch To Fill sheet.
     */
    private static String getSubtitle(PropertyModel model, Context context) {
        if (model.get(SHOW_SUBMIT_SUBTITLE)) {
            return String.format(
                    context.getString(model.get(ORIGIN_SECURE)
                                    ? R.string.touch_to_fill_sheet_subtitle_submission
                                    : R.string.touch_to_fill_sheet_subtitle_insecure_submission),
                    model.get(FORMATTED_URL));
        } else {
            return model.get(ORIGIN_SECURE)
                    ? model.get(FORMATTED_URL)
                    : String.format(
                            context.getString(R.string.touch_to_fill_sheet_subtitle_not_secure),
                            model.get(FORMATTED_URL));
        }
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    private static void bindHeaderView(PropertyModel model, View view, PropertyKey key) {
        if (key == SHOW_SUBMIT_SUBTITLE || key == TITLE || key == FORMATTED_URL
                || key == ORIGIN_SECURE || key == IMAGE_DRAWABLE_ID) {
            TextView sheetTitleText = view.findViewById(R.id.touch_to_fill_sheet_title);
            sheetTitleText.setText(model.get(TITLE));

            TextView sheetSubtitleText = view.findViewById(R.id.touch_to_fill_sheet_subtitle);
            sheetSubtitleText.setText(getSubtitle(model, view.getContext()));

            ImageView sheetHeaderImage = view.findViewById(R.id.touch_to_fill_sheet_header_image);
            sheetHeaderImage.setImageDrawable(AppCompatResources.getDrawable(view.getContext(),
                    usesUnifiedPasswordManagerBranding()
                            ? PasswordManagerResourceProviderFactory.create()
                                      .getPasswordManagerIcon()
                            : model.get(IMAGE_DRAWABLE_ID)));
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private TouchToFillViewBinder() {}
}
