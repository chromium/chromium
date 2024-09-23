// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.chromium.components.embedder_support.util.UrlUtilities.stripScheme;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabViewBinder.ElementViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AddressAccessorySheetViewBinder.PlusAddressInfoViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.utils.InsecureFillingDialogUtils;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.ListModel;

/**
 * This stateless class provides methods to bind a {@link ListModel<AccessorySheetDataPiece>} to the
 * {@link RecyclerView} used as view of a tab for the accessory sheet component.
 */
class PasswordAccessorySheetViewBinder {
    /** Generic UI Configurations that help to transform specific model data. */
    static class UiConfiguration {
        /** Supports loading favicons for accessory data. */
        public FaviconHelper faviconHelper;
    }

    static ElementViewHolder create(
            ViewGroup parent,
            @AccessorySheetDataPiece.Type int viewType,
            UiConfiguration uiConfiguration) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.PLUS_ADDRESS_SECTION:
                return new PlusAddressInfoViewHolder(parent, uiConfiguration.faviconHelper);
            case AccessorySheetDataPiece.Type.PASSKEY_SECTION:
                return new PasskeyChipViewHolder(parent);
            case AccessorySheetDataPiece.Type.PASSWORD_INFO:
                return new PasswordInfoViewHolder(parent, uiConfiguration.faviconHelper);
            case AccessorySheetDataPiece.Type.TITLE:
                return new AccessorySheetTabViewBinder.TitleViewHolder(parent);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
            case AccessorySheetDataPiece.Type.OPTION_TOGGLE:
                return AccessorySheetTabViewBinder.create(parent, viewType);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    /** Holds a clickable {@link ChipView} that represents a Passkey. */
    static class PasskeyChipViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.PasskeySection, ViewGroup> {
        PasskeyChipViewHolder(ViewGroup parent) {
            super(parent, R.layout.password_accessory_passkey_chip);
        }

        @Override
        protected void bind(KeyboardAccessoryData.PasskeySection passkeySection, ViewGroup view) {
            ChipView chip = view.findViewById(R.id.keyboard_accessory_sheet_chip);
            chip.getPrimaryTextView().setText(passkeySection.getDisplayName());
            chip.getPrimaryTextView().setContentDescription(passkeySection.getDisplayName());
            chip.getSecondaryTextView().setText(R.string.password_accessory_passkey_label);
            chip.setOnClickListener((unused) -> passkeySection.triggerSelection());
        }
    }

    /** Holds a TextView that represents a list entry. */
    static class PasswordInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.UserInfo, PasswordAccessoryInfoView> {
        private final FaviconHelper mFaviconHelper;
        String mFaviconRequestOrigin;

        PasswordInfoViewHolder(ViewGroup parent, FaviconHelper faviconHelper) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_password_info);
            mFaviconHelper = faviconHelper;
        }

        @Override
        protected void bind(KeyboardAccessoryData.UserInfo info, PasswordAccessoryInfoView view) {
            bindChipView(view.getUsername(), info.getFields().get(0), view.getContext());
            bindChipView(view.getPassword(), info.getFields().get(1), view.getContext());

            view.getTitle().setVisibility(info.isExactMatch() ? View.GONE : View.VISIBLE);
            // Strip the trailing slash (for aesthetic reasons):
            view.getTitle().setText(stripScheme(info.getOrigin()).replaceFirst("/$", ""));

            // Set the default icon, then try to get a better one.
            mFaviconRequestOrigin = info.getOrigin(); // Save the origin for returning callback.
            view.setIconForBitmap(mFaviconHelper.getDefaultIcon(info.getOrigin()));
            mFaviconHelper.fetchFavicon(info.getOrigin(), d -> setIcon(view, info.getOrigin(), d));
        }

        private void setIcon(
                PasswordAccessoryInfoView view, String requestOrigin, Drawable drawable) {
            // Only set the icon if the origin hasn't changed since this view last requested an
            // icon. Since the Views are recycled, an old callback can target a new view.
            if (requestOrigin.equals(mFaviconRequestOrigin)) view.setIconForBitmap(drawable);
        }

        void bindChipView(ChipView chip, UserInfoField field, Context context) {
            chip.getPrimaryTextView()
                    .setTransformationMethod(
                            field.isObfuscated() ? new PasswordTransformationMethod() : null);
            chip.getPrimaryTextView().setText(field.getDisplayText());
            chip.getPrimaryTextView().setContentDescription(field.getA11yDescription());
            if (field.getIconId() != 0) {
                chip.setIcon(field.getIconId(), /* tintWithTextColor= */ true);
            }
            View.OnClickListener listener = null;
            if (field.isSelectable()) {
                listener = src -> field.triggerSelection();
            } else if (field.isObfuscated()) {
                listener = src -> InsecureFillingDialogUtils.showWarningDialog(context);
            }
            chip.setOnClickListener(listener);
            chip.setClickable(listener != null);
            chip.setEnabled(listener != null);
        }
    }

    static void initializeView(RecyclerView view, RecyclerView.Adapter adapter) {
        view.setAdapter(adapter);
        view.addItemDecoration(new DynamicInfoViewBottomSpacer(PasswordAccessoryInfoView.class));
    }
}
