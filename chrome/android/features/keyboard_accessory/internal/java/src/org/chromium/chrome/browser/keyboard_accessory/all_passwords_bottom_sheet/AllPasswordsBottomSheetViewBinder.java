// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.CredentialProperties.IS_PASSWORD_FIELD;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.ON_QUERY_TEXT_CHANGE;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.ORIGIN;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.ItemType;
import org.chromium.chrome.browser.keyboard_accessory.utils.InsecureFillingDialogUtils;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Provides functions that map {@link AllPasswordsBottomSheetProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link AllPasswordsBottomSheetView}.
 */
class AllPasswordsBottomSheetViewBinder {
    /** Generic UI Configurations that help to transform specific model data. */
    static class UiConfiguration {
        /** Supports loading favicons for accessory data. */
        public FaviconHelper faviconHelper;
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link AllPasswordsBottomSheetView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindAllPasswordsBottomSheet(
            PropertyModel model, AllPasswordsBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == ORIGIN) {
            view.setWarning(
                    formatWarningForOrigin(
                            view.getContentView().getResources(), model.get(ORIGIN)));
        } else if (propertyKey == ON_QUERY_TEXT_CHANGE) {
            view.setSearchQueryChangeHandler(model.get(ON_QUERY_TEXT_CHANGE));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * This method creates a model change processor for each recycler view item when it is created.
     *
     * @param holder A {@link AllPasswordsBottomSheetViewHolder} holding the view and view binder
     *     for the MCP.
     * @param item A {@link MVCListAdapter.ListItem} holding the {@link PropertyModel} for the MCP.
     */
    static void connectPropertyModel(
            AllPasswordsBottomSheetViewHolder holder, MVCListAdapter.ListItem item) {
        holder.setupModelChangeProcessor(item.model);
    }

    /**
     * Factory used to create a new View inside the ListView inside the AllPasswordsBottomSheetView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     * @param itemType The type of View to create.
     * @param uiConfiguration Supports additional generic UI Configuration.
     */
    static AllPasswordsBottomSheetViewHolder createViewHolder(
            ViewGroup parent, @ItemType int itemType, UiConfiguration uiConfiguration) {
        switch (itemType) {
            case ItemType.CREDENTIAL:
                return new AllPasswordsBottomSheetViewHolder(
                        parent,
                        R.layout.keyboard_accessory_sheet_tab_password_info,
                        (model, view, propertyKey) ->
                                bindCredentialView(
                                        model, view, propertyKey, uiConfiguration.faviconHelper));
        }
        assert false : "Cannot create view for ItemType: " + itemType;
        return null;
    }

    /**
     * Called whenever a credential is bound to this view holder. Please note that this method might
     * be called on the same list entry repeatedly, so make sure to always set a default for unused
     * fields.
     *
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     * @param faviconHelper Supports fetching favicons for the view.
     */
    private static void bindCredentialView(
            PropertyModel model, View view, PropertyKey propertyKey, FaviconHelper faviconHelper) {
        Credential credential = model.get(CREDENTIAL);
        ChipView usernameChip = view.findViewById(R.id.suggestion_text);
        ChipView passwordChip = view.findViewById(R.id.password_text);

        if (propertyKey == ON_CLICK_LISTENER || propertyKey == IS_PASSWORD_FIELD) {
            boolean isPasswordField = model.get(IS_PASSWORD_FIELD);
            Callback<CredentialFillRequest> callback = model.get(ON_CLICK_LISTENER);
            updateUsernameChipListener(usernameChip, credential, callback);
            updatePasswordChipListener(passwordChip, credential, isPasswordField, callback);

            updateChipViewVisibility(usernameChip);
            updateChipViewVisibility(passwordChip);
        } else if (propertyKey == CREDENTIAL) {
            TextView passwordTitleView = view.findViewById(R.id.password_info_title);
            String title =
                    credential.isAndroidCredential()
                            ? credential.getAppDisplayName()
                            : UrlFormatter.formatUrlForSecurityDisplay(
                                    new GURL(credential.getOriginUrl()),
                                    SchemeDisplay.OMIT_CRYPTOGRAPHIC);
            passwordTitleView.setText(title);

            usernameChip.getPrimaryTextView().setText(credential.getFormattedUsername());

            boolean isEmptyPassword = credential.getPassword().isEmpty();
            if (!isEmptyPassword) {
                passwordChip
                        .getPrimaryTextView()
                        .setTransformationMethod(new PasswordTransformationMethod());
            }
            passwordChip
                    .getPrimaryTextView()
                    .setText(
                            isEmptyPassword
                                    ? view.getContext()
                                            .getString(
                                                    R.string.all_passwords_bottom_sheet_no_password)
                                    : credential.getPassword());

            // Set the default icon, then try to get a better one.
            ImageView iconView = view.findViewById(R.id.favicon);
            setIconForBitmap(
                    iconView,
                    faviconHelper.getDefaultIcon(
                            credential.isAndroidCredential()
                                    ? credential.getAppDisplayName()
                                    : credential.getOriginUrl()));

            if (!credential.isAndroidCredential()) {
                faviconHelper.fetchFavicon(
                        credential.getOriginUrl(), icon -> setIconForBitmap(iconView, icon));
            }

            if (credential.isPlusAddressUsername()) {
                usernameChip.setIcon(
                        R.drawable.ic_plus_addresses_logo_24dp, /* tintWithTextColor= */ true);
            } else {
                usernameChip.setIcon(ChipView.INVALID_ICON_ID, /* tintWithTextColor= */ false);
            }
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private static void setIconForBitmap(ImageView iconView, @Nullable Drawable icon) {
        final int kIconSize =
                iconView.getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.keyboard_accessory_suggestion_icon_size);
        if (icon != null) icon.setBounds(0, 0, kIconSize, kIconSize);
        iconView.setImageDrawable(icon);
    }

    private static String formatWarningForOrigin(Resources resources, String origin) {
        String formattedOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(
                        new GURL(origin), SchemeDisplay.OMIT_CRYPTOGRAPHIC);
        return String.format(
                resources.getString(R.string.all_passwords_bottom_sheet_subtitle), formattedOrigin);
    }

    private static void updatePasswordChipListener(
            View view,
            Credential credential,
            boolean isPasswordField,
            Callback<CredentialFillRequest> callback) {
        if (isPasswordField) {
            view.setOnClickListener(
                    src -> callback.onResult(new CredentialFillRequest(credential, true)));
            return;
        }
        view.setOnClickListener(
                src -> InsecureFillingDialogUtils.showWarningDialog(view.getContext()));
    }

    private static void updateUsernameChipListener(
            View view, Credential credential, Callback<CredentialFillRequest> callback) {
        if (credential.getUsername().isEmpty()) {
            view.setOnClickListener(null);
            return;
        }
        view.setOnClickListener(
                src -> callback.onResult(new CredentialFillRequest(credential, false)));
    }

    private static void updateChipViewVisibility(ChipView chip) {
        chip.setEnabled(chip.hasOnClickListeners());
        chip.setClickable(chip.hasOnClickListeners());
    }
}
