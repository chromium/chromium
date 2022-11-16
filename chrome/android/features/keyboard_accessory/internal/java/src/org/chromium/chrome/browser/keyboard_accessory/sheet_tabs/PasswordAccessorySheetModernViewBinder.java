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

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.helper.FaviconHelper;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabViewBinder.ElementViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.utils.InsecureFillingDialogUtils;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.ListModel;

/**
 * This stateless class provides methods to bind a {@link ListModel<AccessorySheetDataPiece>}
 * to the {@link RecyclerView} used as view of a tab for the accessory sheet component.
 */
class PasswordAccessorySheetModernViewBinder {
    static ElementViewHolder create(ViewGroup parent, @AccessorySheetDataPiece.Type int viewType) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.PASSWORD_INFO:
                return new PasswordInfoViewHolder(parent);
            case AccessorySheetDataPiece.Type.TITLE:
                return new AccessorySheetTabViewBinder.TitleViewHolder(
                        parent, R.layout.keyboard_accessory_sheet_tab_title);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
            case AccessorySheetDataPiece.Type.OPTION_TOGGLE:
                return AccessorySheetTabViewBinder.create(parent, viewType);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    /**
     * Holds a TextView that represents a list entry.
     */
    static class PasswordInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.UserInfo, PasswordAccessoryInfoView> {
        String mFaviconRequestOrigin;

        PasswordInfoViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_password_info);
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
            FaviconHelper faviconHelper = FaviconHelper.create(view.getContext());
            view.setIconForBitmap(faviconHelper.getDefaultIcon(info.getOrigin()));
            faviconHelper.fetchFavicon(info.getOrigin(), d -> setIcon(view, info.getOrigin(), d));
        }

        private void setIcon(
                PasswordAccessoryInfoView view, String requestOrigin, Drawable drawable) {
            // Only set the icon if the origin hasn't changed since this view last requested an
            // icon. Since the Views are recycled, an old callback can target a new view.
            if (requestOrigin.equals(mFaviconRequestOrigin)) view.setIconForBitmap(drawable);
        }

        void bindChipView(ChipView chip, UserInfoField field, Context context) {
            chip.getPrimaryTextView().setTransformationMethod(
                    field.isObfuscated() ? new PasswordTransformationMethod() : null);
            chip.getPrimaryTextView().setText(field.getDisplayText());
            chip.getPrimaryTextView().setContentDescription(field.getA11yDescription());
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

    static void initializeView(RecyclerView view, AccessorySheetTabModel model) {
        view.setAdapter(PasswordAccessorySheetCoordinator.createModernAdapter(model));
        view.addItemDecoration(new DynamicInfoViewBottomSpacer(PasswordAccessoryInfoView.class));
    }
}
