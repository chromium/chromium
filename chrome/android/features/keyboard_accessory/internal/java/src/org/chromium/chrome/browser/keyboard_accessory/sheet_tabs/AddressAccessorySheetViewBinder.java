// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabViewBinder.ElementViewHolder;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.ListModel;

/**
 * This stateless class provides methods to bind a {@link ListModel<AccessorySheetDataPiece>} to the
 * {@link RecyclerView} used as view of a tab for the address accessory sheet component.
 */
class AddressAccessorySheetViewBinder {
    static ElementViewHolder create(
            ViewGroup parent,
            @AccessorySheetDataPiece.Type int viewType,
            FaviconHelper faviconHelper) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.TITLE:
                return new AccessorySheetTabViewBinder.TitleViewHolder(parent);
            case AccessorySheetDataPiece.Type.PLUS_ADDRESS_SECTION:
                return new PlusAddressInfoViewHolder(parent, faviconHelper);
            case AccessorySheetDataPiece.Type.ADDRESS_INFO:
                return new AddressInfoViewHolder(parent);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
                return AccessorySheetTabViewBinder.create(parent, viewType);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    static class PlusAddressInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.PlusAddressInfo, PlusAddressInfoView> {
        private final FaviconHelper mFaviconHelper;

        PlusAddressInfoViewHolder(ViewGroup parent, FaviconHelper faviconHelper) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_plus_address_info);
            mFaviconHelper = faviconHelper;
        }

        @Override
        protected void bind(
                KeyboardAccessoryData.PlusAddressInfo section, PlusAddressInfoView view) {
            UserInfoField plusAddressField = section.getPlusAddress();
            ChipView chip = view.getPlusAddress();
            chip.getPrimaryTextView().setText(plusAddressField.getDisplayText());
            chip.getPrimaryTextView().setContentDescription(plusAddressField.getA11yDescription());
            chip.setIcon(R.drawable.ic_plus_addresses_logo_24dp, /* tintWithTextColor= */ true);
            chip.setOnClickListener(src -> plusAddressField.triggerSelection());

            // Set the default icon, then try to get a better one.
            view.setIconForBitmap(mFaviconHelper.getDefaultIcon(section.getOrigin()));
            mFaviconHelper.fetchFavicon(section.getOrigin(), view::setIconForBitmap);
        }
    }

    /** Holds a View representing a set of address data. */
    static class AddressInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.UserInfo, AddressAccessoryInfoView> {
        AddressInfoViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_address_info);
        }

        @Override
        protected void bind(KeyboardAccessoryData.UserInfo info, AddressAccessoryInfoView view) {
            bindChipView(view.getNameFull(), info.getFields().get(0));
            bindChipView(view.getCompanyName(), info.getFields().get(1));
            bindChipView(view.getAddressHomeLine1(), info.getFields().get(2));
            bindChipView(view.getAddressHomeLine2(), info.getFields().get(3));
            bindChipView(view.getAddressHomeZip(), info.getFields().get(4));
            bindChipView(view.getAddressHomeCity(), info.getFields().get(5));
            bindChipView(view.getAddressHomeState(), info.getFields().get(6));
            bindChipView(view.getAddressHomeCountry(), info.getFields().get(7));
            bindChipView(view.getPhoneHomeWholeNumber(), info.getFields().get(8));
            bindChipView(view.getEmailAddress(), info.getFields().get(9));
        }

        void bindChipView(ChipView chip, UserInfoField field) {
            chip.getPrimaryTextView().setText(field.getDisplayText());
            chip.getPrimaryTextView().setContentDescription(field.getA11yDescription());
            if (!field.isSelectable() || field.getDisplayText().isEmpty()) {
                chip.setVisibility(View.GONE);
                return;
            }
            chip.setVisibility(View.VISIBLE);
            chip.setOnClickListener(src -> field.triggerSelection());
            chip.setClickable(true);
            chip.setEnabled(true);
        }
    }

    static void initializeView(
            RecyclerView view, AccessorySheetTabItemsModel model, FaviconHelper faviconHelper) {
        view.setAdapter(AddressAccessorySheetCoordinator.createAdapter(model, faviconHelper));
        view.addItemDecoration(new DynamicInfoViewBottomSpacer(AddressAccessoryInfoView.class));
    }
}
