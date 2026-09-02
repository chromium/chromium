// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.CredentialProperties.FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.CredentialProperties.ITEM_COLLECTION_INFO;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.CredentialProperties.SHOW_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.FooterProperties.MANAGE_BUTTON_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.FooterProperties.ON_CLICK_HYBRID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.FooterProperties.ON_CLICK_MANAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.FooterProperties.SHOW_HYBRID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.HeaderProperties.AVATAR;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.HeaderProperties.SUBTITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.HeaderProperties.TITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.VISIBLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.WebAuthnCredentialProperties.ON_WEBAUTHN_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.WebAuthnCredentialProperties.SHOW_WEBAUTHN_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.WebAuthnCredentialProperties.WEBAUTHN_CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.WebAuthnCredentialProperties.WEBAUTHN_FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.WebAuthnCredentialProperties.WEBAUTHN_ITEM_COLLECTION_INFO;

import android.content.Context;
import android.text.Html;
import android.text.TextUtils;
import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.FaviconOrFallback;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.MorePasskeysProperties;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebauthnCredential;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * Provides functions that map {@link TouchToFillPasswordManagerProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link TouchToFillPasswordManagerView}.
 */
@NullMarked
class TouchToFillPasswordManagerViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link TouchToFillPasswordManagerView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindTouchToFillView(
            PropertyModel model, TouchToFillPasswordManagerView view, PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            Callback<Integer> dismissHandler = model.get(DISMISS_HANDLER);
            assumeNonNull(dismissHandler);
            view.setDismissHandler(dismissHandler);
        } else if (propertyKey == VISIBLE) {
            boolean visibilityChangeSuccessful = view.setVisible(model.get(VISIBLE));
            if (!visibilityChangeSuccessful
                    && model.get(VISIBLE)
                    && !ChromeFeatureList.isEnabled(
                            ChromeFeatureList.PASSWORD_FORM_GROUPED_AFFILIATIONS)) {
                Callback<Integer> dismissHandler = model.get(DISMISS_HANDLER);
                assert dismissHandler != null;
                dismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
            }
        } else if (propertyKey == SHEET_ITEMS) {
            view.setSheetItemListAdapter(
                    new RecyclerViewAdapter<>(
                            new SimpleRecyclerViewMcp<>(
                                    model.get(SHEET_ITEMS),
                                    TouchToFillPasswordManagerProperties::getItemType,
                                    TouchToFillPasswordManagerViewBinder::connectPropertyModel),
                            TouchToFillPasswordManagerViewBinder::createViewHolder));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new View inside the ListView inside the
     * TouchToFillPasswordManagerView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     * @param itemType The type of View to create.
     */
    private static TouchToFillPasswordManagerViewHolder createViewHolder(
            ViewGroup parent, @ItemType int itemType) {
        switch (itemType) {
            case ItemType.HEADER:
                return new TouchToFillPasswordManagerViewHolder(
                        parent,
                        R.layout.touch_to_fill_password_manager_header_item,
                        TouchToFillPasswordManagerViewBinder::bindHeaderView);
            case ItemType.CREDENTIAL:
                return new TouchToFillPasswordManagerViewHolder(
                        parent,
                        R.layout.touch_to_fill_password_manager_list_item,
                        TouchToFillPasswordManagerViewBinder::bindCredentialView);
            case ItemType.WEBAUTHN_CREDENTIAL:
                return new TouchToFillPasswordManagerViewHolder(
                        parent,
                        R.layout.touch_to_fill_password_manager_list_item,
                        TouchToFillPasswordManagerViewBinder::bindWebAuthnCredentialView);
            case ItemType.MORE_PASSKEYS:
                return new TouchToFillPasswordManagerViewHolder(
                        parent,
                        R.layout.touch_to_fill_password_manager_more_passkeys_item,
                        TouchToFillPasswordManagerViewBinder::bindMorePasskeysView);
            case ItemType.FILL_BUTTON:
                return new TouchToFillPasswordManagerViewHolder(
                        parent,
                        R.layout.touch_to_fill_fill_button,
                        TouchToFillPasswordManagerViewBinder::bindFillButtonView);
            case ItemType.FOOTER:
                return new TouchToFillPasswordManagerViewHolder(
                        parent,
                        R.layout.touch_to_fill_password_manager_footer_item,
                        TouchToFillPasswordManagerViewBinder::bindFooterView);
        }
        throw new IllegalArgumentException("Cannot create view for ItemType: " + itemType);
    }

    /**
     * This method creates a model change processor for each recycler view item when it is created.
     *
     * @param holder A {@link TouchToFillPasswordManagerViewHolder} holding the view and view binder
     *     for the MCP.
     * @param item A {@link MVCListAdapter.ListItem} holding the {@link PropertyModel} for the MCP.
     */
    private static void connectPropertyModel(
            TouchToFillPasswordManagerViewHolder holder, MVCListAdapter.ListItem item) {
        holder.setupModelChangeProcessor(item.model);
    }

    /**
     * Called whenever a credential is bound to this view holder. Please note that this method might
     * be called on the same list entry repeatedly, so make sure to always set a default for unused
     * fields.
     *
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    private static void bindCredentialView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        Credential credential = model.get(CREDENTIAL);
        assumeNonNull(credential);
        if (propertyKey == FAVICON_OR_FALLBACK) {
            assert !credential.isBackupCredential()
                    : "Recovery credentials should not have "
                            + "favicons, but should instead display the history icon.";
            ImageView imageView = view.findViewById(R.id.favicon);
            assumeNonNull(imageView);
            FaviconOrFallback data = model.get(FAVICON_OR_FALLBACK);
            assumeNonNull(data);
            imageView.setImageDrawable(
                    FaviconUtils.getIconDrawableWithoutFilter(
                            data.mIcon,
                            data.mUrl,
                            data.mFallbackColor,
                            FaviconUtils.createCircularIconGenerator(view.getContext()),
                            view.getResources(),
                            data.mIconSize));
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener(
                    clickedView -> {
                        Callback<Credential> clickListener = model.get(ON_CLICK_LISTENER);
                        assumeNonNull(clickListener);
                        clickListener.onResult(credential);
                    });
        } else if (propertyKey == CREDENTIAL || propertyKey == ITEM_COLLECTION_INFO) {
            TextView pslOriginText = view.findViewById(R.id.credential_origin);
            assumeNonNull(pslOriginText);
            pslOriginText.setText(credential.getDisplayName());
            pslOriginText.setVisibility(
                    credential.isExactMatch() || credential.isBackupCredential()
                            ? View.GONE
                            : View.VISIBLE);

            TextView recoveryLabel = view.findViewById(R.id.recovery_password_label);
            assumeNonNull(recoveryLabel);
            recoveryLabel.setText(
                    view.getContext().getString(R.string.touch_to_fill_recovery_password_label));
            recoveryLabel.setVisibility(credential.isBackupCredential() ? View.VISIBLE : View.GONE);

            if (credential.isBackupCredential()) {
                ImageView imageView = view.findViewById(R.id.favicon);
                assumeNonNull(imageView);
                imageView.setImageResource(R.drawable.ic_history_24dp);
            }

            TextView usernameText = view.findViewById(R.id.username);
            assumeNonNull(usernameText);
            usernameText.setText(credential.getFormattedUsername());

            TextView passwordText = view.findViewById(R.id.password_or_context);
            assumeNonNull(passwordText);
            passwordText.setText(credential.getPassword());
            passwordText.setTransformationMethod(new PasswordTransformationMethod());

            view.setContentDescription(
                    createContentDescription(
                            credential, model.get(ITEM_COLLECTION_INFO), view.getContext()));
        } else if (propertyKey == SHOW_SUBMIT_BUTTON) {
            // Whether Touch To Fill should auto-submit a form doesn't affect the credentials list.
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private static String createContentDescription(
            Credential credential,
            @Nullable FillableItemCollectionInfo collectionInfo,
            Context context) {
        String label = createLabelForContentDescription(credential, context);

        String contentDescription =
                collectionInfo == null
                        ? label
                        : context.getString(
                                R.string.touch_to_fill_a11y_item_collection_info,
                                label,
                                collectionInfo.getPosition(),
                                collectionInfo.getTotal());
        return contentDescription;
    }

    private static String createLabelForContentDescription(Credential credential, Context context) {
        if (TextUtils.isEmpty(credential.getDisplayName())) {
            int stringId =
                    credential.isBackupCredential()
                            ? R.string
                                    .touch_to_fill_recovery_password_credential_accessibility_description
                            : R.string.touch_to_fill_password_credential_accessibility_description;
            return context.getString(stringId, credential.getFormattedUsername());
        }

        int stringId =
                credential.isBackupCredential()
                        ? R.string
                                .touch_to_fill_recovery_password_credential_accessibility_description_with_url
                        : R.string
                                .touch_to_fill_password_credential_accessibility_description_with_url;
        return context.getString(
                stringId, credential.getFormattedUsername(), credential.getDisplayName());
    }

    /**
     * Called whenever a WebAuthn credential is bound to this view holder.
     *
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    private static void bindWebAuthnCredentialView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        WebauthnCredential credential = model.get(WEBAUTHN_CREDENTIAL);
        assumeNonNull(credential);
        View credentialOrigin = view.findViewById(R.id.credential_origin);
        assumeNonNull(credentialOrigin);
        credentialOrigin.setVisibility(View.GONE);
        if (propertyKey == ON_WEBAUTHN_CLICK_LISTENER) {
            view.setOnClickListener(
                    clickedView -> {
                        Callback<WebauthnCredential> clickListener =
                                model.get(ON_WEBAUTHN_CLICK_LISTENER);
                        assumeNonNull(clickListener);
                        clickListener.onResult(credential);
                    });
        } else if (propertyKey == WEBAUTHN_FAVICON_OR_FALLBACK) {
            ImageView imageView = view.findViewById(R.id.favicon);
            assumeNonNull(imageView);
            FaviconOrFallback data = model.get(WEBAUTHN_FAVICON_OR_FALLBACK);
            assumeNonNull(data);
            imageView.setImageDrawable(
                    FaviconUtils.getIconDrawableWithoutFilter(
                            data.mIcon,
                            data.mUrl,
                            data.mFallbackColor,
                            FaviconUtils.createCircularIconGenerator(view.getContext()),
                            view.getResources(),
                            data.mIconSize));
        } else if (propertyKey == WEBAUTHN_CREDENTIAL
                || propertyKey == WEBAUTHN_ITEM_COLLECTION_INFO) {
            TextView usernameText = view.findViewById(R.id.username);
            assumeNonNull(usernameText);
            usernameText.setText(credential.getUsername());
            TextView descriptionText = view.findViewById(R.id.password_or_context);
            assumeNonNull(descriptionText);

            descriptionText.setText(R.string.touch_to_fill_sheet_passkey_credential_context);

            String label =
                    view.getContext()
                            .getString(
                                    R.string
                                            .touch_to_fill_passkey_credential_accessibility_description,
                                    credential.getUsername());
            FillableItemCollectionInfo collectionInfo = model.get(WEBAUTHN_ITEM_COLLECTION_INFO);
            String contentDescription =
                    collectionInfo == null
                            ? label
                            : view.getContext()
                                    .getString(
                                            R.string.touch_to_fill_a11y_item_collection_info,
                                            label,
                                            collectionInfo.getPosition(),
                                            collectionInfo.getTotal());
            view.setContentDescription(contentDescription);
        } else if (propertyKey == SHOW_WEBAUTHN_SUBMIT_BUTTON) {
            // Ignore.
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Called whenever an action button to use more passkeys is bound to this view holder.
     *
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    private static void bindMorePasskeysView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == MorePasskeysProperties.ON_CLICK) {
            view.setOnClickListener(
                    clickedView -> {
                        Runnable onClick = model.get(MorePasskeysProperties.ON_CLICK);
                        assumeNonNull(onClick);
                        onClick.run();
                    });
        } else if (propertyKey == MorePasskeysProperties.TITLE) {
            TextView labelText = view.findViewById(R.id.more_passkeys_label);
            assumeNonNull(labelText);
            labelText.setText(model.get(MorePasskeysProperties.TITLE));
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    /**
     * Called whenever a fill button for a single credential is bound to this view holder.
     *
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    private static void bindFillButtonView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ON_CLICK_LISTENER) {
            Credential credential = model.get(CREDENTIAL);
            assumeNonNull(credential);
            view.setOnClickListener(
                    clickedView -> {
                        Callback<Credential> clickListener = model.get(ON_CLICK_LISTENER);
                        assumeNonNull(clickListener);
                        clickListener.onResult(credential);
                    });
        } else if (propertyKey == ON_WEBAUTHN_CLICK_LISTENER) {
            WebauthnCredential webauthnCredential = model.get(WEBAUTHN_CREDENTIAL);
            assumeNonNull(webauthnCredential);
            view.setOnClickListener(
                    clickedView -> {
                        Callback<WebauthnCredential> clickListener =
                                model.get(ON_WEBAUTHN_CLICK_LISTENER);
                        assumeNonNull(clickListener);
                        clickListener.onResult(webauthnCredential);
                    });
        } else if (propertyKey == SHOW_SUBMIT_BUTTON) {
            TextView buttonTitleText = view.findViewById(R.id.touch_to_fill_button_title);
            assumeNonNull(buttonTitleText);
            buttonTitleText.setText(
                    view.getContext()
                            .getString(
                                    model.get(SHOW_SUBMIT_BUTTON)
                                            ? R.string.touch_to_fill_signin
                                            : R.string.touch_to_fill_continue));
        } else if (propertyKey == SHOW_WEBAUTHN_SUBMIT_BUTTON) {
            TextView buttonTitleText = view.findViewById(R.id.touch_to_fill_button_title);
            assumeNonNull(buttonTitleText);
            buttonTitleText.setText(
                    view.getContext()
                            .getString(
                                    model.get(SHOW_WEBAUTHN_SUBMIT_BUTTON)
                                            ? R.string.touch_to_fill_signin
                                            : R.string.touch_to_fill_continue));
        } else if (propertyKey == FAVICON_OR_FALLBACK
                || propertyKey == CREDENTIAL
                || propertyKey == WEBAUTHN_CREDENTIAL
                || propertyKey == WEBAUTHN_FAVICON_OR_FALLBACK
                || propertyKey == ITEM_COLLECTION_INFO
                || propertyKey == WEBAUTHN_ITEM_COLLECTION_INFO) {
            // Credential properties don't affect the button.
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    private static void bindHeaderView(PropertyModel model, View view, PropertyKey key) {
        if (key == SUBTITLE || key == TITLE || key == IMAGE_DRAWABLE_ID) {
            TextView sheetTitleText = view.findViewById(R.id.touch_to_fill_sheet_title);
            assumeNonNull(sheetTitleText);
            sheetTitleText.setText(model.get(TITLE));

            TextView sheetSubtitleText = view.findViewById(R.id.touch_to_fill_sheet_subtitle);
            assumeNonNull(sheetSubtitleText);
            sheetSubtitleText.setText(
                    Html.fromHtml(model.get(SUBTITLE), Html.FROM_HTML_MODE_LEGACY));

            ImageView sheetHeaderImage = view.findViewById(R.id.touch_to_fill_sheet_header_image);
            assumeNonNull(sheetHeaderImage);
            sheetHeaderImage.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(IMAGE_DRAWABLE_ID)));
        } else if (key == AVATAR) {
            ImageView sheetHeaderAvatar = view.findViewById(R.id.touch_to_fill_sheet_header_avatar);
            assumeNonNull(sheetHeaderAvatar);
            if (model.get(AVATAR) == null) {
                sheetHeaderAvatar.setVisibility(View.INVISIBLE);
            } else {
                sheetHeaderAvatar.setVisibility(View.VISIBLE);
                sheetHeaderAvatar.setImageDrawable(model.get(AVATAR));
            }
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    private static void bindFooterView(PropertyModel model, View view, PropertyKey key) {
        if (key == ON_CLICK_MANAGE) {
            View manageButton = view.findViewById(R.id.touch_to_fill_sheet_manage_passwords);
            assumeNonNull(manageButton);
            manageButton.setOnClickListener(
                    (v) -> {
                        Runnable onClickManage = model.get(ON_CLICK_MANAGE);
                        assumeNonNull(onClickManage);
                        onClickManage.run();
                    });
        } else if (key == ON_CLICK_HYBRID) {
            View hybridButton =
                    view.findViewById(R.id.touch_to_fill_sheet_use_passkeys_other_device);
            assumeNonNull(hybridButton);
            hybridButton.setOnClickListener(
                    (v) -> {
                        Runnable onClickHybrid = model.get(ON_CLICK_HYBRID);
                        assumeNonNull(onClickHybrid);
                        onClickHybrid.run();
                    });
        } else if (key == SHOW_HYBRID) {
            View hybridButton =
                    view.findViewById(R.id.touch_to_fill_sheet_use_passkeys_other_device);
            assumeNonNull(hybridButton);
            hybridButton.setVisibility(model.get(SHOW_HYBRID) ? View.VISIBLE : View.GONE);
        } else if (key == MANAGE_BUTTON_TEXT) {
            TextView managePasswordsView =
                    view.findViewById(R.id.touch_to_fill_sheet_manage_passwords);
            assumeNonNull(managePasswordsView);
            managePasswordsView.setText(model.get(MANAGE_BUTTON_TEXT));
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private TouchToFillPasswordManagerViewBinder() {}
}
